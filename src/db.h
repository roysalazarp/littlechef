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
    _Logout,
    _FindProduct
} Comand;
/* clang-format on */

typedef struct {
    void *db;
    Comand command;
    int result_count;
} QueryHeader;

typedef struct {
    int user_id;
    int session_id;
    char *email;
    char *photo;
    char *name;
    char *surname;
    char *phone_number;
    char *country;
} User;

typedef struct {
    QueryHeader header;
    struct {
        int session_id;
    } query_params;
    User result;
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
        int session_id;
        char *expires_at;
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

typedef struct {
    QueryHeader header;
    struct {
        int product_id;
    } query_params;
    struct {
        int id;
        char *name;
        char *ingredients_list;
        char *photo;
        double price;
        double rating;
        int chef_id;
        char *chef_name;
        char *chef_surname;
    } result;
} FindProduct;

void query(Memory *memory, QueryHeader *header);
typedef void (*Query)(Memory *, QueryHeader *);

#endif /* DB_H */