#include "headers.h"

void router(RequestCtx request_ctx) {
    Arena *request_arena = request_ctx.request_arena;
    int client_socket = request_ctx.client_socket;

    ssize_t read_stream = 0;
    ssize_t max_buffer_size = KB(10);

    char *request = NULL;
    char *ptr = NULL;
    ARENA_IN_USE(request_arena, request, ptr) {
        while (1) {
            ssize_t data_size = recv(client_socket, ptr, max_buffer_size - read_stream, 0);
            ptr += read_stream;

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
                        break;
                    }

                    assert(0);
                }

                printf("recv error\n");
                assert(0);
            }

            assert(data_size > 0);

            /**
             * NOTE: While it is possible to decode the entire HTTP request here at once,
             * we avoid doing so to prevent potential issues with requests containing
             * non-textual data, such as images or binary files. Processing such data
             * inappropriately could lead to corruption or unexpected behavior.
             */

            read_stream += data_size;

            assert(read_stream < max_buffer_size);
        }

        (*ptr) = '\0';
        ptr++;
    }

    request_ctx.request = request;

    String method = find_http_request_value("METHOD", request);
    String url = find_http_request_value("URL", request);

    if (strncmp(url.start_addr, URL("/.well-known/assetlinks.json"), strlen(URL("/.well-known/assetlinks.json"))) == 0 && strncmp(method.start_addr, "GET", method.length) == 0) {
        String path = {0};
        char *str = NULL;

        if (dev_mode) {
            str = "/public/android/assetlinks.dev.json";
        } else {
            str = "/public/android/assetlinks.json";
        }

        path.start_addr = str;
        path.length = strlen(str);

        public_get(request_ctx, path);
        return;
    }

    if (strncmp(url.start_addr, "/public", strlen("/public")) == 0 && strncmp(method.start_addr, "GET", method.length) == 0) {
        public_get(request_ctx, url);
        return;
    }

    if (strncmp(url.start_addr, URL("/"), strlen(URL("/"))) == 0) {
        if (strncmp(method.start_addr, "GET", method.length) == 0) {
            home_get(request_ctx);
            return;
        }
    }

    if (strncmp(url.start_addr, URL("/account"), strlen(URL("/profile"))) == 0) {
        if (strncmp(method.start_addr, "GET", method.length) == 0) {
            account_get(request_ctx);
            return;
        }
    }

    if (strncmp(url.start_addr, URL("/auth-check-email"), strlen(URL("/auth-check-email"))) == 0) {
        if (strncmp(method.start_addr, "POST", method.length) == 0) {
            auth_check_email_post(request_ctx);
            return;
        }
    }

    /*
    if (strncmp(url.start_addr, URL("/register/create-account"), strlen(URL("/register/create-account"))) == 0) {
        if (strncmp(method.start_addr, "POST", method.length) == 0) {
            register_create_account_post(request_ctx);
            return;
        }
    }

    if (strncmp(url.start_addr, URL("/login/create-session"), strlen(URL("/login/create-session"))) == 0) {
        if (strncmp(method.start_addr, "POST", method.length) == 0) {
            login_create_session_post(request_ctx);
            return;
        }
    }

    if (strncmp(url.start_addr, URL("/logout"), strlen(URL("/logout"))) == 0) {
        if (strncmp(method.start_addr, "POST", method.length) == 0) {
            logout_post(request_ctx);
            return;
        }
    }

    if (strncmp(url.start_addr, URL("/test"), strlen(URL("/test"))) == 0) {
        if (strncmp(method.start_addr, "GET", method.length) == 0) {
            test_get(request_ctx);
            return;
        }
    }

    if (strncmp(url.start_addr, URL("/ui-test"), strlen(URL("/ui-test"))) == 0) {
        if (strncmp(method.start_addr, "GET", method.length) == 0) {
            view_get(request_ctx, "ui_test", false);
            return;
        }
    }
    */

    view_get(request_ctx, "not_found", false);

    return;
}

