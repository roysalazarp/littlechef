#ifndef ENTRY_H
#define ENTRY_H

#include "../db/db.h"
#include "../lib/memory/memory.h"

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
    AssetSOA public_assets_soa;
} PersistingData;

void setup_web_server_resources(Memory *persisting_memory, Memory *scratch_memory, AssetSOA asset_list);
Response process_request_and_render_response(RequestCtx request_ctx);

#endif /* ENTRY_H */
