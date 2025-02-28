#include <ctype.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* clang-format off */
#include "./app/shared.h"
#include "./app/utils.h"
#include "./app/memory.h"
#include "./db.h"
/* clang-format on */

char *copy_string(Memory *memory, const char *str) {
    char *buffer = (char *)memory_alloc(memory, strlen(str) + 1);
    memcpy(buffer, str, strlen(str));

    return buffer;
}

enum DayOfWeek { MONDAY = 1, TUESDAY, WEDNESDAY, THURSDAY, FRIDAY, SATURDAY, SUNDAY };
enum Month { JANUARY = 1, FEBRUARY, MARCH, APRIL, MAY, JUNE, JULY, AUGUST, SEPTEMBER, OCTOBER, NOVEMBER, DECEMBER };

int get_day_of_the_week(int day) {
    switch (day) {
        case 1:
            return MONDAY;
        case 2:
            return TUESDAY;
        case 3:
            return WEDNESDAY;
        case 4:
            return THURSDAY;
        case 5:
            return FRIDAY;
        case 6:
            return SATURDAY;
        case 7:
            return SUNDAY;
        default:
            return -1;
    }
}

const char *str_day_of_the_week(int day) {
    switch (day) {
        case MONDAY:
            return "Mon";
        case TUESDAY:
            return "Tue";
        case WEDNESDAY:
            return "Wed";
        case THURSDAY:
            return "Thu";
        case FRIDAY:
            return "Fri";
        case SATURDAY:
            return "Sat";
        case SUNDAY:
            return "Sun";
        default:
            return NULL;
    }
}

int get_month(int month) {
    switch (month) {
        case 1:
            return JANUARY;
        case 2:
            return FEBRUARY;
        case 3:
            return MARCH;
        case 4:
            return APRIL;
        case 5:
            return MAY;
        case 6:
            return JUNE;
        case 7:
            return JULY;
        case 8:
            return AUGUST;
        case 9:
            return SEPTEMBER;
        case 10:
            return OCTOBER;
        case 11:
            return NOVEMBER;
        case 12:
            return DECEMBER;
        default:
            return -1;
    }
}

const char *str_month(int month) {
    switch (month) {
        case JANUARY:
            return "Jan";
        case FEBRUARY:
            return "Feb";
        case MARCH:
            return "Mar";
        case APRIL:
            return "Apr";
        case MAY:
            return "May";
        case JUNE:
            return "Jun";
        case JULY:
            return "Jul";
        case AUGUST:
            return "Aug";
        case SEPTEMBER:
            return "Sep";
        case OCTOBER:
            return "Oct";
        case NOVEMBER:
            return "Nov";
        case DECEMBER:
            return "Dec";
        default:
            return NULL;
    }
}