void public_get(RequestCtx request_ctx, String url) {
    Arena *request_arena = request_ctx.request_arena;
    int client_socket = request_ctx.client_socket;

    char *path = (char *)arena_alloc(request_arena, sizeof('.') + url.length);
    char *ptr = path;
    *ptr = '.';
    ptr++;
    strncpy(ptr, url.start_addr, url.length);

    char *content_type = file_content_type(request_arena, path);
    char *content = find_value(path, global_arena_data->public_files_dict);

    char *response = (char *)request_arena->current;

    sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: %lu\r\n"
            "Content-Type: %s\r\n\r\n"
            "%s",
            strlen(content), content_type, content);

    response[strlen(response)] = '\0';

    request_arena->current = response + strlen(response) + 1;

    if (send(client_socket, response, strlen(response), 0) == -1) {
        /* TODO */
    }
}

/**
 * Generic handler for pages (e.g., login, registration) that do not require
 * authentication. when `accepts_query_params` is true, query values are
 * rendered in their respective template placeholders.
 */
void view_get(RequestCtx request_ctx, char *view, boolean accepts_query_params) {
    Arena *request_arena = request_ctx.request_arena;
    int client_socket = request_ctx.client_socket;
    char *request = request_ctx.request;

    Dict replaces = {0};
    if (accepts_query_params) {
        String query_params = find_http_request_value("QUERY_PARAMS", request);

        if (query_params.length > 0) {
            replaces = parse_and_decode_params(request_arena, query_params);
        }
    }

    char *template = find_value(view, global_arena_data->templates);

    char *response = NULL;
    char *ptr = NULL;
    ARENA_IN_USE(request_arena, response, ptr) {
        sprintf(response,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: %lu\r\n"
                "Content-Type: text/html\r\n\r\n"
                "%s",
                strlen(template), template);

        uint8_t size = get_dictionary_size(replaces);

        uint8_t i;
        for (i = 0; i < size; i++) {
            KV kv = get_key_value(replaces, i);

            replace_val(response, kv.k, kv.v);
        }

        ptr = response + strlen(response) + 1;
    }

    if (send(client_socket, response, strlen(response), 0) == -1) {
        /* TODO */
    }
}

/**
 * A dummy page used for testing purposes.
 */
/*
void test_get(RequestCtx request_ctx) {
    Arena *request_arena = request_ctx.request_arena;
    int client_socket = request_ctx.client_socket;
    char *request = request_ctx.request;
    DBConnection *connection = NULL;
    char *response = NULL;

    connection = get_available_connection(request_arena);

    const char *command_1 = "SELECT * FROM app.countries WHERE id = $1 OR id = $2";

    Oid paramTypes_1[N2_PARAMS] = {23, 23};
    int id1 = htonl(3);
    int id2 = htonl(23);
    const char *paramValues_1[N2_PARAMS];
    paramValues_1[0] = (char *)&id1;
    paramValues_1[1] = (char *)&id2;
    int paramLengths_1[N2_PARAMS] = {sizeof(id1), sizeof(id2)};
    int paramFormats_1[N2_PARAMS] = {1, 1};

    PGresult *result_1 = WPQsendQueryParams(connection, command_1, N2_PARAMS, paramTypes_1, paramValues_1, paramLengths_1, paramFormats_1, TEXT);
    PQclear(result_1);

    char *template = find_value("test", global_arena_data->templates);

    response = (char *)request_arena->current;

    sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n\r\n"
            "%s",
            template);

    response[strlen(response)] = '\0';

    char _key_value_01[] = "name\0Table 1ajkshdkjsadhakjsdhsadkjhasjdksahdjkashdakj\0sub_name\0some long subname";
    CharsBlock key_value_01;
    key_value_01.start_addr = _key_value_01;
    key_value_01.end_addr = &(_key_value_01[sizeof(_key_value_01)]);

    char _key_value_02[] = "name\0Table 2\0sub_name\0some long subname";
    CharsBlock key_value_02;
    key_value_02.start_addr = _key_value_02;
    key_value_02.end_addr = &(_key_value_02[sizeof(_key_value_02)]);

    char _key_value_03[] = "name\0Table 3\0sub_name\0some long subname";
    CharsBlock key_value_03;
    key_value_03.start_addr = _key_value_03;
    key_value_03.end_addr = &(_key_value_03[sizeof(_key_value_03)]);

    CharsBlock empty = {0};

    render_val(response, "page_name", "Home page!");

    render_for(response, "table", 3, key_value_01, key_value_02, key_value_03);

    render_for(response, "rows", 2, empty, empty);
    render_for(response, "cells", 3, empty, empty, empty);
    render_for(response, "cells", 0);

    render_for(response, "rows", 5, empty, empty, empty, empty, empty);
    render_for(response, "cells", 4, empty, empty, empty, empty);
    render_for(response, "cells", 0);
    render_for(response, "cells", 0);
    render_for(response, "cells", 2, empty, empty);
    render_for(response, "cells", 1, empty);

    render_for(response, "rows", 3, empty, empty, empty);
    render_for(response, "cells", 1, empty);
    render_for(response, "cells", 1, empty);

    char _key_value[] = ".\0my val";
    CharsBlock key_value;
    key_value.start_addr = _key_value;
    key_value.end_addr = &(_key_value[sizeof(_key_value)]);

    render_for(response, "cells", 1, key_value);

    request_arena->current = response + strlen(response) + 1;

    if (send(client_socket, response, strlen(response), 0) == -1) {
    }

    request_cleanup(request_arena, connection, client_socket);
}
*/

