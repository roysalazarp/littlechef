#include "headers.h"

void router(RequestCtx request_ctx) {
    Arena *scratch_arena = request_ctx.scratch_arena;
    int client_socket = request_ctx.client_socket;

    char *request = (char *)scratch_arena->current;
    request_ctx.request = request;

    char *tmp_request = request;

    ssize_t read_stream = 0;
    ssize_t buffer_size = KB(2);

    int8_t does_http_request_contain_body = -1; /* -1 means we haven't checked yet */
    String method;

    int8_t is_multipart_form_data = -1; /* -1 means we haven't checked yet */
    String content_type;

    while (1) {
        char *advanced_request_ptr = tmp_request + read_stream;

        ssize_t incomming_stream_size = recv(client_socket, advanced_request_ptr, buffer_size - read_stream, 0);
        if (incomming_stream_size == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (read_stream > 0) {
                    break;
                }

                longjmp(ctx, 1);
            }
        }

        if (incomming_stream_size <= 0) {
            printf("fd %d - Empty request\n", client_socket);
            return;
        }

        /**
         * NOTE: While it is possible to decode the entire HTTP request here at once,
         * we avoid doing so to prevent potential issues with requests containing
         * non-textual data, such as images or binary files. Processing such data
         * inappropriately could lead to corruption or unexpected behavior.
         */

        read_stream += incomming_stream_size;

        if (does_http_request_contain_body == -1) {
            method = find_http_request_value("METHOD", advanced_request_ptr);

            if (strncmp("GET", method.start_addr, method.length) == 0 || strncmp("HEAD", method.start_addr, method.length) == 0) {
                does_http_request_contain_body = 0;
            } else {
                does_http_request_contain_body = 1;
            }
        }

        if (!does_http_request_contain_body) {
            uint8_t request_ended = 0;

            char *current_start = advanced_request_ptr;
            char *current_end = advanced_request_ptr + incomming_stream_size;

            while (current_start < current_end) {
                char header_end[] = "\r\n\r\n";
                if (strncmp(current_start, header_end, strlen(header_end)) == 0) {
                    request_ended = 1;
                    break;
                }

                current_start++;
            }

            if (request_ended) {
                break;
            }
        }

        if (is_multipart_form_data == -1) {
            content_type = find_http_request_value("Content-Type", advanced_request_ptr);

            if (strncmp("multipart/form-data", content_type.start_addr, content_type.length) == 0) {
                is_multipart_form_data = 1;
            } else {
                is_multipart_form_data = 0;
            }
        }

        if (is_multipart_form_data) {
            uint8_t request_ended = 0;

            char *current_start = advanced_request_ptr;
            char *current_end = advanced_request_ptr + incomming_stream_size;

            while (current_start < current_end) {
                char multipart_form_data_end[] = "--\r\n";
                if (strncmp(current_start, multipart_form_data_end, strlen(multipart_form_data_end)) == 0) {
                    request_ended = 1;
                    break;
                }

                current_start++;
            }

            if (request_ended) {
                break;
            }
        }

        if (read_stream >= buffer_size) {
            buffer_size += KB(2);
        }
    }

    tmp_request += read_stream;
    (*tmp_request) = '\0';
    tmp_request++;

    scratch_arena->current = tmp_request;

    String url = find_http_request_value("URL", request);

    if (strncmp(url.start_addr, URL("/.well-known/assetlinks.json"), strlen(URL("/.well-known/assetlinks.json"))) == 0 && strncmp(method.start_addr, "GET", method.length) == 0) {
        char str[] = "/public/.well-known/assetlinks.json";
        String _url = {0};
        _url.start_addr = str;
        _url.length = strlen(str);

        public_get(request_ctx, _url);
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

    if (strncmp(url.start_addr, URL("/auth/check-email"), strlen(URL("/auth/check-email"))) == 0) {
        if (strncmp(method.start_addr, "POST", method.length) == 0) {
            auth_check_email_post(request_ctx);
            return;
        }
    }

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

    view_get(request_ctx, "not_found", false);

    return;
}

void public_get(RequestCtx request_ctx, String url) {
    Arena *scratch_arena = request_ctx.scratch_arena;
    int client_socket = request_ctx.client_socket;

    char *path = (char *)arena_alloc(scratch_arena, sizeof('.') + url.length);
    char *ptr = path;
    *ptr = '.';
    ptr++;
    strncpy(ptr, url.start_addr, url.length);

    char *content_type = file_content_type(scratch_arena, path);
    char *content = find_value(path, arena_data->public_files_dict);

    char *response = (char *)scratch_arena->current;

    sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n\r\n"
            "%s",
            content_type, content);

    response[strlen(response)] = '\0';

    scratch_arena->current = response + strlen(response) + 1;

    if (send(client_socket, response, strlen(response), 0) == -1) {
    }

    request_cleanup(scratch_arena, NULL, client_socket);
}

