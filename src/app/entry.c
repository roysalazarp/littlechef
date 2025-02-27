#include <argon2.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* clang-format off */
#include "./profiler.h"
#include "./shared.h"
#include "./utils.h"
#include "./memory.h"
#include "./http_utils.h"
#include "./template_engine.h"
#include "./minifiers.h"
#include "./routes_utils.h"
#include "./entry.h"
#include "../db.h"
/* clang-format on */

int generate_salt(void *salt, size_t salt_size);

#define EMAIL_REGEX "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"

#define HASH_LENGTH 32
#define SALT_LENGTH 16
#define PASSWORD_BUFFER 255 /** Do I need this ??? */

typedef char *ValidationError;

void setup_web_server_resources(Memory *persisting_memory, Memory *scratch_memory, Dict assets) {
    PersistingData *persisting_data = (PersistingData *)memory_alloc(persisting_memory, sizeof(PersistingData));

    Dict templates = build_html_components(persisting_memory, scratch_memory, assets);
    persisting_data->templates = templates;

    u8 size = get_dictionary_size(assets);

    Dict public_assets = {0};
    char *p = NULL;

    public_assets.start_addr = p = (char *)memory_in_use(persisting_memory);
    u8 i;
    for (i = 0; i < size; i++) {
        KV asset = get_key_value(assets, i);

        /* NOT interested in html files */
        if (strncmp(asset.k + strlen(asset.k) - strlen(".html"), ".html", strlen(".html")) == 0) {
            continue;
        }

        strncpy(p, asset.k, strlen(asset.k) + 1);
        p += strlen(p) + 1;

        strncpy(p, asset.v, strlen(asset.v) + 1);
        if (strncmp(asset.k + strlen(asset.k) - strlen(".js"), ".js", strlen(".js")) == 0) {
            js_minify(p);
        }

        p += strlen(p) + 1;
    }

    public_assets.end_addr = p - 1;
    memory_out_of_use(persisting_memory, p);

    persisting_data->public_assets = public_assets;
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

            DictArray rows = request_ctx.query(request_ctx.request_memory, request_ctx.db, _IsUserAuthenticated, params);
            if (rows.length == 1) {
                user = *(rows.dicts[0]);
            }

            return user;
        }
    }

    return user;
}

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

Response view_get(RequestCtx request_ctx, char *view) {
    PersistingData *persisting_data = (PersistingData *)((u8 *)request_ctx.persisting_memory->start + sizeof(Memory));
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

typedef enum { GET, POST, PATCH, DELETE } HTTPMethods;

boolean match(const char *request, const char *target, HTTPMethods method) {
    const char *methods[] = {"GET", "POST", "PATCH", "DELETE"};

    const char *request_method = methods[method];
    size_t request_method_length = strlen(methods[method]);

    const char *request_target = request + request_method_length + /* space character */ 1;

    if (strncmp(request_method, request, request_method_length) == 0 && strncmp(request_target, target, strlen(target)) == 0) {
        return true;
    }

    return false;
}

#define MATCH(request, target, method) match(request, target "\x20", method)

Response process_request_and_render_response(RequestCtx request_ctx) {
    PersistingData *persisting_data = (PersistingData *)((u8 *)request_ctx.persisting_memory->start + sizeof(Memory));
    Dict templates = persisting_data->templates;
    Dict public_assets = persisting_data->public_assets;

    Memory *request_memory = request_ctx.request_memory;
    Response rendered_response = {0};
    char *p = NULL;

    Dict user = {0};

    char *request = request_ctx.request;

    if (MATCH(request, "/.well-known/assetlinks.json", GET)) {
        String path = {0};

#ifdef DEBUG
        char *str = "/assets/public/android/assetlinks.dev.json";
#else
        char *str = "/assets/public/android/assetlinks.json";
#endif

        path.start_addr = str;
        path.length = strlen(str);

        rendered_response = public_get(request_ctx, public_assets, path);
        return rendered_response;
    }

    String method = find_http_request_value("METHOD", request);
    String url = find_http_request_value("URL", request);

    if (strncmp(url.start_addr, "/assets/public", strlen("/assets/public")) == 0 && strncmp(method.start_addr, "GET", method.length) == 0) {
        rendered_response = public_get(request_ctx, public_assets, url);
        return rendered_response;
    }

    if (MATCH(request, "/", GET)) {
        char *template = find_value("home", templates);

        user = is_authenticated(request_ctx);
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

    if (MATCH(request, "/account", GET)) {

        TIME_BLOCK("is_authenticated") {
            /**/
            user = is_authenticated(request_ctx);
        }

        if (user.start_addr) {
            char *template = find_value("profile", templates);

            char *email = find_value("email", user);
            char *photo = find_value("photo", user);
            char *name = find_value("name", user);
            char *surname = find_value("surname", user);
            char *phone_number = find_value("phone_number", user);
            char *country = find_value("country", user);

            char *rendered_template = NULL;

            TIME_BLOCK("rendering account response") {
                rendered_template = p = (char *)memory_in_use(request_memory);
                memcpy(p, template, strlen(template));
                render_val(p, "email", email); /** For profile view */
                render_val(p, "email", email); /** For personal info view */

                replace_val(p, "email", email);

                if (photo) {
                    replace_val(p, "user_avatar_big", photo);
                    replace_val(p, "user_avatar", photo);
                }

                if (name) {
                    render_val(p, "name", name);
                    render_val(p, "name", name);
                    replace_val(p, "firstname", name);
                }

                if (surname) {
                    render_val(p, "surname", surname);
                    render_val(p, "surname", surname);
                    replace_val(p, "lastname", surname);
                }

                if (phone_number) {
                    render_val(p, "phone_number", phone_number);
                    replace_val(p, "phone_number", phone_number);
                }

                if (country) {
                    render_val(p, "country", country);
                }

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
            }

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

    if (MATCH(request, "/auth-check-email", POST)) {
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

        DictArray rows = request_ctx.query(request_ctx.request_memory, request_ctx.db, _IsUserRegistered, params);
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

    if (MATCH(request, "/login/create-session", POST)) {
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

        DictArray rows = request_ctx.query(request_ctx.request_memory, request_ctx.db, _GetUserHashedPasswordByEmail, params);
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

        DictArray rows2 = request_ctx.query(request_ctx.request_memory, request_ctx.db, _CreateUserSession, query_params);
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

    if (MATCH(request, "/register/create-account", POST)) {
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

        u8 salt[SALT_LENGTH];
        memset(salt, 0, SALT_LENGTH);

        ASSERT(generate_salt(salt, SALT_LENGTH) != -1);

        u32 t_cost = 2;
        u32 m_cost = (1 << 16);
        u32 parallelism = 1;

        u8 hash[HASH_LENGTH];
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

        DictArray rows = request_ctx.query(request_ctx.request_memory, request_ctx.db, _CreateUser, query_params);
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

    if (MATCH(request, "/logout", POST)) {
        user = is_authenticated(request_ctx);
        if (user.start_addr) {
            DictArray rows = request_ctx.query(request_ctx.request_memory, request_ctx.db, _Logout, user);
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

    if (MATCH(request, "/test", GET)) {
        char *template = find_value("test", templates);

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

    rendered_response = view_get(request_ctx, "not_found");
    return rendered_response;
}