void home_get(RequestCtx request_ctx) {
    Arena *request_arena = request_ctx.request_arena;
    int client_socket = request_ctx.client_socket;

    char *template = find_value("home", global_arena_data->templates);

    char *response = NULL;
    char *ptr = NULL;

    Dict user = is_authenticated(request_ctx);
    if (user.start_addr) {
        ARENA_IN_USE(request_arena, response, ptr) {
            /** TODO: Add some data to the "home" template that is specific to the user when logged in */

            sprintf(response,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: %lu\r\n"
                    "Content-Type: text/html\r\n\r\n"
                    "%s",
                    strlen(template), template);

            ptr = response + strlen(response) + 1;
        }
    } else {
        ARENA_IN_USE(request_arena, response, ptr) {
            sprintf(response,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: %lu\r\n"
                    "Content-Type: text/html\r\n\r\n"
                    "%s",
                    strlen(template), template);

            ptr = response + strlen(response) + 1;
        }
    }

    if (send(client_socket, response, strlen(response), 0) == -1) {
        /* TODO */
    }
}

void account_get(RequestCtx request_ctx) {
    Arena *request_arena = request_ctx.request_arena;
    int client_socket = request_ctx.client_socket;

    char *response = NULL;
    char *ptr = NULL;

    Dict user = is_authenticated(request_ctx);
    if (user.start_addr) {
        char *template = find_value("profile", global_arena_data->templates);
        char *email = find_value("email", user);

        char *template_cpy = NULL;
        char *tmp = NULL;
        ARENA_IN_USE(scratch_arena, template_cpy, tmp) {
            memcpy(template_cpy, template, strlen(template));
            render_val(template_cpy, "email", email);

            tmp = template_cpy + strlen(template_cpy) + 1;
        }

        ARENA_IN_USE(request_arena, response, ptr) {
            sprintf(response,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: %lu\r\n"
                    "Content-Type: text/html\r\n\r\n"
                    "%s",
                    strlen(template_cpy), template_cpy);

            ptr = response + strlen(response) + 1;
        }

        arena_reset(scratch_arena, sizeof(Arena));
    } else {
        char *template = find_value("welcome", global_arena_data->templates);

        ARENA_IN_USE(request_arena, response, ptr) {
            sprintf(response,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: %lu\r\n"
                    "Content-Type: text/html\r\n\r\n"
                    "%s",
                    strlen(template), template);

            ptr = response + strlen(response) + 1;
        }
    }

    if (send(client_socket, response, strlen(response), 0) == -1) {
        /* TODO */
    }
}

