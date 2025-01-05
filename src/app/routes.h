#ifndef ROUTES_H
#define ROUTES_H

#include "../db.h"
#include "./entry.h"
#include "./shared.h"

Response public_get(RequestCtx request_ctx, Dict public_assets, String url);
Response home_get(RequestCtx request_ctx);
Response account_get(RequestCtx request_ctx);
Response profile_get(RequestCtx request_ctx);
Response auth_check_email_post(RequestCtx request_ctx);
Response login_create_session_post(RequestCtx request_ctx);
Response register_create_account_post(RequestCtx request_ctx);
Response logout_post(RequestCtx request_ctx);
Response test_get(RequestCtx request_ctx);
Response view_get(RequestCtx request_ctx, char *view);

#endif /* ROUTES_H */