/**
 * Generic handler for pages (e.g., login, registration) that do not require
 * authentication. when `accepts_query_params` is true, query values are
 * rendered in their respective template placeholders.
 */
void view_get(RequestCtx request_ctx, char *view, boolean accepts_query_params) {
    Arena *scratch_arena = request_ctx.scratch_arena;
    int client_socket = request_ctx.client_socket;
    char *request = request_ctx.request;

    Dict replaces = {0};
    if (accepts_query_params) {
        String query_params = find_http_request_value("QUERY_PARAMS", request);

        if (query_params.length > 0) {
            replaces = parse_and_decode_params(scratch_arena, query_params);
        }
    }

    char *template = find_value(view, arena_data->templates);

    char *response = (char *)scratch_arena->current;

    sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n\r\n"
            "%s",
            template);

    response[strlen(response)] = '\0';

    uint8_t size = get_dictionary_size(replaces);

    uint8_t i;
    for (i = 0; i < size; i++) {
        KV kv = get_key_value(replaces, i);

        replace_val(response, kv.k, kv.v);
    }

    scratch_arena->current = response + strlen(response) + 1;

    if (send(client_socket, response, strlen(response), 0) == -1) {
    }

    request_cleanup(scratch_arena, NULL, client_socket);
}

/**
 * A dummy page used for testing purposes.
 */
void test_get(RequestCtx request_ctx) {
    Arena *scratch_arena = request_ctx.scratch_arena;
    int client_socket = request_ctx.client_socket;
    char *request = request_ctx.request;
    DBConnection *connection = NULL;
    char *response = NULL;

    connection = get_available_connection(scratch_arena);

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

    char *template = find_value("test", arena_data->templates);

    response = (char *)scratch_arena->current;

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

    scratch_arena->current = response + strlen(response) + 1;

    if (send(client_socket, response, strlen(response), 0) == -1) {
    }

    request_cleanup(scratch_arena, connection, client_socket);
}

void home_get(RequestCtx request_ctx) {
    Arena *scratch_arena = request_ctx.scratch_arena;
    int client_socket = request_ctx.client_socket;
    char *request = request_ctx.request;
    DBConnection *connection = NULL;
    char *response = NULL;

    connection = get_available_connection(scratch_arena);

    char *template = find_value("home", arena_data->templates);

    Dict user = is_authenticated(request_ctx, connection);
    if (user.start_addr) {
        response = (char *)scratch_arena->current;

        sprintf(response,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n\r\n"
                "%s",
                template);

        response[strlen(response)] = '\0';

        /** TODO: Add some data to the "home" template that is specific to the user when logged in */

        scratch_arena->current = response + strlen(response) + 1;

        goto send_response;
    }

    response = (char *)scratch_arena->current;

    sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n\r\n"
            "%s",
            template);

    response[strlen(response)] = '\0';

    scratch_arena->current = response + strlen(response) + 1;

send_response:
    if (send(client_socket, response, strlen(response), 0) == -1) {
    }

    request_cleanup(scratch_arena, connection, client_socket);
}

void account_get(RequestCtx request_ctx) {
    Arena *scratch_arena = request_ctx.scratch_arena;
    int client_socket = request_ctx.client_socket;
    char *request = request_ctx.request;
    DBConnection *connection = NULL;
    char *response = NULL;

    connection = get_available_connection(scratch_arena);

    Dict user = is_authenticated(request_ctx, connection);
    if (user.start_addr) {
        char *template = find_value("profile", arena_data->templates);
        char *email = find_value("email", user);

        response = (char *)scratch_arena->current;

        sprintf(response,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n\r\n"
                "%s",
                template);

        response[strlen(response)] = '\0';

        render_val(response, "email", email);

        scratch_arena->current = response + strlen(response) + 1;
    } else {
        char *template = find_value("auth", arena_data->templates);

        response = (char *)scratch_arena->current;

        sprintf(response,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n\r\n"
                "%s",
                template);

        response[strlen(response)] = '\0';

        scratch_arena->current = response + strlen(response) + 1;
    }

    if (send(client_socket, response, strlen(response), 0) == -1) {
    }

    request_cleanup(scratch_arena, connection, client_socket);
}