void auth_check_email_post(RequestCtx request_ctx) {
    Arena *request_arena = request_ctx.request_arena;
    int client_socket = request_ctx.client_socket;
    char *request = request_ctx.request;

    char *response = NULL;
    char *ptr = NULL;

    String body = find_body(request);
    Dict params = parse_and_decode_params(request_arena, body);

    char *email = find_value("email", params);
    ValidationError bad_email = validate_email(request_arena, email);
    if (bad_email) {
        /* TODO */
    }

    int rc;
    const char *sql = "SELECT email FROM users WHERE email = ?";

    rc = sqlite3_exec(request_ctx.db, sql, 0, 0, 0);
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(request_ctx.db, sql, strlen(sql) + 1, &stmt, NULL);
    assert(rc == SQLITE_OK);

    rc = sqlite3_bind_text(stmt, 1, email, strlen(email), SQLITE_STATIC);
    assert(rc == SQLITE_OK);

    char *template = NULL;

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        template = find_value("login_view", global_arena_data->templates);
    } else {
        template = find_value("register_view", global_arena_data->templates);
    }

    char *template_cpy = NULL;
    char *tmp = NULL;
    ARENA_IN_USE(scratch_arena, template_cpy, tmp) {
        memcpy(template_cpy, template, strlen(template));
        replace_val(template_cpy, "email", email);

        tmp = template_cpy + strlen(template_cpy) + 1;
    }

    ARENA_IN_USE(request_arena, response, ptr) {
        sprintf(response,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: %lu\r\n"
                "Content-Type: text/html\r\n\r\n"
                "%s",
                strlen(template_cpy), template_cpy);

        ptr = response + strlen(response) + 1;
    }

    arena_reset(scratch_arena, sizeof(Arena));

    if (send(client_socket, response, strlen(response), 0) == -1) {
        /* TODO */
    }
}

