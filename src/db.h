#ifndef DB_H
#define DB_H

#include <sqlite3.h>

#include "./app/memory.h"

/* clang-format off */
typedef enum {
    _TestQuery,
    _IsUserAuthenticated,
    _IsUserRegistered,
    _GetUserHashedPasswordByEmail,
    _CreateUserSession,
    _CreateUser,
    _Logout
} Comand;
/* clang-format on */

typedef struct {
    void *db;
    Comand command;
    int result_count;
} QueryHeader;

typedef struct {
    QueryHeader header;
    struct {
        int session_id;
    } query_params;
    struct {
        int user_id;
        int session_id;
        char *email;
        char *photo;
        char *name;
        char *surname;
        char *phone_number;
        char *country;
    } result;
} IsUserAuthenticated;

typedef struct {
    QueryHeader header;
    struct {
        char *email;
    } query_params;
    struct {
        boolean is_registered;
    } result;
} IsUserRegistered;

typedef struct {
    QueryHeader header;
    struct {
        char *email;
    } query_params;
    struct {
        int user_id;
        char *hashed_password;
    } result;
} GetUserHashedPasswordByEmail;

typedef struct {
    QueryHeader header;
    struct {
        int user_id;
    } query_params;
    struct {
        int session_id;
        char *expires_at;
    } result;
} CreateUserSession;

typedef struct {
    QueryHeader header;
    struct {
        char *email;
        char *hashed_password;
    } query_params;
    struct {
        int user_id;
    } result;
} CreateUser;

typedef struct {
    QueryHeader header;
    struct {
        int session_id;
    } query_params;
    struct {
        boolean is_logged_out;
    } result;
} Logout;

void query2(Memory *memory, QueryHeader *header);
typedef void (*Query2)(Memory *, QueryHeader *);

DictArray query(Memory *memory, void *db, Comand command, Dict query_params);
typedef DictArray (*Query)(Memory *, void *, Comand, Dict);

#endif /* DB_H */