void auth_check_email_post(RequestCtx request_ctx) {
    Arena *scratch_arena = request_ctx.scratch_arena;
    int client_socket = request_ctx.client_socket;
    char *request = request_ctx.request;
    DBConnection *connection = NULL;
    char *response = NULL;

    String body = find_body(request);
    Dict params = parse_and_decode_params(scratch_arena, body);

    char *email = find_value("email", params);
    ValidationError bad_email = validate_email(scratch_arena, email);

    if (bad_email) {
        char *template = find_value("email_check_error", arena_data->templates);

        response = (char *)scratch_arena->current;

        sprintf(response,
                "HTTP/1.1 422 Unprocessable Entity\r\n"
                "Content-Type: text/html\r\n\r\n"
                "%s",
                template);

        response[strlen(response)] = '\0';

        render_val(response, "email_validation_error_message", bad_email);

        scratch_arena->current = response + strlen(response) + 1;

        goto send_response;
    }

    connection = get_available_connection(scratch_arena);

    const char *command_1 = "SELECT email FROM app.users WHERE email = $1";
    Oid paramTypes_1[N1_PARAMS] = {25};
    const char *paramValues_1[N1_PARAMS];
    paramValues_1[0] = email;
    int paramLengths_1[N1_PARAMS] = {0};
    int paramFormats_1[N1_PARAMS] = {0};

    PGresult *result_1 = WPQsendQueryParams(connection, command_1, N1_PARAMS, paramTypes_1, paramValues_1, paramLengths_1, paramFormats_1, TEXT);

    int rows = PQntuples(result_1);
    PQclear(result_1);

    char *template = NULL;

    if (rows) {
        template = find_value("login", arena_data->templates);
    } else {
        template = find_value("register", arena_data->templates);
    }

    response = (char *)scratch_arena->current;

    sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n\r\n"
            "%s",
            template);

    response[strlen(response)] = '\0';

    replace_val(response, "email", email);

    scratch_arena->current = response + strlen(response) + 1;

send_response:
    if (send(client_socket, response, strlen(response), 0) == -1) {
    }

    request_cleanup(scratch_arena, connection, client_socket);
}

void register_create_account_post(RequestCtx request_ctx) {
    Arena *scratch_arena = request_ctx.scratch_arena;
    int client_socket = request_ctx.client_socket;
    char *request = request_ctx.request;
    DBConnection *connection = NULL;
    char *response = NULL;

    String body = find_body(request);
    Dict params = parse_and_decode_params(scratch_arena, body);

    char *email = find_value("email", params);
    char *password = find_value("password", params);
    char *repeat_password = find_value("password-again", params);
    char *accept_terms = find_value("accept-terms", params);

    ValidationError bad_email = validate_email(scratch_arena, email);
    ValidationError bad_password = validate_password(scratch_arena, password);
    ValidationError bad_repeat_password = validate_repeat_password(scratch_arena, password, repeat_password);
    ValidationError bad_accept_terms = validate_accept_terms(scratch_arena, accept_terms);

    if (bad_email || bad_password || bad_repeat_password || bad_accept_terms) {
        char *template = find_value("register_validation_errors", arena_data->templates);

        response = (char *)scratch_arena->current;

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

        scratch_arena->current = response + strlen(response) + 1;

        goto send_response;
    }

    connection = get_available_connection(scratch_arena);

    /** Hash user password */
    uint8_t salt[SALT_LENGTH];
    memset(salt, 0, SALT_LENGTH);

    assert(generate_salt(salt, SALT_LENGTH) != -1);

    uint32_t t_cost = 2;         /* 2-pass computation */
    uint32_t m_cost = (1 << 16); /* 64 mebibytes memory usage */
    uint32_t parallelism = 1;    /* number of threads and lanes */

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

    /** Create session */
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

    response = (char *)scratch_arena->current;

    sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Set-Cookie: session_id=%s; Path=/; HttpOnly; Secure; SameSite=Strict; Expires=%s\r\n"
            "HX-Redirect: /\r\n\r\n",
            session_id, expires_at);

    scratch_arena->current = response + strlen(response) + 1;

    PQclear(result_2);

send_response:
    if (send(client_socket, response, strlen(response), 0) == -1) {
    }

    request_cleanup(scratch_arena, connection, client_socket);
}

void logout_post(RequestCtx request_ctx) {
    Arena *scratch_arena = request_ctx.scratch_arena;
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

            connection = get_available_connection(scratch_arena);

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

    request_cleanup(scratch_arena, connection, client_socket);
}