/*
void register_create_account_post(RequestCtx request_ctx) {
    Arena *request_arena = request_ctx.request_arena;
    int client_socket = request_ctx.client_socket;
    char *request = request_ctx.request;
    DBConnection *connection = NULL;
    char *response = NULL;

    String body = find_body(request);
    Dict params = parse_and_decode_params(request_arena, body);

    char *email = find_value("email", params);
    char *password = find_value("password", params);
    char *repeat_password = find_value("password-again", params);
    char *accept_terms = find_value("accept-terms", params);

    ValidationError bad_email = validate_email(request_arena, email);
    ValidationError bad_password = validate_password(request_arena, password);
    ValidationError bad_repeat_password = validate_repeat_password(request_arena, password, repeat_password);
    ValidationError bad_accept_terms = validate_accept_terms(request_arena, accept_terms);

    if (bad_email || bad_password || bad_repeat_password || bad_accept_terms) {
        char *template = find_value("register_validation_errors", global_arena_data->templates);

        response = (char *)request_arena->current;

        sprintf(response,
                "HTTP/1.1 422 Unprocessable Entity\r\n"
                "Content-Type: text/html\r\n\r\n"
                "%s",
                template);

        response[strlen(response)] = '\0';

        if (bad_email) {
            render_val(response, "email_validation_error_message", bad_email);
        }

        if (bad_password) {
            render_val(response, "password_validation_error_message", bad_password);
        }

        if (bad_repeat_password) {
            render_val(response, "repeat_password_validation_error_message", bad_repeat_password);
        }

        if (bad_accept_terms) {
            render_val(response, "accept_terms_validation_error_message", bad_accept_terms);
        }

        request_arena->current = response + strlen(response) + 1;

        goto send_response;
    }

    connection = get_available_connection(request_arena);

    uint8_t salt[SALT_LENGTH];
    memset(salt, 0, SALT_LENGTH);

    assert(generate_salt(salt, SALT_LENGTH) != -1);

    uint32_t t_cost = 2;
    uint32_t m_cost = (1 << 16);
    uint32_t parallelism = 1;

    uint8_t hash[HASH_LENGTH];
    memset(hash, 0, HASH_LENGTH);

    char secure_password[PASSWORD_BUFFER];
    memset(secure_password, 0, PASSWORD_BUFFER);
    if (argon2i_hash_raw(t_cost, m_cost, parallelism, password, strlen(password), salt, SALT_LENGTH, hash, HASH_LENGTH) != ARGON2_OK) {
        fprintf(stderr, "Failed to create hash from password\nError code: %d\n", errno);
    }

    if (argon2i_hash_encoded(t_cost, m_cost, parallelism, password, strlen(password), salt, SALT_LENGTH, HASH_LENGTH, secure_password, PASSWORD_BUFFER) != ARGON2_OK) {
        fprintf(stderr, "Failed to encode hash\nError code: %d\n", errno);
    }

    if (argon2i_verify(secure_password, password, strlen(password)) != ARGON2_OK) {
        fprintf(stderr, "Failed to verify password\nError code: %d\n", errno);
    }

    const char *command_1 = "INSERT INTO app.users (email, password) VALUES ($1, $2) RETURNING id";

    Oid paramTypes_1[N2_PARAMS] = {25, 25};
    const char *paramValues_1[N2_PARAMS];
    paramValues_1[0] = email;
    paramValues_1[1] = secure_password;
    int paramLengths_1[N2_PARAMS] = {0, 0};
    int paramFormats_1[N2_PARAMS] = {0, 0};

    PGresult *result_1 = WPQsendQueryParams(connection, command_1, N2_PARAMS, paramTypes_1, paramValues_1, paramLengths_1, paramFormats_1, BINARY);

    unsigned char *user_id = (unsigned char *)PQgetvalue(result_1, 0, 0);

    const char *command_2 = "INSERT INTO app.users_sessions (user_id, expires_at) "
                            "VALUES ($1, NOW() + INTERVAL '1 hour') "
                            "ON CONFLICT (user_id) DO UPDATE SET "
                            "updated_at = NOW(), expires_at = EXCLUDED.expires_at "
                            "RETURNING id, to_char(expires_at, 'Dy, DD Mon YYYY HH24:MI:SS GMT') AS expires_at;";

    Oid paramTypes_2[N1_PARAMS] = {2950};
    const char *paramValues_2[N1_PARAMS];
    paramValues_2[0] = (const char *)user_id;
    int paramLengths_2[N1_PARAMS] = {16};
    int paramFormats_2[N1_PARAMS] = {1};

    PGresult *result_2 = WPQsendQueryParams(connection, command_2, N1_PARAMS, paramTypes_2, paramValues_2, paramLengths_2, paramFormats_2, TEXT);

    PQclear(result_1);

    char *session_id = PQgetvalue(result_2, 0, 0);
    char *expires_at = PQgetvalue(result_2, 0, 1);

    response = (char *)request_arena->current;

    sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Set-Cookie: session_id=%s; Path=/; HttpOnly; Secure; SameSite=Strict; Expires=%s\r\n"
            "HX-Redirect: /\r\n\r\n",
            session_id, expires_at);

    request_arena->current = response + strlen(response) + 1;

    PQclear(result_2);

send_response:
    if (send(client_socket, response, strlen(response), 0) == -1) {
    }

    request_cleanup(request_arena, connection, client_socket);
}
*/

/*
void logout_post(RequestCtx request_ctx) {
    Arena *request_arena = request_ctx.request_arena;
    int client_socket = request_ctx.client_socket;
    char *request = request_ctx.request;
    DBConnection *connection = NULL;

    String cookie = find_http_request_value("Cookie", request);
    if (cookie.start_addr && cookie.length) {
        String session_id_reference = find_cookie_value("session_id", cookie);
        if (session_id_reference.start_addr && session_id_reference.length) {
            uuid_str_t session_id_str;
            memset(session_id_str, 0, sizeof(uuid_str_t));
            memcpy(session_id_str, session_id_reference.start_addr, session_id_reference.length);
            session_id_str[session_id_reference.length] = '\0';

            uuid_t session_id;
            memset(session_id, 0, sizeof(uuid_t));
            uuid_parse(session_id_str, session_id);

            connection = get_available_connection(request_arena);

            const char *command_1 = "DELETE FROM app.users_sessions WHERE id = $1";

            Oid paramTypes_1[N1_PARAMS] = {2950};
            const char *paramValues_1[N1_PARAMS];
            paramValues_1[0] = (const char *)session_id;
            int paramLengths_1[N1_PARAMS] = {16};
            int paramFormats_1[N1_PARAMS] = {1};

            PGresult *result_1 = WPQsendQueryParams(connection, command_1, N1_PARAMS, paramTypes_1, paramValues_1, paramLengths_1, paramFormats_1, TEXT);

            PQclear(result_1);
        }
    }

    char response[] = "HTTP/1.1 200 OK\r\n"
                      "HX-Refresh: true\r\n\r\n";

    if (send(client_socket, response, strlen(response), 0) == -1) {
    }

    request_cleanup(request_arena, connection, client_socket);
}
*/

