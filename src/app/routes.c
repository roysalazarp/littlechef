#include <argon2.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* clang-format off */
#include "./utils.h"
#include "./shared.h"
#include "./memory.h"
#include "./http_utils.h"
#include "./routes_utils.h"
#include "./template_engine.h"
#include "./routes.h"
#include "../db.h"
/* clang-format on */

#define EMAIL_REGEX "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"

#define HASH_LENGTH 32
#define SALT_LENGTH 16
#define PASSWORD_BUFFER 255 /** Do I need this ??? */

typedef char *ValidationError;

int generate_salt(void *salt, size_t salt_size);

ValidationError validate_email(Memory *memory, const char *email) {
    regex_t regex;

    ASSERT(regcomp(&regex, EMAIL_REGEX, REG_EXTENDED) == 0);

    if (regexec(&regex, email, 0, NULL, 0) == REG_NOMATCH) {
        char error[] = "Invalid email format";
        size_t error_length = strlen(error);
        ValidationError err = memory_alloc(memory, error_length + 1);
        memcpy(err, error, error_length);

        return err;
    }

    return NULL;
}

ValidationError validate_password(Memory *memory, const char *password) {
    if (strlen(password) < 4) {
        char error[] = "Password should be at least 4 characters";
        size_t error_length = strlen(error);
        ValidationError err = memory_alloc(memory, error_length + 1);
        memcpy(err, error, error_length);

        return err;
    }

    return NULL;
}

ValidationError validate_repeat_password(Memory *memory, const char *password, const char *repeat_password) {
    if (strcmp(password, repeat_password) != 0) {
        char error[] = "Password and repeat password should match";
        size_t error_length = strlen(error);
        ValidationError err = memory_alloc(memory, error_length + 1);
        memcpy(err, error, error_length);

        return err;
    }

    return NULL;
}

Dict is_authenticated(RequestCtx request_ctx) {
    char *request = request_ctx.request;

    Dict user = {0};

    String cookie = find_http_request_value("Cookie", request);
    if (cookie.start_addr && cookie.length) {
        String sid = find_http_cookie_value("session_id", cookie);
        if (sid.start_addr && sid.length) {
            Memory *request_memory = request_ctx.request_memory;

            Dict params = {0};
            char *p = NULL;
            params.start_addr = p = (char *)memory_in_use(request_memory);

            memcpy(p, "session_id", strlen("session_id"));
            p += strlen(p) + 1;
            memcpy(p, sid.start_addr, sid.length);
            p += strlen(p) + 1;

            params.end_addr = p;
            memory_out_of_use(request_memory, p);

            DictArray rows = request_ctx.query(request_ctx.request_memory, request_ctx.db, IS_USER_AUTHENTICATED, params);
            if (rows.length == 1) {
                user = *(rows.dicts[0]);
            }

            return user;
        }
    }

    return user;
}

Response view_get(RequestCtx request_ctx, char *view) {
    PersistingData *persisting_data = (PersistingData *)((uint8_t *)request_ctx.persisting_memory->start + sizeof(Memory));
    Dict templates = persisting_data->templates;

    Memory *request_memory = request_ctx.request_memory;

    char *template = find_value(view, templates);

    Response rendered_response = {0};
    char *p = NULL;
    rendered_response.content = p = (char *)memory_in_use(request_memory);
    sprintf(p,
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: %lu\r\n"
            "Content-Type: text/html\r\n\r\n"
            "%s",
            strlen(template), template);

    p += strlen(p) + 1;
    memory_out_of_use(request_memory, p);

    rendered_response.length = p - rendered_response.content;

    return rendered_response;
}

Response public_get(RequestCtx request_ctx, Dict public_assets, String url) {
    Memory *request_memory = request_ctx.request_memory;

    char *full_path = NULL;
    char *p = NULL;

    full_path = p = (char *)memory_in_use(request_memory);

    ASSERT(getcwd(full_path, PATH_MAX) != NULL);
    p += strlen(p);
    memcpy(p, url.start_addr, url.length);
    p += strlen(p) + 1;

    memory_out_of_use(request_memory, p);

    char *content_type = file_content_type(request_memory, full_path);
    char *content = find_value(full_path, public_assets);

    Response rendered_response = {0};
    rendered_response.content = p = (char *)memory_in_use(request_memory);

    sprintf(p,
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: %lu\r\n"
            "Content-Type: %s\r\n\r\n"
            "%s",
            strlen(content), content_type, content);

    p += strlen(p) + 1;

    memory_out_of_use(request_memory, p);

    rendered_response.length = p - rendered_response.content;

    return rendered_response;
}

