#include <string.h>

/* clang-format off */
#include "./utils.h"
#include "./memory.h"
#include "./http_utils.h"
#include "./template_engine.h"
#include "./minifiers.h"
#include "./routes.h"
#include "./entry.h"
#include "../db.h"
/* clang-format on */

#define URL(path) path "\x20"
#define URL_WITH_QUERY(path) path "?"

void setup_web_server_resources(Memory *persisting_memory, Memory *scratch_memory, Dict assets) {
    PersistingData *persisting_data = (PersistingData *)memory_alloc(persisting_memory, sizeof(PersistingData));

    Dict templates = build_html_components(persisting_memory, scratch_memory, assets);
    persisting_data->templates = templates;

    uint8_t size = get_dictionary_size(assets);

    Dict public_assets = {0};
    char *p = NULL;

    public_assets.start_addr = p = (char *)memory_in_use(persisting_memory);
    uint8_t i;
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

Response process_request_and_render_response(RequestCtx request_ctx) {
    PersistingData *persisting_data = (PersistingData *)((uint8_t *)request_ctx.persisting_memory->start + sizeof(Memory));
    Dict templates = persisting_data->templates;
    Dict public_assets = persisting_data->public_assets;

    char *request = request_ctx.request;

    Response response = {0};

    String method = find_http_request_value("METHOD", request);
    String url = find_http_request_value("URL", request);

    if (strncmp(url.start_addr, URL("/.well-known/assetlinks.json"), strlen(URL("/.well-known/assetlinks.json"))) == 0 && strncmp(method.start_addr, "GET", method.length) == 0) {
        String path = {0};

#ifdef DEBUG
        char *str = "/assets/public/android/assetlinks.dev.json";
#else
        char *str = "/assets/public/android/assetlinks.json";
#endif

        path.start_addr = str;
        path.length = strlen(str);

        response = public_get(request_ctx, public_assets, path);
        return response;
    }

    if (strncmp(url.start_addr, "/assets/public", strlen("/assets/public")) == 0 && strncmp(method.start_addr, "GET", method.length) == 0) {
        response = public_get(request_ctx, public_assets, url);
        return response;
    }

    if (strncmp(url.start_addr, URL("/"), strlen(URL("/"))) == 0) {
        if (strncmp(method.start_addr, "GET", method.length) == 0) {
            response = home_get(request_ctx);
            return response;
        }
    }

    if (strncmp(url.start_addr, URL("/account"), strlen(URL("/account"))) == 0) {
        if (strncmp(method.start_addr, "GET", method.length) == 0) {
            response = account_get(request_ctx);
            return response;
        }
    }

    if (strncmp(url.start_addr, URL("/auth-check-email"), strlen(URL("/auth-check-email"))) == 0) {
        if (strncmp(method.start_addr, "POST", method.length) == 0) {
            response = auth_check_email_post(request_ctx);
            return response;
        }
    }

    if (strncmp(url.start_addr, URL("/login/create-session"), strlen(URL("/login/create-session"))) == 0) {
        if (strncmp(method.start_addr, "POST", method.length) == 0) {
            response = login_create_session_post(request_ctx);
            return response;
        }
    }

    if (strncmp(url.start_addr, URL("/register/create-account"), strlen(URL("/register/create-account"))) == 0) {
        if (strncmp(method.start_addr, "POST", method.length) == 0) {
            response = register_create_account_post(request_ctx);
            return response;
        }
    }

    if (strncmp(url.start_addr, URL("/logout"), strlen(URL("/logout"))) == 0) {
        if (strncmp(method.start_addr, "POST", method.length) == 0) {
            response = logout_post(request_ctx);
            return response;
        }
    }

    if (strncmp(url.start_addr, URL("/test"), strlen(URL("/test"))) == 0) {
        if (strncmp(method.start_addr, "GET", method.length) == 0) {
            response = test_get(request_ctx);
            return response;
        }
    }

    response = view_get(request_ctx, "not_found");
    return response;
}