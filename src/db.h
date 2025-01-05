#ifndef DB_H
#define DB_H

#include <sqlite3.h>

#include "./app/memory.h"

/* clang-format off */
enum Comands {
    TEST_QUERY,
    IS_USER_AUTHENTICATED,
    IS_USER_REGISTERED,
    GET_USER_HASHED_PASSWORD_BY_EMAIL,
    CREATE_USER_SESSION,
    CREATE_USER,
    LOGOUT
};
/* clang-format on */

DictArray query(Memory *memory, void *db, enum Comands command, Dict query_params);
typedef DictArray (*Query)(Memory *, void *, enum Comands, Dict);

typedef struct {
    Query query;
    void *db;
} DBHandlers;

#endif /* DB_H */