#ifndef ENTRY_H
#define ENTRY_H

#include "../db.h"
#include "memory.h"

typedef struct {
    Memory *persisting_memory;
    Memory *request_memory;
    Query query;
    void *db;
    char *request;
} RequestCtx;

typedef struct {
    KeyValueArray *templates_array;
    KeyValueArray *public_assets_array;
} PersistingData;

void setup_web_server_resources(Memory *persisting_memory, Memory *scratch_memory, KeyValueArray *asset_array);
Response process_request_and_render_response(RequestCtx request_ctx);

#endif /* ENTRY_H */