/*
void login_create_session_post(RequestCtx request_ctx) {
    Arena *request_arena = request_ctx.request_arena;
    int client_socket = request_ctx.client_socket;
    char *request = request_ctx.request;
    DBConnection *connection = NULL;
    char *response = NULL;

    String body = find_body(request);
    Dict params = parse_and_decode_params(request_arena, body);

    char *password = find_value("password", params);

    ValidationError bad_password = validate_password(request_arena, password);
    if (bad_password) {
        char *template = find_value("login_errors", global_arena_data->templates);

        response = (char *)request_arena->current;

        sprintf(response,
                "HTTP/1.1 422 Unprocessable Entity\r\n"
                "Content-Type: text/html\r\n\r\n"
                "%s",
                template);

        response[strlen(response)] = '\0';

        render_val(response, "password_validation_error_message", bad_password);

        request_arena->current = response + strlen(response) + 1;

        goto send_response;
    }

    connection = get_available_connection(request_arena);

    char *email = find_value("email", params);

    const char *command_1 = "SELECT id, password FROM app.users WHERE email = $1";

    Oid paramTypes_1[N1_PARAMS] = {25};
    const char *paramValues_1[N1_PARAMS];
    paramValues_1[0] = email;
    int paramLengths_1[N1_PARAMS] = {0};
    int paramFormats_1[N1_PARAMS] = {0};

    PGresult *result_1 = WPQsendQueryParams(connection, command_1, N1_PARAMS, paramTypes_1, paramValues_1, paramLengths_1, paramFormats_1, BINARY);

    char *stored_password = PQgetvalue(result_1, 0, 1);

    if (argon2i_verify(stored_password, password, strlen(password)) != ARGON2_OK) {
        PQclear(result_1);

        char *template = find_value("login_errors", global_arena_data->templates);

        response = (char *)request_arena->current;

        sprintf(response,
                "HTTP/1.1 422 Unprocessable Entity\r\n"
                "Content-Type: text/html\r\n\r\n"
                "%s",
                template);

        response[strlen(response)] = '\0';

        render_val(response, "wrong_password_error_message", "Incorrect password.");

        request_arena->current = response + strlen(response) + 1;

        goto send_response;
    }

    unsigned char *user_id = (unsigned char *)PQgetvalue(result_1, 0, 0);

    const char *command_2 = "INSERT INTO app.users_sessions (user_id, expires_at) "
                            "VALUES ($1, NOW() + INTERVAL '1 hour') "
                            "ON CONFLICT (user_id) DO UPDATE SET "
                            "updated_at = NOW(), expires_at = EXCLUDED.expires_at "
                            "RETURNING id, to_char(expires_at, 'Dy, DD Mon YYYY HH24:MI:SS GMT') AS expires_at;";

    Oid paramTypes_2[N1_PARAMS] = {2950};
    const char *paramValues_2[N1_PARAMS];
    paramValues_2[0] = (const char *)user_id;
    int paramLengths_2[N1_PARAMS] = {16};
    int paramFormats_2[N1_PARAMS] = {1};

    PGresult *result_2 = WPQsendQueryParams(connection, command_2, N1_PARAMS, paramTypes_2, paramValues_2, paramLengths_2, paramFormats_2, TEXT);

    PQclear(result_1);

    char *session_id = PQgetvalue(result_2, 0, 0);
    char *expires_at = PQgetvalue(result_2, 0, 1);

    response = (char *)request_arena->current;

    sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Set-Cookie: session_id=%s; Path=/; HttpOnly; Secure; SameSite=Strict; Expires=%s\r\n"
            "HX-Redirect: /\r\n\r\n",
            session_id, expires_at);

    request_arena->current = response + strlen(response) + 1;

    PQclear(result_2);

send_response:
    if (send(client_socket, response, strlen(response), 0) == -1) {
    }

    request_cleanup(request_arena, connection, client_socket);
}
*/

