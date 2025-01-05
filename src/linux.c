#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* clang-format off */
#include "./app/shared.h"
#include "./db.h"
#include "./app/memory.h"
#include "./app/entry.h"
#include "./app/utils.h"
/* clang-format on */

#define ASSETS_PATH "assets" /** From project root dir */
#define PORT 8080
#define DB_NAME "littlechef-dev.db"

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif

#define MAX_QUEUED 50 /** ?? */
#define MAX_PATH_LENGTH 300

#define BLOCK_EXECUTION -1 /* In the context of epoll this puts the process to sleep. */

typedef struct {
    enum { SERVER_SOCKET, CLIENT_SOCKET } type;
    int fd;
} Socket;

int epoll_fd;
int nfds;
struct epoll_event events[MAX_QUEUED];
struct epoll_event event;

volatile sig_atomic_t keep_running = true;

Memory *initialise_memory(size_t size) {
    void *raw_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Memory *memory = memory_setup(raw_memory, size);

    return memory;
}

void locate_files(char **buffer, const char *base_path) {
    DIR *dir = opendir(base_path);
    ASSERT(dir != NULL);

    struct dirent *entry = {0};
    struct stat statbuf = {0};
    char path[MAX_PATH_LENGTH];
    memset(path, 0, MAX_PATH_LENGTH);
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            sprintf(path, "%s/%s", base_path, entry->d_name);

            if (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
                locate_files(buffer, path);
            } else {
                size_t path_len = strlen(path);
                strcpy(*buffer, path);
                *buffer += strlen(*buffer) + 1;
            }
        }
    }

    closedir(dir);
}

void initialise_web_server_resources(Memory *memory) {
    char *p = NULL;
    uint8_t i;

    Memory *assets_memory = initialise_memory(PAGE_SIZE * 50);
    Memory *assets_scratch_memory = initialise_memory(PAGE_SIZE * 50);

    char *base_path = NULL;
    base_path = p = (char *)memory_in_use(assets_memory);
    ASSERT(getcwd(p, PATH_MAX) != NULL);
    p += strlen(p);
    *p = '/';
    p++;
    memcpy(p, ASSETS_PATH, strlen(ASSETS_PATH));
    p += strlen(p) + 1;
    memory_out_of_use(assets_memory, p);

    StringArray files_list = {0};
    files_list.start_addr = p = (char *)memory_in_use(assets_memory);
    locate_files(&p, base_path);
    memory_out_of_use(assets_memory, p);
    files_list.end_addr = p;

    uint8_t files_list_length = get_string_array_length(files_list);

    Dict assets = {0};
    assets.start_addr = p = (char *)memory_in_use(assets_memory);
    for (i = 0; i < files_list_length; i++) {
        char *path = get_string_at(files_list, i);

        memcpy(p, path, strlen(path));
        p += strlen(p) + 1;

        long file_size = 0;

        FILE *file = fopen(path, "r");
        ASSERT(file != NULL);
        ASSERT(fseek(file, 0, SEEK_END) != -1);
        file_size = ftell(file);
        ASSERT(file_size != -1);
        rewind(file);

        size_t read_size = fread(p, sizeof(char), file_size, file);
        ASSERT(read_size == (size_t)file_size);

        fclose(file);

        p += strlen(p) + 1;
    }
    assets.end_addr = p;
    memory_out_of_use(assets_memory, p);

    setup_web_server_resources(memory, assets_scratch_memory, assets);

    munmap(assets_memory->start, assets_memory->size);
    munmap(assets_scratch_memory->start, assets_scratch_memory->size);
}

void sigint_handler(int signo) {
    if (signo == SIGINT) {
        printf("\nReceived SIGINT, exiting program...\n");
        keep_running = false;
    }
}

