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
#include "../lib/profiler/profiler.h"
#include "../app/shared.h"
#include "../db/db.h"
#include "../lib/memory/memory.h"
#include "../app/entry.h"
#include "../lib/utils/utils.h"
#include "../lib/json/json_parser.h"
#include "../lib/http/http.h"
/* clang-format on */

#define ASSETS_FULLPATH "/home/roy/repositories/littlechef/assets"
#define PORT 8080
#define DB_NAME "littlechef-dev.db"

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif

#define MAX_QUEUED 50 /** ?? */
#define MAX_PATH_LENGTH 512

#define BLOCK_EXECUTION -1 /* In the context of epoll this puts the process to sleep. */

typedef struct {
    enum { SERVER_SOCKET, CLIENT_SOCKET } type;
    int fd;
} Socket;

volatile sig_atomic_t keep_running = true;

Memory *initialise_memory(size_t size) {
    void *raw_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Memory *memory = memory_setup(raw_memory, size);

    return memory;
}

#define MAX_ASSET_FILES 72
void locate_files(Memory *memory, String *filepaths, char base_path[], const char *root_path, u32 *count, u32 count_limit) {
    DIR *dir = opendir(base_path);
    ASSERT(dir != NULL);

    struct dirent *entry = {0};
    struct stat statbuf = {0};
    char path_buffer[MAX_PATH_LENGTH];
    memset(path_buffer, 0, MAX_PATH_LENGTH);

    while ((entry = readdir(dir)) != NULL) {
        boolean dot_dir = (boolean)(strcmp(entry->d_name, ".") == 0);
        boolean dot_dot_dir = (boolean)(strcmp(entry->d_name, "..") == 0);

        if (dot_dir || dot_dot_dir) {
            continue;
        }

        sprintf(path_buffer, "%s/%s", base_path, entry->d_name);

        boolean is_dir = (boolean)(stat(path_buffer, &statbuf) == 0 && S_ISDIR(statbuf.st_mode));

        if (is_dir) {
            locate_files(memory, filepaths, path_buffer, root_path, count, count_limit);
        } else {
            if (*count > count_limit) {
                printf("%s dir contains more than %d (count_limit)\n", root_path, count_limit);
                ASSERT(0);
            }

            size_t filepath_length = strlen(path_buffer);

            // NOTE: Although the filepath is of type String, it must be null-terminated.
            // This is because it will be passed to fopen, which requires a null-terminated
            // character array.
            char *filepath = memory_alloc(memory, filepath_length /* +1 for null terminator */ + 1);
            memcpy(filepath, path_buffer, filepath_length);

            filepaths[*count].data = filepath;
            filepaths[*count].length = filepath_length;

            (*count)++;
        }
    }

    closedir(dir);
}

