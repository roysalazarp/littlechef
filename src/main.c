#include "headers.h"

Socket *create_server_socket(uint16_t port);
void sigint_handler(int signo);

Arena *global_arena;
ArenaDataLookup *global_arena_data;

Arena *scratch_arena;

int epoll_fd;
int nfds;
struct epoll_event events[MAX_EVENTS];
struct epoll_event event;

volatile sig_atomic_t keep_running = 1;

#ifdef DEV
boolean dev_mode = true;
#else
boolean dev_mode = false;
#endif

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

    scratch_arena = arena_init(PAGE_SIZE * 10);

    global_arena = arena_init(PAGE_SIZE * 100);

    /** To look up data stored in arena */
    global_arena_data = (ArenaDataLookup *)arena_alloc(global_arena, sizeof(ArenaDataLookup));

    Dict envs = {0};
    if (dev_mode) {
        envs = load_env_variables(global_arena, "./.env.dev");
    } else {
        envs = load_env_variables(global_arena, "./.env.prod");

        const char *public_base_path = find_value("CMPL__PUBLIC_FOLDER", envs);
        Dict public_files = load_public_files(global_arena, public_base_path);
        global_arena_data->public_files_dict = public_files;

        const char *html_base_path = find_value("CMPL__TEMPLATES_FOLDER", envs);
        Dict templates = load_templates(global_arena, html_base_path);
        global_arena_data->templates = templates;
    }

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

    struct sockaddr_in client_addr; /** Why is this needed ?? */
    socklen_t client_addr_len = sizeof(client_addr);

    char *arena_freeze_ptr = NULL;
    if (dev_mode) {
        arena_freeze_ptr = (char *)global_arena->current;
    }

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

                        if (dev_mode) {
                            /** Reload html and static files into memory */
                            arena_reset2(global_arena, (uint8_t *)arena_freeze_ptr);

                            const char *public_base_path = find_value("CMPL__PUBLIC_FOLDER", envs);
                            Dict public_files = load_public_files(global_arena, public_base_path);
                            global_arena_data->public_files_dict = public_files;

                            const char *html_base_path = find_value("CMPL__TEMPLATES_FOLDER", envs);
                            Dict templates = load_templates(global_arena, html_base_path);
                            global_arena_data->templates = templates;
                        }

                        /** Allocate memory for handling client request */
                        Arena *request_arena = arena_init(PAGE_SIZE * 30);

                        Socket *client_socket_info = (Socket *)arena_alloc(request_arena, sizeof(Socket));
                        client_socket_info->type = CLIENT_SOCKET;
                        client_socket_info->fd = client_fd;

                        RequestCtx *request_ctx = (RequestCtx *)arena_alloc(request_arena, sizeof(RequestCtx));
                        request_ctx->request_arena = request_arena;
                        request_ctx->client_socket = client_fd;

                        const char *db_name = find_value("DB", envs);
                        assert(sqlite3_open(db_name, &request_ctx->db) == 0);

                        event.events = EPOLLIN | EPOLLET;
                        event.data.ptr = client_socket_info;
                        assert(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) != -1);

                        break;
                    }

                    printf("Server socket only should receive EPOLLIN events\n");
                    assert(0);

                    break;
                }

                case CLIENT_SOCKET: {
                    if (events[i].events & EPOLLIN) { /** Data available for read in client socket buffer */
                        Arena *request_arena = (Arena *)((uint8_t *)socket_info - sizeof(Arena));
                        RequestCtx *request_ctx = (RequestCtx *)((uint8_t *)request_arena + (sizeof(Arena) + sizeof(Socket)));

                        router(*request_ctx);

                        close(request_ctx->client_socket);
                        sqlite3_close(request_ctx->db);
                        arena_free(request_arena);

                        break;
                    }

                    printf("Client socket only should receive EPOLLIN events\n");
                    assert(0);

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

    arena_free(scratch_arena);
    arena_free(global_arena);

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

    Socket *server_socket = arena_alloc(global_arena, sizeof(Socket));
    server_socket->fd = server_fd;
    server_socket->type = SERVER_SOCKET;

    global_arena_data->socket = server_socket;

    return global_arena_data->socket;
}

void sigint_handler(int signo) {
    if (signo == SIGINT) {
        printf("\nReceived SIGINT, exiting program...\n");
        keep_running = 0;
    }
}