void dump_dict(Dict dict, char dir_name[]) {
    char cwd[KB(1)];
    memset(cwd, 0, KB(1));
    ASSERT(getcwd(cwd, sizeof(cwd)) != NULL);

    char memory_dir[] = "/memory";

    ASSERT((strlen(cwd) + strlen(memory_dir)) < KB(1));

    memcpy(&(cwd[strlen(cwd)]), memory_dir, strlen(memory_dir));

    /* Check if the directory exists */
    if (access(cwd, F_OK) == -1) {
        /* Directory doesn't exist, so create it */
        ASSERT(mkdir(cwd, 0755) == 0);
    }

    char slash[] = "/";
    ASSERT((strlen(cwd) + strlen(slash) + strlen(dir_name)) < KB(1));

    memcpy(&(cwd[strlen(cwd)]), slash, strlen(slash));
    memcpy(&(cwd[strlen(cwd)]), dir_name, strlen(dir_name));

    char command[KB(2)];
    sprintf(command, "rm -rf %s", cwd);
    ASSERT(system(command) == 0);
    ASSERT(mkdir(cwd, 0755) == 0);

    char *ptr = dict.start_addr;
    while (ptr < dict.end_addr) {
        char *key = ptr;

        char file_name[KB(2)];
        memset(file_name, 0, KB(2));
        sprintf(file_name, "%s/%s", cwd, key);

        char *p = file_name + strlen(cwd) + strlen(slash);
        while (*p != '\0') {
            if (*p == '/') {
                *p = '\\'; /* Replace '/' with '\' */
            }
            p++;
        }

        FILE *file = fopen(file_name, "w");
        ASSERT(file);

        ptr += strlen(ptr) + 1;
        char *value = ptr;

        fprintf(file, "%s", value);
        ptr += strlen(ptr) + 1;

        fclose(file);
    }
}

int generate_salt(void *salt, size_t salt_size) {
    FILE *dev_urandom = fopen("/dev/urandom", "rb");

    int output = 0;
    if (fread(salt, 1, salt_size, dev_urandom) != salt_size) {
        output = -1;
    }

    fclose(dev_urandom);
    return output;
}

int main() {
    int i;

    /* Registers a signal handler to ensure the program exits gracefully */
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        printf("Failed to set up signal handler for SIGINT\nError code: %d\n", errno);
        ASSERT(0);
    }

    Memory *persisting_memory = initialise_memory(PAGE_SIZE * 80);

#ifndef DEBUG
    initialise_web_server_resources(persisting_memory);
