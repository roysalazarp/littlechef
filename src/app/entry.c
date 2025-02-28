#include <argon2.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
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

typedef char StrNumber[10];

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

User is_authenticated(RequestCtx request_ctx) {
    char *request = request_ctx.request;

    String cookie = find_http_request_value("Cookie", request);
    if (cookie.start_addr && cookie.length) {
        String sid = find_http_cookie_value("session_id", cookie);
        if (sid.start_addr && sid.length) {
            char sid_str[21];
            memset(sid_str, 0, sizeof(sid_str));
            strncpy(sid_str, sid.start_addr, sid.length);

            Memory *request_memory = request_ctx.request_memory;

            IsUserAuthenticated helper = {0};
            helper.header.command = _IsUserAuthenticated;
            helper.header.db = request_ctx.db;
            helper.query_params.session_id = atoi(sid_str);

            request_ctx.query(request_ctx.request_memory, (QueryHeader *)&helper);

            return helper.result;
        }
    }

    User user = {0};
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

    if (strncmp(request_method, request, request_method_length) == 0) {
        if (method == GET) {
            if (strncmp(request_target, target, strlen(target)) == 0 && request_target[strlen(target)] == '?') {
                return true;
            }
        }

        if (strncmp(request_target, target, strlen(target)) == 0 && request_target[strlen(target)] == ' ') {
            return true;
        }
    }

    return false;
}

Response process_request_and_render_response(RequestCtx request_ctx) {
    PersistingData *persisting_data = (PersistingData *)((u8 *)request_ctx.persisting_memory->start + sizeof(Memory));
    Dict templates = persisting_data->templates;
    Dict public_assets = persisting_data->public_assets;

    Memory *request_memory = request_ctx.request_memory;
    Response rendered_response = {0};
    char *p = NULL;

    User user = {0};

    char *request = request_ctx.request;

    if (match(request, "/.well-known/assetlinks.json", GET)) {
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

    if (match(request, "/", GET)) {
        char *template = find_value("home", templates);

        user = is_authenticated(request_ctx);
        if (user.user_id) {
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

    if (match(request, "/account", GET)) {
        user = is_authenticated(request_ctx);
        if (user.user_id) {
            char *template = find_value("profile", templates);

            char *rendered_template = NULL;

            rendered_template = p = (char *)memory_in_use(request_memory);
            memcpy(p, template, strlen(template));
            render_val(p, "email", user.email); /** For profile view */
            render_val(p, "email", user.email); /** For personal info view */

            replace_val(p, "email", user.email);

            if (user.photo) {
                replace_val(p, "user_avatar_big", user.photo);
                replace_val(p, "user_avatar", user.photo);
            }

            if (user.name) {
                render_val(p, "name", user.name);
                render_val(p, "name", user.name);
                replace_val(p, "firstname", user.name);
            }

            if (user.surname) {
                render_val(p, "surname", user.surname);
                render_val(p, "surname", user.surname);
                replace_val(p, "lastname", user.surname);
            }

            if (user.phone_number) {
                render_val(p, "phone_number", user.phone_number);
                replace_val(p, "phone_number", user.phone_number);
            }

            if (user.country) {
                render_val(p, "country", user.country);
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

    if (match(request, "/auth-check-email", POST)) {
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

        IsUserRegistered helper = {0};
        helper.header.command = _IsUserRegistered;
        helper.header.db = request_ctx.db;
        helper.query_params.email = email;

        request_ctx.query(request_ctx.request_memory, (QueryHeader *)&helper);

        char *template = NULL;
        if (helper.result.is_registered) {
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

    if (match(request, "/login/create-session", POST)) {
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

        char *email = find_value("email", params);

        GetUserHashedPasswordByEmail helper = {0};
        helper.header.command = _GetUserHashedPasswordByEmail;
        helper.header.db = request_ctx.db;
        helper.query_params.email = email;

        request_ctx.query(request_ctx.request_memory, (QueryHeader *)&helper);

        if (argon2i_verify(helper.result.hashed_password, password, strlen(password)) != ARGON2_OK) {
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

        CreateUserSession helper2 = {0};
        helper2.header.command = _CreateUserSession;
        helper2.header.db = request_ctx.db;
        helper2.query_params.user_id = helper.result.user_id;

        request_ctx.query(request_ctx.request_memory, (QueryHeader *)&helper2);

        rendered_response.content = p = (char *)memory_in_use(request_memory);
        sprintf(p,
                "HTTP/1.1 200 OK\r\n"
                "Set-Cookie: session_id=%d; Path=/; HttpOnly; Secure; SameSite=Strict; Expires=%s\r\n"
                "Content-Length: 0\r\n"
                "HX-Redirect: /\r\n\r\n",
                helper2.result.session_id, helper2.result.expires_at);

        p += strlen(p) + 1;
        memory_out_of_use(request_memory, p);

        rendered_response.length = p - rendered_response.content;

        return rendered_response;
    }

    if (match(request, "/register/create-account", POST)) {
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

        CreateUser helper = {0};
        helper.header.command = _CreateUser;
        helper.header.db = request_ctx.db;
        helper.query_params.email = email;
        helper.query_params.hashed_password = hashed_password;

        request_ctx.query(request_ctx.request_memory, (QueryHeader *)&helper);

        rendered_response.content = p = (char *)memory_in_use(request_memory);
        sprintf(p,
                "HTTP/1.1 200 OK\r\n"
                "Set-Cookie: session_id=%d; Path=/; HttpOnly; Secure; SameSite=Strict; Expires=%s\r\n"
                "Content-Length: 0\r\n"
                "HX-Redirect: /\r\n\r\n",
                helper.result.session_id, helper.result.expires_at);

        p += strlen(p) + 1;
        memory_out_of_use(request_memory, p);

        rendered_response.length = p - rendered_response.content;

        return rendered_response;
    }

    if (match(request, "/logout", POST)) {
        user = is_authenticated(request_ctx);
        if (user.user_id) {
            Logout helper = {0};
            helper.header.command = _Logout;
            helper.header.db = request_ctx.db;
            helper.query_params.session_id = user.session_id;

            request_ctx.query(request_ctx.request_memory, (QueryHeader *)&helper);

            ASSERT(helper.result.is_logged_out);
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

    if (match(request, "/product", GET)) {
        int id = 1;

        String query_params = find_http_request_value("QUERY_PARAMS", request);
        Dict params = parse_and_decode_params(request_memory, query_params);

        char *id_str = find_value("id", params);

        if (!id_str) {
            return rendered_response;
        }

        FindProduct helper = {0};
        helper.header.command = _FindProduct;
        helper.header.db = request_ctx.db;
        helper.query_params.product_id = atoi(id_str);

        request_ctx.query(request_ctx.request_memory, (QueryHeader *)&helper);

        char *template = find_value("product_view", templates);

        StrNumber product_id;
        memset(product_id, 0, sizeof(product_id));

        sprintf(product_id, "%d", helper.result.id);

        char *rendered_template = NULL;
        rendered_template = p = (char *)memory_in_use(request_memory);
        memcpy(p, template, strlen(template));

        render_val(p, "product_id", product_id);

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

    if (match(request, "/test", GET)) {
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