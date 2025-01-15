#ifndef HEADERS_H
#define HEADERS_H

#include <argon2.h>
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libpq-fe.h>
#include <linux/limits.h>
#include <netinet/in.h>
#include <regex.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>

/*
+-----------------------------------------------------------------------------------+
|                                     defines                                       |
+-----------------------------------------------------------------------------------+
*/

#define N0 0
#define N1 1
#define N2 2
#define N3 3
#define N4 4
#define N5 5
#define N6 6
#define N7 7
#define N8 8
#define N9 9
#define N10 10

#define KB(value) ((value) * 1024)
#define PAGE_SIZE KB(4)

#define true N1
#define false N0

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif

#define CONNECTION_POOL_SIZE 5
#define MAX_CLIENT_CONNECTIONS 100 /** ?? */

#define BINARY N1
#define TEXT N0

#define MAX_EVENTS 10      /** ?? */
#define BLOCK_EXECUTION -1 /* In the context of epoll this puts the process to sleep. */

#define N1_PARAMS N1
#define N2_PARAMS N2
#define N3_PARAMS N3
#define N4_PARAMS N4
#define N5_PARAMS N5
#define N6_PARAMS N6
#define N7_PARAMS N7
#define N8_PARAMS N8
#define N9_PARAMS N9
#define N10_PARAMS N10

#define HASH_LENGTH 32
#define SALT_LENGTH 16
#define PASSWORD_BUFFER 255 /** Do I need this ??? */

#define URL(path) path "\x20"
#define URL_WITH_QUERY(path) path "?"

#define EMAIL_REGEX "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"

#define MAX_COMPONENT_NAME_LENGTH 100

#define COMPONENT_DEFINITION_OPENING_TAG__START "<x-component-def "
#define COMPONENT_DEFINITION_OPENING_TAG__END "\">"
#define COMPONENT_IMPORT_OPENING_TAG__START "<x-component "
#define OPENING_COMPONENT_IMPORT_TAG_SELF_CLOSING_END " />"
#define COMPONENT_IMPORT_OPENING_TAG__END "\">"
#define COMPONENT_DEFINITION_CLOSING_TAG "</x-component-def>"
#define COMPONENT_IMPORT_CLOSING_TAG "</x-component>"
#define INSERT_OPENING_TAG "<x-insert>"
#define INSERT_CLOSING_TAG "</x-insert>"
#define SLOT_MARK "%x-slot%"
#define SELF_CLOSING_TAG "/>"
#define CLOSING_BRACKET ">"

#define FOR_OPENING_TAG__START "<x-for name=\""
#define FOR_OPENING_TAG__END "\">"
#define FOR_CLOSING_TAG "</x-for>"

#define VAL_OPENING_TAG__START "<x-val name=\""
#define VAL_SELF_CLOSING_TAG__END "\" />"

#define NAME_ATTRIBUTE_PATTERN "\\s+name\\s*=\\s*\\\"[^\\\"]*\\\""
#define ATTRIBUTE_KEY_PATTERN "^[[:space:]]*([[:alnum:]_-]+)[[:space:]]*=" /** C regex compatible */
#define ATTRIBUTE_VALUE_PATTERN "\\\"[^\\\"]*\\\""
#define ATTRIBUTE_PATTERN "\\s\\w+(?:[-]\\w+)*\\s*= *\"[^\"]*\""
#define ATTRIBUTE_PATTERN_2 "[ \t]\\w+(-\\w+)*[ \t]*=[ \t]*\"[^\"]*\""
#define INJECT_ATTRIBUTE_PATTERN "\\sinject:\\w+(?:[-]\\w+)*\\s*= *\"[^\"]*\""
#define INHERIT_ATTRIBUTE_PATTERN "\\sinherit:\\w+(?:[-]\\w+)*"
#define SELF_CLOSING_TAG_PATTERN "/>"
#define COMPONENT_IMPORT_CLOSING_TAG_PATTERN "<\\/x-component\\s*>"
#define PLACEHOLDER_PATTERN "%[a-zA-Z0-9_-]+%"
#define COMPONENT_IMPORT_TAG_PATTERN "<x-component "
#define SLOT_TAG_PATTERN "<x-slot "
#define INSERT_TAG_PATTERN "<x-insert "

#define MAX_PATH_LENGTH 300
#define MAX_FILES 20

/*
+-----------------------------------------------------------------------------------+
|                                     structs                                       |
+-----------------------------------------------------------------------------------+
*/

typedef char *ValidationError;
typedef int boolean;

typedef struct {
    char *start_addr;
    char *end_addr;
} CharsBlock;

typedef CharsBlock Dict;        /** { 'k', 'e', 'y', '\0', 'v', 'a', 'l', 'u', 'e', '\0' ... } */
typedef CharsBlock StringArray; /** { 'm', 'o', 'r', 'n', 'i', 'n', 'g', '\0', 'b', 'u', 'e', 'n', 'o', 's', ' ', 'd', 'i', 'a', 's', '\0' ...} */

typedef struct {
    char *start_addr;
    size_t length;
} String;

typedef struct {
    String k;
    String v;
} RO_KV; /** Read only key value */

typedef struct {
    size_t size;
    void *start;
    void *current;
} Arena;

typedef struct {
    Arena *request_arena;
    int client_socket;
    char *request;
} RequestCtx;

typedef struct {
    int fd;
    RequestCtx *request_ctx;
    jmp_buf jmp_buf;
    uint8_t queued;
} Client;

typedef enum { SERVER_SOCKET, CLIENT_SOCKET, DB_SOCKET } FDType;