Dict is_authenticated(RequestCtx request_ctx) {
    Arena *request_arena = request_ctx.request_arena;
    char *request = request_ctx.request;
    sqlite3 *db = request_ctx.db;

    Dict user = {0};

    String cookie = find_http_request_value("Cookie", request);
    if (cookie.start_addr && cookie.length) {
        String session_id_reference = find_cookie_value("session_id", cookie);
        if (session_id_reference.start_addr && session_id_reference.length) {
            int rc;

            const char *sql = "SELECT u.id, u.email "
                              "FROM users_sessions us "
                              "JOIN users u ON u.id = us.user_id "
                              "WHERE us.id = ? AND datetime('now') < us.expires_at";

            rc = sqlite3_exec(db, sql, 0, 0, 0);
            sqlite3_stmt *stmt = NULL;
            rc = sqlite3_prepare_v2(db, sql, strlen(sql) + 1, &stmt, NULL);
            assert(rc == SQLITE_OK);

            char session_id_str[10];
            memset(session_id_str, 0, sizeof(session_id_str));
            memcpy(session_id_str, session_id_reference.start_addr, session_id_reference.length);

            int session_id = atoi(session_id_str);
            rc = sqlite3_bind_int(stmt, 1, session_id);
            assert(rc == SQLITE_OK);

            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                int id = sqlite3_column_int(stmt, 0);
                const unsigned char *user_email = sqlite3_column_text(stmt, 1);

                char user_id[10];
                memset(user_id, 0, sizeof(user_id));
                sprintf(user_id, "%d", id);

                char *user_info = NULL;
                char *ptr = NULL;
                ARENA_IN_USE(request_arena, user_info, ptr) { user = add_to_dictionary(user_info, 2, "id", user_id, "email", user_email); }
            } else {
                assert(rc == SQLITE_DONE);
            }

            sqlite3_finalize(stmt);

            return user;
        }
    }

    return user;
}

/*
void request_cleanup(Arena *arena, DBConnection *connection, int client_socket) {
    close(client_socket);
    arena_free(arena);

    if (connection != NULL && connection->client.fd != 0) {
        uint8_t was_request_queued = connection->client.queued;

        memset(&(connection->client), 0, sizeof(Client));

        int conn_socket = PQsocket(connection->conn);
        event.events = EPOLLOUT | EPOLLET;
        event.data.ptr = connection;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn_socket, &event);

        if (was_request_queued) {
            longjmp(db_ctx, 1);
        }

        longjmp(ctx, 1);
    }
}
*/

ValidationError validate_email(Arena *arena, const char *email) {
    regex_t regex;

    assert(regcomp(&regex, EMAIL_REGEX, REG_EXTENDED) == 0);

    if (regexec(&regex, email, 0, NULL, 0) == REG_NOMATCH) {
        char error[] = "Invalid email format";
        size_t error_length = strlen(error);
        ValidationError err = arena_alloc(arena, error_length + 1);
        memcpy(err, error, error_length);

        return err;
    }

    return NULL;
}

ValidationError validate_password(Arena *arena, const char *password) {
    if (strlen(password) < 4) {
        char error[] = "Password should be at least 4 characters";
        size_t error_length = strlen(error);
        ValidationError err = arena_alloc(arena, error_length + 1);
        memcpy(err, error, error_length);

        return err;
    }

    return NULL;
}

ValidationError validate_repeat_password(Arena *arena, const char *password, const char *repeat_password) {
    if (strcmp(password, repeat_password) != 0) {
        char error[] = "Password and repeat password should match";
        size_t error_length = strlen(error);
        ValidationError err = arena_alloc(arena, error_length + 1);
        memcpy(err, error, error_length);

        return err;
    }

    return NULL;
}

ValidationError validate_accept_terms(Arena *arena, const char *accept_terms) {
    if (strcmp(accept_terms, "true") != 0) {
        char error[] = "Accept Terms to proceed";
        size_t error_length = strlen(error);
        ValidationError err = arena_alloc(arena, error_length + 1);
        memcpy(err, error, error_length);

        return err;
    }

    return NULL;
}
