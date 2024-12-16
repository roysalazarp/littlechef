#include "headers.h"

DBConnection connection_pool[CONNECTION_POOL_SIZE];
QueuedRequest queue[MAX_CLIENT_CONNECTIONS];

Arena *arena;
ArenaDataLookup *arena_data;

int epoll_fd;
int nfds;
struct epoll_event events[MAX_EVENTS];
struct epoll_event event;

jmp_buf ctx;
jmp_buf db_ctx;

volatile sig_atomic_t keep_running = 1;

int main() {
    int i;

    /**
     * Registers a signal handler to ensure the program exits gracefully.
     * This allows Valgrind to generate a complete memory report upon termination.
     */
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        fprintf(stderr, "Failed to set up signal handler for SIGINT\nError code: %d\n", errno);
        assert(0);
    }

    arena = arena_init(PAGE_SIZE * 50);

    /** To look up data stored in arena */
    arena_data = (ArenaDataLookup *)arena_alloc(arena, sizeof(ArenaDataLookup));

    Dict envs = load_env_variables(ENV_FILE_PATH);

    const char *public_base_path = find_value("CMPL__PUBLIC_FOLDER", envs);
    load_public_files(public_base_path);

    const char *html_base_path = find_value("CMPL__TEMPLATES_FOLDER", envs);
    load_templates(html_base_path);

    epoll_fd = epoll_create1(0);
    assert(epoll_fd != -1);

    const char *port_str = find_value("PORT", envs);

    char *endptr;
    long port = strtol(port_str, &endptr, 10);

    if (*endptr != '\0' || port < 0 || port > 65535) {
        fprintf(stderr, "Invalid port number: %s\n", port_str);
        assert(0);
    }

    Socket *server_socket = create_server_socket((uint16_t)port);
    int server_fd = server_socket->fd;

    event.events = EPOLLIN;
    event.data.ptr = server_socket;
    assert(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) != -1);

    printf("Server listening on port: %d...\n", (int)port);

    create_connection_pool(envs);

    /** Clear envs for security */
    memset(envs.start_addr, 0, envs.end_addr - envs.start_addr);

    struct sockaddr_in client_addr; /** Why is this needed ?? */
    socklen_t client_addr_len = sizeof(client_addr);

    while (1) {
        nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, BLOCK_EXECUTION);

        if (keep_running == 0) {
            break;
        }

        assert(nfds != -1);

        for (i = 0; i < nfds; i++) {
            Socket *socket_info = (Socket *)events[i].data.ptr;

            switch (socket_info->type) {
                case SERVER_SOCKET: {
                    if (events[i].events & EPOLLIN) { /** Server received new client request */
                        int client_fd = accept(socket_info->fd, (struct sockaddr *)&client_addr, &client_addr_len);
                        assert(client_fd != -1);

                        int client_fd_flags = fcntl(client_fd, F_GETFL, 0);
                        assert(fcntl(client_fd, F_SETFL, client_fd_flags | O_NONBLOCK) != -1);

                        /** Allocate memory for handling client request */
                        Arena *scratch_arena = arena_init(PAGE_SIZE * 30);

                        Socket *client_socket_info = (Socket *)arena_alloc(scratch_arena, sizeof(Socket));
                        client_socket_info->type = CLIENT_SOCKET;
                        client_socket_info->fd = client_fd;

                        RequestCtx *request_ctx = (RequestCtx *)arena_alloc(scratch_arena, sizeof(RequestCtx));
                        request_ctx->scratch_arena = scratch_arena;
                        request_ctx->client_socket = client_fd;

                        event.events = EPOLLIN | EPOLLET;
                        event.data.ptr = client_socket_info;
                        assert(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) != -1);

                        break;
                    }

                    printf("Server socket only should receive EPOLLIN events\n");
                    assert(0);

                    break;
                }

                case CLIENT_SOCKET: { /** Client socket (aka. client request) ready for read */
                    if (events[i].events & EPOLLIN) {
                        Arena *scratch_arena = (Arena *)((uint8_t *)socket_info - sizeof(Arena));
                        RequestCtx *request_ctx = (RequestCtx *)((uint8_t *)scratch_arena + (sizeof(Arena) + sizeof(Socket)));

                        if (setjmp(ctx) == 0) {
                            router(*request_ctx);
                        }

                        break;
                    }

                    printf("Client socket only should receive EPOLLIN events\n");
                    assert(0);

                    break;
                }

                case DB_SOCKET: {
                    if (events[i].events & EPOLLIN) { /** DB socket (aka. connection) ready for read */
                        DBConnection *connection = (DBConnection *)socket_info;

                        /** The connection should belong to a client fd (aka. client request) */
                        assert(connection->client.fd != 0);

                        /** Postgres query response is ready for read, jump back to code and
                         * pass index to restore request state through connection pool */
                        longjmp(connection->client.jmp_buf, 1);
                    } else if (events[i].events & EPOLLOUT) { /** DB socket (aka connection) is ready for write */
                        DBConnection *connection = (DBConnection *)socket_info;

                        QueuedRequest *request = NULL;

                        int j;
                        for (j = 0; j < MAX_CLIENT_CONNECTIONS; j++) {
                            if (queue[j].client.fd != 0) {
                                /** First client request in the queue waiting for connection */
                                request = &(queue[j]);

                                break;
                            }
                        }

                        if (request) {
                            connection->client = request->client;
                            memcpy(&(connection->client), &(request->client), sizeof(Client));
                            memset(&(request->client), 0, sizeof(Client));

                            if (setjmp(db_ctx) == 0) {
                                /* Request handler queued the request because no connection was
                                 * available in the pool, here we jump back to code after queuing request */
                                longjmp(connection->client.jmp_buf, 1);
                            }
                        }
                    }

                    break;
                }

                default: {
                    assert(0);

                    break;
                }
            }
        }
    }

    close(server_fd);

    for (i = 0; i < CONNECTION_POOL_SIZE; i++) {
        PQfinish(connection_pool[i].conn);
    }

    arena_free(arena);

    return 0;
}

Socket *create_server_socket(uint16_t port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(server_fd != -1);

    int server_fd_flags = fcntl(server_fd, F_GETFL, 0);
    assert(fcntl(server_fd, F_SETFL, server_fd_flags | O_NONBLOCK) != -1);

    int server_fd_optname = 1;
    assert(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &server_fd_optname, sizeof(int)) != -1);

    /** Configure server address */
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;         /** IPv4 */
    server_addr.sin_port = htons(port);       /** Convert the port number from host byte order to network byte order (big-endian) */
    server_addr.sin_addr.s_addr = INADDR_ANY; /** Listen on all available network interfaces (IPv4 addresses) */

    assert(bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != -1);

    assert(listen(server_fd, MAX_CLIENT_CONNECTIONS) != -1);

    Socket *server_socket = arena_alloc(arena, sizeof(Socket));
    server_socket->fd = server_fd;
    server_socket->type = SERVER_SOCKET;

    arena_data->socket = server_socket;

    return arena_data->socket;
}

void sigint_handler(int signo) {
    if (signo == SIGINT) {
        printf("\nReceived SIGINT, exiting program...\n");
        keep_running = 0;
    }
}