#endif

    epoll_fd = epoll_create1(0);
    ASSERT(epoll_fd != -1);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT(server_fd != -1);

    int server_fd_flags = fcntl(server_fd, F_GETFL, 0);
    ASSERT(fcntl(server_fd, F_SETFL, server_fd_flags | O_NONBLOCK) != -1);

    int server_fd_optname = 1;
    ASSERT(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &server_fd_optname, sizeof(int)) != -1);

    /** Configure server address */
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;             /** IPv4 */
    server_addr.sin_port = htons((uint16_t)PORT); /** Convert the port number from host byte order to network byte order (big-endian) */
    server_addr.sin_addr.s_addr = INADDR_ANY;     /** Listen on all available network interfaces (IPv4 addresses) */

    ASSERT(bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != -1);

    ASSERT(listen(server_fd, MAX_QUEUED) != -1);

    Socket server_socket = {0};
    server_socket.fd = server_fd;
    server_socket.type = SERVER_SOCKET;

    event.events = EPOLLIN;
    event.data.ptr = &server_socket;
    ASSERT(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket.fd, &event) != -1);

    printf("Server listening on port: %d...\n", (int)PORT);

    struct sockaddr_in client_addr; /** Why is this needed ?? */
    socklen_t client_addr_len = sizeof(client_addr);

    while (true) {
        nfds = epoll_wait(epoll_fd, events, MAX_QUEUED, BLOCK_EXECUTION);

        if (keep_running == false) {
            break;
        }

        ASSERT(nfds != -1);

        for (i = 0; i < nfds; i++) {
            Socket *socket_info = (Socket *)events[i].data.ptr;

            switch (socket_info->type) {
                case SERVER_SOCKET: {
                    if (events[i].events & EPOLLIN) { /** Server received new client request */
                        int client_fd = accept(socket_info->fd, (struct sockaddr *)&client_addr, &client_addr_len);
                        ASSERT(client_fd != -1);

                        int client_fd_flags = fcntl(client_fd, F_GETFL, 0);
                        ASSERT(fcntl(client_fd, F_SETFL, client_fd_flags | O_NONBLOCK) != -1);

#ifdef DEBUG
                        memory_reset(persisting_memory, (uint8_t *)persisting_memory->start + sizeof(Memory));
                        initialise_web_server_resources(persisting_memory);
#endif

                        Memory *request_memory = initialise_memory(PAGE_SIZE * 50);

                        Socket *client_socket_info = (Socket *)memory_alloc(request_memory, sizeof(Socket));
                        client_socket_info->type = CLIENT_SOCKET;
                        client_socket_info->fd = client_fd;

                        RequestCtx *request_ctx = (RequestCtx *)memory_alloc(request_memory, sizeof(RequestCtx));
                        request_ctx->persisting_memory = persisting_memory;
                        request_ctx->request_memory = request_memory;
                        request_ctx->query = query;

                        sqlite3 *pdb = NULL;
                        ASSERT(sqlite3_open(DB_NAME, &pdb) == 0);

                        request_ctx->db = (void *)pdb;

                        event.events = EPOLLIN | EPOLLET;
                        event.data.ptr = client_socket_info;
                        ASSERT(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) != -1);

                        break;
                    }

                    printf("Server socket only should receive EPOLLIN events\n");
                    ASSERT(0);

                    break;
                }

                case CLIENT_SOCKET: {
                    if (events[i].events & EPOLLIN) { /** Data available for read in client socket buffer */
                        char *p = NULL;

                        Memory *request_memory = (Memory *)((uint8_t *)socket_info - sizeof(Memory));
                        RequestCtx *request_ctx = (RequestCtx *)((uint8_t *)request_memory + (sizeof(Memory) + sizeof(Socket)));

                        int client_socket = socket_info->fd;

                        ssize_t read_stream = 0;
                        ssize_t max_buffer_size = KB(10);

                        request_ctx->request = p = (char *)memory_in_use(request_memory);

                        while (1) {
                            ssize_t data_size = recv(client_socket, p, max_buffer_size - read_stream, 0);
                            p += read_stream;

                            if (data_size == -1) {
                                if (errno == EAGAIN || errno == EWOULDBLOCK) { /** No more data available for read in socket buffer */
                                    /** TODO: Detecting the end of an HTTP message properly */
                                    /**
                                     * The following code incorrectly assumes that once all available data has been read from the socket buffer,
                                     * the entire HTTP message has been received. This is a flawed implementation because the client may send
                                     * a very large request that does not fit entirely in the buffer at once.
                                     *
                                     * In such cases, we need to return to the event loop and wait for a notification when more data is available
                                     * for reading **and ensure the complete message is received.**
                                     */
                                    if (read_stream > 0) {
                                        (*p) = '\0';
                                        p++;
                                        break;
                                    }

                                    ASSERT(0);
                                }

                                printf("recv error\n");
                                ASSERT(0);
                            }

                            if (data_size == 0 && read_stream == 0) {
                                goto request_cleanup;
                            }

                            ASSERT(data_size > 0); /** ??? */

                            read_stream += data_size;

                            ASSERT(read_stream < max_buffer_size);
                        }

                        memory_out_of_use(request_memory, p);

                        Response response = process_request_and_render_response(*request_ctx);

                        if (send(client_socket, response.content, response.length, 0) == -1) {
                            /* TODO */
                        }

                    request_cleanup:
                        close(client_socket);
                        sqlite3_close(request_ctx->db);

                        munmap(request_ctx->request_memory->start, request_ctx->request_memory->size);

                        break;
                    }

                    printf("Client socket only should receive EPOLLIN events\n");
                    ASSERT(0);

                    break;
                }

                default: {
                    ASSERT(0);

                    break;
                }
            }
        }
    }

    close(server_socket.fd);
    munmap(persisting_memory->start, persisting_memory->size);

    return 0;
}