Response test_get(RequestCtx request_ctx) {
    PersistingData *persisting_data = (PersistingData *)((uint8_t *)request_ctx.persisting_memory->start + sizeof(Memory));
    Dict templates = persisting_data->templates;

    Memory *request_memory = request_ctx.request_memory;

    char *template = find_value("test", templates);

    Response rendered_response = {0};
    char *p = NULL;

    rendered_response.content = p = (char *)memory_in_use(request_memory);
    sprintf(p,
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: %lu\r\n"
            "Content-Type: text/html\r\n\r\n"
            "%s",
            strlen(template), template);

    p += strlen(p) + 1;
    memory_out_of_use(request_memory, p);

    rendered_response.length = p - rendered_response.content;

    return rendered_response;
}

Response home_get(RequestCtx request_ctx) {
    PersistingData *persisting_data = (PersistingData *)((uint8_t *)request_ctx.persisting_memory->start + sizeof(Memory));
    Dict templates = persisting_data->templates;

    Memory *request_memory = request_ctx.request_memory;

    char *template = find_value("home", templates);

    Response rendered_response = {0};
    char *p = NULL;

    Dict user = is_authenticated(request_ctx);
    if (user.start_addr) {
        /** TODO: Add some data to the "home" template that is specific to the user when logged in */

        rendered_response.content = p = (char *)memory_in_use(request_memory);
        sprintf(p,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: %lu\r\n"
                "Content-Type: text/html\r\n\r\n"
                "%s",
                strlen(template), template);

        p += strlen(p) + 1;
        memory_out_of_use(request_memory, p);

        rendered_response.length = p - rendered_response.content;

        return rendered_response;
    }

    rendered_response.content = p = (char *)memory_in_use(request_memory);
    sprintf(p,
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: %lu\r\n"
            "Content-Type: text/html\r\n\r\n"
            "%s",
            strlen(template), template);

    p += strlen(p) + 1;
    memory_out_of_use(request_memory, p);

    rendered_response.length = p - rendered_response.content;

    return rendered_response;
}

Response account_get(RequestCtx request_ctx) {
    PersistingData *persisting_data = (PersistingData *)((uint8_t *)request_ctx.persisting_memory->start + sizeof(Memory));
    Dict templates = persisting_data->templates;

    Memory *request_memory = request_ctx.request_memory;

    Response rendered_response = {0};
    char *p = NULL;

    Dict user = is_authenticated(request_ctx);
    if (user.start_addr) {
        char *template = find_value("profile", templates);
        char *email = find_value("email", user);

        char *rendered_template = NULL;
        rendered_template = p = (char *)memory_in_use(request_memory);
        memcpy(p, template, strlen(template));
        render_val(p, "email", email); /** For profile view */
        render_val(p, "email", email); /** For personal info view */

        p += strlen(p) + 1;
        memory_out_of_use(request_memory, p);

        rendered_response.content = p = (char *)memory_in_use(request_memory);
        sprintf(p,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: %lu\r\n"
                "Content-Type: text/html\r\n\r\n"
                "%s",
                strlen(rendered_template), rendered_template);

        p += strlen(p) + 1;
        memory_out_of_use(request_memory, p);

        rendered_response.length = p - rendered_response.content;

        return rendered_response;
    }

    char *template = find_value("welcome", templates);

    rendered_response.content = p = (char *)memory_in_use(request_memory);
    sprintf(p,
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: %lu\r\n"
            "Content-Type: text/html\r\n\r\n"
            "%s",
            strlen(template), template);

    p += strlen(p) + 1;
    memory_out_of_use(request_memory, p);

    rendered_response.length = p - rendered_response.content;

    return rendered_response;
}