typedef struct {
    FDType type; /** This need to be the first element in the struct */
    uint8_t index;
    PGconn *conn;
    Client client;
} DBConnection;

typedef struct {
    PGresult *result;
    DBConnection *connection;
} DBQueryCtx;

typedef struct {
    uint8_t index;
    Client client;
} QueuedRequest;

typedef struct {
    FDType type; /** This need to be the first element in the struct */
    int fd;
} Socket;

typedef struct {
    Socket *socket;
    Dict public_files_dict;
    Dict templates;
} ArenaDataLookup;

typedef char uuid_str_t[37];

typedef CharsBlock TagLocation;

typedef struct {
    TagLocation opening_tag;
    TagLocation closing_tag;
} BlockLocation;

typedef struct {
    char *k;
    char *v;
} KV;

typedef struct {
    String block;
    String opening_tag;
} HTMLBlock;

/*
+-----------------------------------------------------------------------------------+
|                               function declaration                                |
+-----------------------------------------------------------------------------------+
*/

/** main.c */

Socket *create_server_socket(uint16_t port);
void sigint_handler(int signo);

/** arena.c */

Arena *arena_init(size_t size);
void *arena_alloc(Arena *arena, size_t size);
void arena_free(Arena *arena);
void arena_reset(Arena *arena, size_t arena_header_size);

/** template_engine.c */

Dict load_public_files(Arena *arena, const char *base_path);
Dict load_html_components(Arena *arena, const char *base_path);
Dict load_templates(Arena *arena, const char *base_path);
BlockLocation find_block(char *template, char *block_name);
size_t render_val(char *template, char *val_name, char *value);
size_t render_for(char *template, char *scope, int times, ...);
size_t replace_val(char *template, char *value_name, char *value);
size_t html_minify(char *buffer, char *html, size_t html_length);
String find_tag_name(char *import_statement);
char *find_component_declaration(char *string);
StringArray get_files_list(Arena *arena, const char *base_path, const char *extension);
HTMLBlock find_html_block(char *text, size_t text_length, const char *tag_name);
String find_attribute(String opening_tag, const char *attr_name);

/** connection.c */

void create_connection_pool(Dict envs);
DBConnection *get_available_connection(Arena *arena);
PGresult *WPQsendQueryParams(DBConnection *connection, const char *command, int nParams, const Oid *paramTypes, const char *const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat);
PGresult *get_result(DBConnection *connection);
void print_query_result(PGresult *query_result);

/** routes.c */

void router(RequestCtx request_ctx);
void public_get(RequestCtx request_ctx, String url);
void view_get(RequestCtx request_ctx, char *view, boolean accepts_query_params);
void test_get(RequestCtx request_ctx);
void home_get(RequestCtx request_ctx);
void register_create_account_post(RequestCtx request_ctx);
void login_create_session_post(RequestCtx request_ctx);
void logout_post(RequestCtx request_ctx);
void auth_check_email_post(RequestCtx request_ctx);
void account_get(RequestCtx request_ctx);
void request_cleanup(Arena *arena, DBConnection *connection, int client_socket);
Dict is_authenticated(RequestCtx request_ctx, DBConnection *connection);

ValidationError validate_email(Arena *arena, const char *email);
ValidationError validate_password(Arena *arena, const char *password);
ValidationError validate_repeat_password(Arena *arena, const char *password, const char *repeat_password);
ValidationError validate_accept_terms(Arena *arena, const char *accept_terms);

/** routes_utils.c */

String find_http_request_value(const char key[], char *request);
String find_body(const char *request);
String find_body_value(const char key[], String body);
char *file_content_type(Arena *arena, const char *path);
char char_to_hex(unsigned char nibble); /** TODO: Review this function */
char hex_to_char(unsigned char c);      /** TODO: Review this function */
size_t url_encode_utf8(char **string, size_t length);
size_t url_decode_utf8(char **string, size_t length);
Dict parse_and_decode_params(Arena *arena, String raw_query_params);
String find_cookie_value(const char *key, String cookies);

/** utils.c */

Dict load_env_variables(const char *filepath);
void read_file(char **buffer, long *file_size, const char *absolute_file_path);
char *locate_files(char *buffer, const char *base_path, const char *extension, uint8_t level, size_t *all_paths_length);
char *find_value(const char key[], Dict dict);
int generate_salt(uint8_t *salt, size_t salt_size);
uint8_t get_dictionary_size(Dict dict);
uint8_t get_string_array_length(StringArray array);
char *get_string_at(StringArray array, uint8_t pos);
void replace_slashes(char *str);
void dump_dict(Dict dict, char folder_name[]);
KV get_key_value(Dict dict, uint8_t pos);
char *copy_string(Arena *arena, String str);
Dict add_to_dictionary(const char *buffer, int key_value_pairs, ...);

/*
+-----------------------------------------------------------------------------------+
|                                     globals                                       |
+-----------------------------------------------------------------------------------+
*/

extern DBConnection connection_pool[CONNECTION_POOL_SIZE];
extern QueuedRequest queue[MAX_CLIENT_CONNECTIONS];

extern Arena *global_arena;
extern ArenaDataLookup *global_arena_data;

extern Arena *scratch_arena;

extern int epoll_fd;
extern int nfds;
extern struct epoll_event events[MAX_EVENTS];
extern struct epoll_event event;

extern jmp_buf ctx;
extern jmp_buf db_ctx;

extern volatile sig_atomic_t keep_running;

extern boolean dev_mode;

#endif