void login_create_session_post(RequestCtx request_ctx) {
    Arena *scratch_arena = request_ctx.scratch_arena;
    int client_socket = request_ctx.client_socket;
    char *request = request_ctx.request;
    DBConnection *connection = NULL;
    char *response = NULL;

    String body = find_body(request);
    Dict params = parse_and_decode_params(scratch_arena, body);

    char *password = find_value("password", params);

    ValidationError bad_password = validate_password(scratch_arena, password);
    if (bad_password) {
        char *template = find_value("login_errors", arena_data->templates);

        response = (char *)scratch_arena->current;

        sprintf(response,
                "HTTP/1.1 422 Unprocessable Entity\r\n"
                "Content-Type: text/html\r\n\r\n"
                "%s",
                template);

        response[strlen(response)] = '\0';

        render_val(response, "password_validation_error_message", bad_password);

        scratch_arena->current = response + strlen(response) + 1;

        goto send_response;
    }

    connection = get_available_connection(scratch_arena);

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

        char *template = find_value("login_errors", arena_data->templates);

        response = (char *)scratch_arena->current;

        sprintf(response,
                "HTTP/1.1 422 Unprocessable Entity\r\n"
                "Content-Type: text/html\r\n\r\n"
                "%s",
                template);

        response[strlen(response)] = '\0';

        render_val(response, "wrong_password_error_message", "Incorrect password.");

        scratch_arena->current = response + strlen(response) + 1;

        goto send_response;
    }

    unsigned char *user_id = (unsigned char *)PQgetvalue(result_1, 0, 0);

    /** Create session */
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

    response = (char *)scratch_arena->current;

    sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Set-Cookie: session_id=%s; Path=/; HttpOnly; Secure; SameSite=Strict; Expires=%s\r\n"
            "HX-Redirect: /\r\n\r\n",
            session_id, expires_at);

    scratch_arena->current = response + strlen(response) + 1;

    PQclear(result_2);

send_response:
    if (send(client_socket, response, strlen(response), 0) == -1) {
    }

    request_cleanup(scratch_arena, connection, client_socket);
}

Dict is_authenticated(RequestCtx request_ctx, DBConnection *connection) {
    Arena *scratch_arena = request_ctx.scratch_arena;
    char *request = request_ctx.request;

    Dict user = {0};

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

            const char *command_1 = "SELECT u.id, u.email "
                                    "FROM app.users_sessions us "
                                    "JOIN app.users u ON u.id = us.user_id "
                                    "WHERE us.id = $1 AND NOW() < us.expires_at";

            Oid paramTypes_1[N1_PARAMS] = {2950};
            const char *paramValues_1[N1_PARAMS];
            paramValues_1[0] = (const char *)session_id;
            int paramLengths_1[N1_PARAMS] = {16};
            int paramFormats_1[N1_PARAMS] = {1};

            PGresult *result_1 = WPQsendQueryParams(connection, command_1, N1_PARAMS, paramTypes_1, paramValues_1, paramLengths_1, paramFormats_1, TEXT);

            int num_rows = PQntuples(result_1);
            if (num_rows) {
                char *user_info = (char *)scratch_arena->current;

                char *user_id = PQgetvalue(result_1, 0, 0);
                char *user_email = PQgetvalue(result_1, 0, 1);

                user = add_key_value(user_info, 2, "id", user_id, "email", user_email);

                scratch_arena->current = user.end_addr;

                PQclear(result_1);

                return user;
            }
        }
    }

    return user;
}

void request_cleanup(Arena *scratch_arena, DBConnection *connection, int client_socket) {
    close(client_socket);
    arena_free(scratch_arena);

    if (connection != NULL && connection->client.fd != 0) {
        uint8_t was_request_queued = connection->client.queued;

        /* Set connection as unused */
        memset(&(connection->client), 0, sizeof(Client));

        /** Set connection available for write */
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

ValidationError validate_email(Arena *scratch_arena, const char *email) {
    regex_t regex;

    assert(regcomp(&regex, EMAIL_REGEX, REG_EXTENDED) == 0);

    if (regexec(&regex, email, 0, NULL, 0) == REG_NOMATCH) {
        char error[] = "Invalid email format";
        size_t error_length = strlen(error);
        ValidationError err = arena_alloc(scratch_arena, error_length + 1);
        memcpy(err, error, error_length);

        return err;
    }

    return NULL;
}

ValidationError validate_password(Arena *scratch_arena, const char *password) {
    if (strlen(password) < 4) {
        char error[] = "Password should be at least 4 characters";
        size_t error_length = strlen(error);
        ValidationError err = arena_alloc(scratch_arena, error_length + 1);
        memcpy(err, error, error_length);

        return err;
    }

    return NULL;
}

ValidationError validate_repeat_password(Arena *scratch_arena, const char *password, const char *repeat_password) {
    if (strcmp(password, repeat_password) != 0) {
        char error[] = "Password and repeat password should match";
        size_t error_length = strlen(error);
        ValidationError err = arena_alloc(scratch_arena, error_length + 1);
        memcpy(err, error, error_length);

        return err;
    }

    return NULL;
}

ValidationError validate_accept_terms(Arena *scratch_arena, const char *accept_terms) {
    if (strcmp(accept_terms, "true") != 0) {
        char error[] = "Accept Terms to proceed";
        size_t error_length = strlen(error);
        ValidationError err = arena_alloc(scratch_arena, error_length + 1);
        memcpy(err, error, error_length);

        return err;
    }

    return NULL;
}