Response login_create_session_post(RequestCtx request_ctx) {
    PersistingData *persisting_data = (PersistingData *)((uint8_t *)request_ctx.persisting_memory->start + sizeof(Memory));
    Dict templates = persisting_data->templates;

    Memory *request_memory = request_ctx.request_memory;

    char *request = request_ctx.request;

    Response rendered_response = {0};
    char *p = NULL;

    String body = find_body(request);
    Dict params = parse_and_decode_params(request_memory, body);

    char *password = find_value("password", params);
    ValidationError bad_password = validate_password(request_memory, password);
    if (bad_password) {
        char *template = find_value("input_validation_error", templates);

        char *rendered_template = NULL;
        rendered_template = p = (char *)memory_in_use(request_memory);

        memcpy(p, template, strlen(template));
        replace_val(p, "id", "password-validation-error");
        render_val(p, "error_message", bad_password);

        p += strlen(p) + 1;
        memory_out_of_use(request_memory, p);

        rendered_response.content = p = (char *)memory_in_use(request_memory);
        sprintf(p,
                "HTTP/1.1 422 Unprocessable Entity\r\n"
                "Content-Length: %lu\r\n"
                "Content-Type: text/html\r\n\r\n"
                "%s",
                strlen(rendered_template), rendered_template);

        p += strlen(p) + 1;
        memory_out_of_use(request_memory, p);

        rendered_response.length = p - rendered_response.content;

        return rendered_response;
    }

    DictArray rows = request_ctx.query(request_ctx.request_memory, request_ctx.db, GET_USER_HASHED_PASSWORD_BY_EMAIL, params);
    ASSERT(rows.length == 1);

    Dict query_output = *(rows.dicts[0]);

    char *hashed_password = find_value("hashed_password", query_output);
    if (argon2i_verify(hashed_password, password, strlen(password)) != ARGON2_OK) {
        char *template = find_value("input_validation_error", templates);

        char *rendered_template = NULL;
        rendered_template = p = (char *)memory_in_use(request_memory);

        memcpy(p, template, strlen(template));
        replace_val(p, "id", "password-validation-error");
        render_val(p, "error_message", "Incorrect password");

        p += strlen(p) + 1;
        memory_out_of_use(request_memory, p);

        rendered_response.content = p = (char *)memory_in_use(request_memory);
        sprintf(p,
                "HTTP/1.1 422 Unprocessable Entity\r\n"
                "Content-Length: %lu\r\n"
                "Content-Type: text/html\r\n\r\n"
                "%s",
                strlen(rendered_template), rendered_template);

        p += strlen(p) + 1;
        memory_out_of_use(request_memory, p);

        rendered_response.length = p - rendered_response.content;

        return rendered_response;
    }

    char *user_id = find_value("user_id", query_output);

    Dict query_params = {0};
    query_params.start_addr = p = (char *)memory_in_use(request_memory);

    strncpy(p, "user_id", strlen("user_id") + 1);
    p += strlen(p) + 1;
    strncpy(p, user_id, strlen(user_id) + 1);
    p += strlen(p) + 1;

    query_params.end_addr = p;
    memory_out_of_use(request_memory, p);

    DictArray rows2 = request_ctx.query(request_ctx.request_memory, request_ctx.db, CREATE_USER_SESSION, query_params);
    ASSERT(rows2.length == 1);

    Dict query_output2 = *(rows2.dicts[0]);

    char *session_id = find_value("session_id", query_output2);
    char *expires_at = find_value("expires_at", query_output2);

    rendered_response.content = p = (char *)memory_in_use(request_memory);
    sprintf(p,
            "HTTP/1.1 200 OK\r\n"
            "Set-Cookie: session_id=%s; Path=/; HttpOnly; Secure; SameSite=Strict; Expires=%s\r\n"
            "Content-Length: 0\r\n"
            "HX-Redirect: /\r\n\r\n",
            session_id, expires_at);

    p += strlen(p) + 1;
    memory_out_of_use(request_memory, p);

    rendered_response.length = p - rendered_response.content;

    return rendered_response;
}