void query(Memory *memory, QueryHeader *header) {
    const char *sql = NULL;
    sqlite3_stmt *stmt = NULL;

    sqlite3 *db = header->db;

    switch (header->command) {
        case _IsUserAuthenticated: {
            IsUserAuthenticated *helper = (IsUserAuthenticated *)header;

            int session_id_param = helper->query_params.session_id;

            sql = "SELECT u.id, u.email, us.id as sid "
                  "FROM sessions us "
                  "JOIN users u ON u.id = us.user_id "
                  "WHERE us.id = ? AND datetime('now') < us.expires_at;";

            sqlite3_prepare_v2(db, sql, strlen(sql) + 1, &stmt, NULL);
            sqlite3_bind_int(stmt, 1, session_id_param);

            if (sqlite3_step(stmt) != SQLITE_ROW) {
                goto exit;
            }

            int user_id = sqlite3_column_int(stmt, 0);
            helper->result.user_id = user_id;

            const char *email = (const char *)sqlite3_column_text(stmt, 1);
            helper->result.email = copy_string(memory, email);

            sqlite3_finalize(stmt);

            sql = "SELECT photo, name, surname, phone_number, country_id FROM user_info WHERE user_id = ?;";

            sqlite3_prepare_v2(db, sql, strlen(sql) + 1, &stmt, NULL);
            sqlite3_bind_int(stmt, 1, user_id);

            if (sqlite3_step(stmt) != SQLITE_ROW) {
                goto exit;
            }

            const char *photo = (const char *)sqlite3_column_text(stmt, 0);
            helper->result.photo = copy_string(memory, photo);

            const char *name = (const char *)sqlite3_column_text(stmt, 1);
            helper->result.name = copy_string(memory, name);

            const char *surname = (const char *)sqlite3_column_text(stmt, 2);
            helper->result.surname = copy_string(memory, surname);

            const char *phone_number = (const char *)sqlite3_column_text(stmt, 3);
            helper->result.phone_number = copy_string(memory, phone_number);

            int country_id = sqlite3_column_int(stmt, 4);

            sqlite3_finalize(stmt);

            sql = "SELECT name FROM country WHERE id = ?;";

            sqlite3_prepare_v2((sqlite3 *)db, sql, strlen(sql) + 1, &stmt, NULL);
            sqlite3_bind_int(stmt, 1, country_id);

            if (sqlite3_step(stmt) != SQLITE_ROW) {
                goto exit;
            }

            const char *country = (const char *)sqlite3_column_text(stmt, 0);
            helper->result.country = copy_string(memory, country);

            goto exit;
        }
        case _IsUserRegistered: {
            IsUserRegistered *helper = (IsUserRegistered *)header;

            char *email_param = helper->query_params.email;

            sql = "SELECT email FROM users WHERE email = ?;";

            sqlite3_prepare_v2(db, sql, strlen(sql) + 1, &stmt, NULL);
            sqlite3_bind_text(stmt, 1, email_param, strlen(email_param), SQLITE_STATIC);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                helper->result.is_registered = true;
            } else {
                helper->result.is_registered = false;
            }

            goto exit;
        }
        case _GetUserHashedPasswordByEmail: {
            GetUserHashedPasswordByEmail *helper = (GetUserHashedPasswordByEmail *)header;

            char *email_param = helper->query_params.email;

            sql = "SELECT id, password FROM users WHERE email = ?;";

            sqlite3_prepare_v2(db, sql, strlen(sql) + 1, &stmt, NULL);
            sqlite3_bind_text(stmt, 1, email_param, strlen(email_param), SQLITE_STATIC);

            if (sqlite3_step(stmt) != SQLITE_ROW) {
                goto exit;
            }

            int user_id = sqlite3_column_int(stmt, 0);
            helper->result.user_id = user_id;

            const char *hashed_password = (const char *)sqlite3_column_text(stmt, 1);
            helper->result.hashed_password = copy_string(memory, hashed_password);

            goto exit;
        }
        case _CreateUserSession: {
            CreateUserSession *helper = (CreateUserSession *)header;

            int user_id_param = helper->query_params.user_id;

            sql = "INSERT OR REPLACE INTO sessions (user_id, expires_at, updated_at) "
                  "VALUES (?, datetime('now', '+1 hour'), datetime('now')) "
                  "RETURNING id, "
                  "strftime('%d', expires_at) AS day, "
                  "strftime('%m', expires_at) AS month, "
                  "strftime('%Y', expires_at) AS year, "
                  "strftime('%H:%M:%S', expires_at) AS time;";

            sqlite3_prepare_v2(db, sql, strlen(sql) + 1, &stmt, NULL);
            sqlite3_bind_int(stmt, 1, user_id_param);

            if (sqlite3_step(stmt) != SQLITE_ROW) {
                goto exit;
            }

            int session_id = sqlite3_column_int(stmt, 0);
            helper->result.session_id = session_id;

            int day = sqlite3_column_int(stmt, 1);
            int month = sqlite3_column_int(stmt, 2);
            int year = sqlite3_column_int(stmt, 3);
            const unsigned char *time = sqlite3_column_text(stmt, 4);

            const char *day_str = str_day_of_the_week(get_day_of_the_week(day));
            const char *month_str = str_month(get_month(month));

            char *expires_at = (char *)memory_in_use(memory);
            sprintf(expires_at, "%s, %d %s %d %s GTM", day_str, day, month_str, year, time);
            memory_out_of_use(memory, expires_at + strlen(expires_at) + 1);

            helper->result.expires_at = expires_at;

            goto exit;
        }
        case _CreateUser: {
            CreateUser *helper = (CreateUser *)header;

            char *email_param = helper->query_params.email;
            char *hashed_password_param = helper->query_params.hashed_password;

            sql = "INSERT INTO users (email, password) VALUES (?, ?) RETURNING id;";

            sqlite3_prepare_v2((sqlite3 *)db, sql, strlen(sql) + 1, &stmt, NULL);
            sqlite3_bind_text(stmt, 1, email_param, strlen(email_param), SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, hashed_password_param, strlen(hashed_password_param), SQLITE_STATIC);

            if (sqlite3_step(stmt) != SQLITE_ROW) {
                goto exit;
            }

            int user_id = sqlite3_column_int(stmt, 0);

            CreateUserSession helper2 = {0};
            helper2.header.command = _CreateUserSession;
            helper2.header.db = helper->header.db;
            helper2.query_params.user_id = user_id;

            query(memory, (QueryHeader *)&helper2);

            helper->result.session_id = helper2.result.session_id;
            helper->result.expires_at = helper2.result.expires_at;

            goto exit;
        }
        case _Logout: {
            Logout *helper = (Logout *)header;

            int session_id_param = helper->query_params.session_id;

            sql = "DELETE FROM sessions WHERE id = ?;";

            sqlite3_prepare_v2((sqlite3 *)db, sql, strlen(sql) + 1, &stmt, NULL);
            sqlite3_bind_int(stmt, 1, session_id_param);

            if (sqlite3_step(stmt) == SQLITE_DONE) {
                helper->result.is_logged_out = true;
            } else {
                helper->result.is_logged_out = false;
            }

            goto exit;
        }
        case _FindProduct: {
            FindProduct *helper = (FindProduct *)header;

            int product_id_param = helper->query_params.product_id;

            sql = "SELECT "
                  "     p.id, "
                  "     p.name, "
                  "     p.ingredients_list, "
                  "     p.photo, "
                  "     p.price, "
                  "     p.rating, "
                  "     p.chef_id, "
                  "     u.name AS chef_name, "
                  "     u.surname AS chef_surname "
                  "FROM products p "
                  "LEFT JOIN user_info u ON p.chef_id = u.user_id "
                  "WHERE p.id = ?;";

            sqlite3_prepare_v2((sqlite3 *)db, sql, strlen(sql) + 1, &stmt, NULL);
            sqlite3_bind_int(stmt, 1, product_id_param);

            if (sqlite3_step(stmt) != SQLITE_ROW) {
                goto exit;
            }

            int id = sqlite3_column_int(stmt, 0);
            helper->result.id = id;

            const char *name = (const char *)sqlite3_column_text(stmt, 1);
            helper->result.name = copy_string(memory, name);

            const char *ingredients_list = (const char *)sqlite3_column_text(stmt, 2);
            helper->result.ingredients_list = copy_string(memory, ingredients_list);

            const char *photo = (const char *)sqlite3_column_text(stmt, 3);
            helper->result.photo = copy_string(memory, photo);

            double price = sqlite3_column_double(stmt, 4);
            helper->result.price = price;

            double rating = sqlite3_column_double(stmt, 5);
            helper->result.rating = rating;

            int chef_id = sqlite3_column_int(stmt, 6);
            helper->result.chef_id = chef_id;

            const char *chef_name = (const char *)sqlite3_column_text(stmt, 7);
            helper->result.chef_name = copy_string(memory, chef_name);

            const char *chef_surname = (const char *)sqlite3_column_text(stmt, 8);
            helper->result.chef_surname = copy_string(memory, chef_surname);

            goto exit;
        }
        default: {
            return;
        }
    }

exit:
    sqlite3_finalize(stmt);

    return;
}