void initialise_web_server_resources(Memory *memory) {
    // All asset files (e.g., HTML, JS, etc.) located at ASSETS_FULLPATH
    // are provided in a temporary buffer to the application through
    // the setup_web_server_resources function. This function typicall,
    // processes these files into a global memory block that persists
    // throughout the web server's lifetime.

    // Temporary memory buffer from which the application will retrieve
    // the assets from.
    Memory *assets_memory = initialise_memory(PAGE_SIZE * 50);

    String *filepaths = memory_alloc(assets_memory, sizeof(String) * MAX_ASSET_FILES);
    u32 count = 0;
    locate_files(assets_memory, filepaths, ASSETS_FULLPATH, ASSETS_FULLPATH, &count, MAX_ASSET_FILES);

    String *contents = memory_alloc(assets_memory, sizeof(String) * count);

    u32 i = 0;
    while (i < count) {
        long file_size = 0;

        FILE *file = fopen(filepaths[i].data, "r");
        ASSERT(file != NULL);
        ASSERT(fseek(file, 0, SEEK_END) != -1);
        file_size = ftell(file);
        ASSERT(file_size != -1);
        rewind(file);

        char *asset_file_content = memory_alloc(assets_memory, file_size);
        size_t read_size = fread(asset_file_content, sizeof(char), file_size, file);
        ASSERT(read_size == (size_t)file_size);
        fclose(file);

        contents[i].data = asset_file_content;
        contents[i].length = file_size;

        i++;
    }

    AssetSOA assets_soa = {0};
    assets_soa.locations = filepaths;
    assets_soa.contents = contents;
    assets_soa.count = count;

    Memory *scratch_memory = initialise_memory(PAGE_SIZE * 50);
    setup_web_server_resources(memory, scratch_memory, assets_soa);

    munmap(assets_memory->start, assets_memory->size);
    munmap(scratch_memory->start, scratch_memory->size);
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

    char memory_dir[] = "/__memory__";

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

void test_json(char *base_path) {
    Memory *memory = initialise_memory(PAGE_SIZE * 800);

    String *filepaths = memory_alloc(memory, sizeof(String) * 350);
    u32 count = 0;
    locate_files(memory, filepaths, base_path, base_path, &count, 350);

    String *json_tests = memory_alloc(memory, sizeof(String) * count);

    u32 f = 0;
    while (f < count) {
        long file_size = 0;

        FILE *file = fopen(filepaths[f].data, "r");
        ASSERT(file != NULL);
        ASSERT(fseek(file, 0, SEEK_END) != -1);
        file_size = ftell(file);
        ASSERT(file_size != -1);
        rewind(file);

        char *asset_file_content = memory_alloc(memory, file_size);
        size_t read_size = fread(asset_file_content, sizeof(char), file_size, file);
        ASSERT(read_size == (size_t)file_size);
        fclose(file);

        json_tests[f].data = asset_file_content;
        json_tests[f].length = file_size;

        f++;
    }

    AssetSOA json_assets_soa = {0};
    json_assets_soa.locations = filepaths;
    json_assets_soa.contents = json_tests;
    json_assets_soa.count = count;

    for (f = 0; f < json_assets_soa.count; f++) {
        // json parser works for file test_input.json
        // try to make the parser pass for json files in folder json_processor_complience_test_cases/
        JSONElement *json = json_parse(memory, json_assets_soa.contents[f]);

        printf("\n");
    }
}

int main() {
    // test_json("/home/roy/repositories/littlechef/json_processor_test_cases");

    struct HttpMessage msg;
    char buf[1000] = "GET / HTTP/1.1\r\nHost: 127.0.0.1:8080\r\nConnection: keep-alive\r\nCache-Control: max-age=0\r\nsec-ch-ua: \"Google Chrome\";v=\"137\", \"Chromium\";v=\"137\", \"Not/A)Brand\";v=\"24\"\r\nsec-ch-ua-mobile: ?0\r\nsec-ch-ua-platform: \"macOS\"\r\nUpgrade-Insecure-Requests: 1\r\nUser-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/137.0.0.0 Safari/537.36\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7\r\nSec-Fetch-Site: none\r\nSec-Fetch-Mode: navigate\r\nSec-Fetch-User: ?1\r\nSec-Fetch-Dest: document\r\nAccept-Encoding: gzip, deflate, br, zstd\r\nAccept-Language: en-GB,en-US;q=0.9,en;q=0.8\r\n\r\n";

    String raw_http_msg = {0};
    raw_http_msg.data = buf;
    raw_http_msg.length = strlen(buf);

    init_http_message(&msg, HttpRequest);
    parse_http_message(&msg, raw_http_msg, sizeof(buf));

    int i;

    int epoll_fd;
    int nfds;
    struct epoll_event events[MAX_QUEUED];
    struct epoll_event event;

    /* Registers a signal handler to ensure the program exits gracefully */
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        printf("Failed to set up signal handler for SIGINT\nError code: %d\n", errno);
        ASSERT(0);
    }

    Memory *persisting_memory = initialise_memory(PAGE_SIZE * 200);

    initialise_web_server_resources(persisting_memory);

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

#if DEBUG
                        memory_reset(persisting_memory, (u8 *)persisting_memory->start + sizeof(Memory));
                        initialise_web_server_resources(persisting_memory);
#endif

                        Memory *request_memory = initialise_memory(PAGE_SIZE * 100);

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

                        Memory *request_memory = (Memory *)((u8 *)socket_info - sizeof(Memory));
                        RequestCtx *request_ctx = (RequestCtx *)((u8 *)request_memory + (sizeof(Memory) + sizeof(Socket)));

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

                            /*
                            ASSERT(data_size > 0);
                            */

                            read_stream += data_size;

                            ASSERT(read_stream < max_buffer_size);
                        }

                        memory_out_of_use(request_memory, p);

                        Response response = {0};
                        /* begin_profile(); */
                        response = process_request_and_render_response(*request_ctx);
                        /* end_and_print_profile(); */

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