Response register_create_account_post(RequestCtx request_ctx) {
    PersistingData *persisting_data = (PersistingData *)((uint8_t *)request_ctx.persisting_memory->start + sizeof(Memory));
    Dict templates = persisting_data->templates;

    Memory *request_memory = request_ctx.request_memory;

    char *request = request_ctx.request;

    Response rendered_response = {0};
    char *p = NULL;

    String body = find_body(request);
    Dict params = parse_and_decode_params(request_memory, body);

    char *email = find_value("email", params);
    char *password = find_value("password", params);
    char *repeat_password = find_value("repeat-password", params);

    ValidationError bad_email = validate_email(request_memory, email);
    ValidationError bad_password = validate_password(request_memory, password);
    ValidationError bad_repeat_password = validate_repeat_password(request_memory, password, repeat_password);
    if (bad_email || bad_password || bad_repeat_password) {
        char *template = find_value("input_validation_error", templates);

        char *rendered_template = NULL;
        rendered_template = p = (char *)memory_in_use(request_memory);

        if (bad_email) {
            memcpy(p, template, strlen(template));
            replace_val(p, "id", "email-validation-error");
            render_val(p, "error_message", bad_email);
            p += strlen(p);
        }

        if (bad_password) {
            memcpy(p, template, strlen(template));
            replace_val(p, "id", "password-validation-error");
            render_val(p, "error_message", bad_password);
            p += strlen(p);
        }

        if (bad_repeat_password) {
            memcpy(p, template, strlen(template));
            replace_val(p, "id", "repeat-password-validation-error");
            render_val(p, "error_message", bad_repeat_password);
            p += strlen(p);
        }

        p += strlen(p) + 1;
        memory_out_of_use(request_memory, p);

        rendered_response.content = p = (char *)memory_in_use(request_memory);
        sprintf(p,
                "HTTP/1.1 422 Unprocessable Entity\r\n"
                "Content-Length: %lu\r\n"
                "Content-Type: text/html\r\n\r\n"
                "%s",
                strlen(rendered_template), rendered_template);

        p += strlen(p) + 1;
        memory_out_of_use(request_memory, p);

        rendered_response.length = p - rendered_response.content;

        return rendered_response;
    }

    uint8_t salt[SALT_LENGTH];
    memset(salt, 0, SALT_LENGTH);

    ASSERT(generate_salt(salt, SALT_LENGTH) != -1);

    uint32_t t_cost = 2;
    uint32_t m_cost = (1 << 16);
    uint32_t parallelism = 1;

    uint8_t hash[HASH_LENGTH];
    memset(hash, 0, HASH_LENGTH);

    char hashed_password[PASSWORD_BUFFER];
    memset(hashed_password, 0, PASSWORD_BUFFER);
    if (argon2i_hash_raw(t_cost, m_cost, parallelism, password, strlen(password), salt, SALT_LENGTH, hash, HASH_LENGTH) != ARGON2_OK) {
        printf("Failed to create hash from password\n");
        ASSERT(0);
    }

    if (argon2i_hash_encoded(t_cost, m_cost, parallelism, password, strlen(password), salt, SALT_LENGTH, HASH_LENGTH, hashed_password, PASSWORD_BUFFER) != ARGON2_OK) {
        printf("Failed to encode hash\n");
        ASSERT(0);
    }

    if (argon2i_verify(hashed_password, password, strlen(password)) != ARGON2_OK) {
        printf("Failed to verify password\n");
        ASSERT(0);
    }

    Dict query_params = {0};
    query_params.start_addr = p = (char *)memory_in_use(request_memory);

    strncpy(p, "email", strlen("email") + 1);
    p += strlen(p) + 1;
    strncpy(p, email, strlen(email) + 1);
    p += strlen(p) + 1;

    strncpy(p, "hashed_password", strlen("hashed_password") + 1);
    p += strlen(p) + 1;
    strncpy(p, hashed_password, strlen(hashed_password) + 1);
    p += strlen(p) + 1;

    query_params.end_addr = p;
    memory_out_of_use(request_memory, p);

    DictArray rows = request_ctx.query(request_ctx.request_memory, request_ctx.db, CREATE_USER, query_params);
    ASSERT(rows.length == 1);

    Dict query_output = *(rows.dicts[0]);

    char *session_id = find_value("session_id", query_output);
    char *expires_at = find_value("expires_at", query_output);

    rendered_response.content = p = (char *)memory_in_use(request_memory);
    sprintf(p,
            "HTTP/1.1 200 OK\r\n"
            "Set-Cookie: session_id=%s; Path=/; HttpOnly; Secure; SameSite=Strict; Expires=%s\r\n"
            "Content-Length: 0\r\n"
            "HX-Redirect: /\r\n\r\n",
            session_id, expires_at);

    p += strlen(p) + 1;
    memory_out_of_use(request_memory, p);

    rendered_response.length = p - rendered_response.content;

    return rendered_response;
}

Response logout_post(RequestCtx request_ctx) {
    Memory *request_memory = request_ctx.request_memory;

    Response rendered_response = {0};
    char *p = NULL;

    Dict user = is_authenticated(request_ctx);
    if (user.start_addr) {
        DictArray rows = request_ctx.query(request_ctx.request_memory, request_ctx.db, LOGOUT, user);
        ASSERT(rows.length == 1);

        Dict query_output = *(rows.dicts[0]);

        char *is_logged_out = find_value("is_logged_out", query_output);
        ASSERT(*is_logged_out == 't');
    }

    rendered_response.content = p = (char *)memory_in_use(request_memory);
    sprintf(p, "HTTP/1.1 200 OK\r\n"
               "Set-Cookie: session_id=deleted; Path=/; HttpOnly; Secure; SameSite=Strict; Expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n"
               "Content-Length: 0\r\n"
               "HX-Refresh: true\r\n\r\n");

    p += strlen(p) + 1;
    memory_out_of_use(request_memory, p);

    rendered_response.length = p - rendered_response.content;

    return rendered_response;
}

Response auth_check_email_post(RequestCtx request_ctx) {
    PersistingData *persisting_data = (PersistingData *)((uint8_t *)request_ctx.persisting_memory->start + sizeof(Memory));
    Dict templates = persisting_data->templates;

    Memory *request_memory = request_ctx.request_memory;

    char *request = request_ctx.request;

    Response rendered_response = {0};
    char *p = NULL;

    String body = find_body(request);
    Dict params = parse_and_decode_params(request_memory, body);

    char *email = find_value("email", params);
    ValidationError bad_email = validate_email(request_memory, email);
    if (bad_email) {
        char *template = find_value("input_validation_error", templates);

        char *rendered_template = NULL;
        rendered_template = p = (char *)memory_in_use(request_memory);

        memcpy(p, template, strlen(template));
        replace_val(p, "id", "email-validation-error");
        render_val(p, "error_message", bad_email);

        p += strlen(p) + 1;
        memory_out_of_use(request_memory, p);

        rendered_response.content = p = (char *)memory_in_use(request_memory);
        sprintf(p,
                "HTTP/1.1 422 Unprocessable Entity\r\n"
                "Content-Length: %lu\r\n"
                "Content-Type: text/html\r\n\r\n"
                "%s",
                strlen(rendered_template), rendered_template);

        p += strlen(p) + 1;
        memory_out_of_use(request_memory, p);

        rendered_response.length = p - rendered_response.content;

        return rendered_response;
    }

    DictArray rows = request_ctx.query(request_ctx.request_memory, request_ctx.db, IS_USER_REGISTERED, params);
    ASSERT(rows.length == 1);

    Dict query_output = *(rows.dicts[0]);

    char *is_registered = find_value("is_registered", query_output);

    char *template = NULL;
    if (*is_registered == 't') {
        template = find_value("login_view", templates);
    } else {
        template = find_value("register_view", templates);
    }

    char *rendered_template = NULL;
    rendered_template = p = (char *)memory_in_use(request_memory);

    memcpy(p, template, strlen(template));
    replace_val(p, "email", email);

    p += strlen(p) + 1;
    memory_out_of_use(request_memory, p);

    rendered_response.content = p = (char *)memory_in_use(request_memory);
    sprintf(p,
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: %lu\r\n"
            "Content-Type: text/html\r\n\r\n"
            "%s",
            strlen(rendered_template), rendered_template);

    p += strlen(p) + 1;
    memory_out_of_use(request_memory, p);

    rendered_response.length = p - rendered_response.content;

    return rendered_response;
}