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

DictArray query(Memory *memory, void *db, enum Comands command, Dict query_params) {
    DictArray rows = {0};
    sqlite3_stmt *stmt = NULL;
    char *p = NULL;
    int rc = 0;

    switch (command) {
        case TEST_QUERY: {
            int limit = 2;
            const char *sql_test = "SELECT id, email FROM users WHERE id < ? ORDER BY id ASC LIMIT ?;";

            rc = sqlite3_prepare_v2((sqlite3 *)db, sql_test, strlen(sql_test) + 1, &stmt, NULL);
            if (rc != SQLITE_OK) {
                ASSERT(0);
                return rows;
            }

            int max_id = 3;

            rc = sqlite3_bind_int(stmt, 1, max_id);
            if (rc != SQLITE_OK) {
                ASSERT(0);
                return rows;
            }

            rc = sqlite3_bind_int(stmt, 2, limit);
            if (rc != SQLITE_OK) {
                ASSERT(0);
                return rows;
            }

            rows.dicts = (Dict **)memory_alloc(memory, limit * sizeof(Dict *));

            int count = 0;
            p = (char *)memory_in_use(memory);
            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                Dict *dict = (Dict *)p;
                p += sizeof(Dict);
                dict->start_addr = p;

                rows.length = count + 1;
                rows.dicts[count] = dict;

                int user_id = sqlite3_column_int(stmt, 0);
                strncpy(p, "user_id", strlen("user_id") + 1);
                p += strlen(p) + 1;
                sprintf(p, "%d", user_id);
                p += strlen(p) + 1;

                const char *email = (const char *)sqlite3_column_text(stmt, 1);
                strncpy(p, "email", strlen("email") + 1);
                p += strlen(p) + 1;
                strncpy(p, email, strlen(email) + 1);
                p += strlen(p) + 1;

                dict->end_addr = p;

                count++;
            }
            memory_out_of_use(memory, p);

            sqlite3_finalize(stmt);

            return rows;
        }
        case IS_USER_AUTHENTICATED: {
            const char *session_id = find_value("session_id", query_params);

            const char *sql = "SELECT u.id, u.email, us.id as sid "
                              "FROM users_sessions us "
                              "JOIN users u ON u.id = us.user_id "
                              "WHERE us.id = ? AND datetime('now') < us.expires_at;";

            rc = sqlite3_prepare_v2((sqlite3 *)db, sql, strlen(sql) + 1, &stmt, NULL);
            if (rc != SQLITE_OK) {
                ASSERT(0);
                return rows;
            }

            rc = sqlite3_bind_int(stmt, 1, atoi(session_id));
            if (rc != SQLITE_OK) {
                ASSERT(0);
                return rows;
            }

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                rows.dicts = (Dict **)memory_alloc(memory, 1 * sizeof(Dict *));

                p = (char *)memory_in_use(memory);
                Dict *dict = (Dict *)p;
                p += sizeof(Dict);
                dict->start_addr = p;

                rows.length = 1;
                rows.dicts[0] = dict;

                int user_id = sqlite3_column_int(stmt, 0);
                strncpy(p, "user_id", strlen("user_id") + 1);
                p += strlen(p) + 1;
                sprintf(p, "%d", user_id);
                p += strlen(p) + 1;

                const char *email = (const char *)sqlite3_column_text(stmt, 1);
                strncpy(p, "email", strlen("email") + 1);
                p += strlen(p) + 1;
                strncpy(p, email, strlen(email) + 1);
                p += strlen(p) + 1;

                strncpy(p, "session_id", strlen("session_id") + 1);
                p += strlen(p) + 1;
                sprintf(p, "%d", sqlite3_column_int(stmt, 2));
                p += strlen(p) + 1;

                dict->end_addr = p;
                memory_out_of_use(memory, p);
            }

            sqlite3_finalize(stmt);

            return rows;
        }
        case IS_USER_REGISTERED: {
            const char *email = find_value("email", query_params);

            const char *sql = "SELECT email FROM users WHERE email = ?;";

            rc = sqlite3_prepare_v2((sqlite3 *)db, sql, strlen(sql) + 1, &stmt, NULL);
            if (rc != SQLITE_OK) {
                ASSERT(0);
                return rows;
            }

            rc = sqlite3_bind_text(stmt, 1, email, strlen(email), SQLITE_STATIC);
            if (rc != SQLITE_OK) {
                ASSERT(0);
                return rows;
            }

            rows.dicts = (Dict **)memory_alloc(memory, 1 * sizeof(Dict *));

            p = (char *)memory_in_use(memory);

            Dict *dict = (Dict *)p;
            p += sizeof(Dict);
            dict->start_addr = p;

            rows.length = 1;
            rows.dicts[0] = dict;

            strncpy(p, "is_registered", strlen("is_registered") + 1);
            p += strlen(p) + 1;

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                *p = 't';
            } else {
                *p = 'f';
            }

            p += sizeof(char) + 1;
            dict->end_addr = p;

            memory_out_of_use(memory, p);

            sqlite3_finalize(stmt);

            return rows;
        }
        case GET_USER_HASHED_PASSWORD_BY_EMAIL: {
            const char *email = find_value("email", query_params);

            const char *sql = "SELECT id, password FROM users WHERE email = ?;";

            rc = sqlite3_prepare_v2((sqlite3 *)db, sql, strlen(sql) + 1, &stmt, NULL);
            if (rc != SQLITE_OK) {
                ASSERT(0);
                return rows;
            }

            rc = sqlite3_bind_text(stmt, 1, email, strlen(email), SQLITE_STATIC);
            if (rc != SQLITE_OK) {
                ASSERT(0);
                return rows;
            }

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                rows.dicts = (Dict **)memory_alloc(memory, 1 * sizeof(Dict *));

                p = (char *)memory_in_use(memory);
                Dict *dict = (Dict *)p;
                p += sizeof(Dict);
                dict->start_addr = p;

                rows.length = 1;
                rows.dicts[0] = dict;

                int user_id = sqlite3_column_int(stmt, 0);
                strncpy(p, "user_id", strlen("user_id") + 1);
                p += strlen(p) + 1;
                sprintf(p, "%d", user_id);
                p += strlen(p) + 1;

                const char *hashed_password = (const char *)sqlite3_column_text(stmt, 1);
                strncpy(p, "hashed_password", strlen("hashed_password") + 1);
                p += strlen(p) + 1;
                strncpy(p, hashed_password, strlen(hashed_password) + 1);
                p += strlen(p) + 1;

                dict->end_addr = p;
                memory_out_of_use(memory, p);
            }

            sqlite3_finalize(stmt);

            return rows;
        }
        case CREATE_USER_SESSION: {
            char *user_id = find_value("user_id", query_params);

            const char *sql = "INSERT OR REPLACE INTO users_sessions (user_id, expires_at, updated_at) "
                              "VALUES (?, datetime('now', '+1 hour'), datetime('now')) "
                              "RETURNING id, "
                              "strftime('%d', expires_at) AS day, "
                              "strftime('%m', expires_at) AS month, "
                              "strftime('%Y', expires_at) AS year, "
                              "strftime('%H:%M:%S', expires_at) AS time;";

            rc = sqlite3_prepare_v2((sqlite3 *)db, sql, strlen(sql) + 1, &stmt, NULL);
            if (rc != SQLITE_OK) {
                ASSERT(0);
                return rows;
            }

            rc = sqlite3_bind_int(stmt, 1, atoi(user_id));
            if (rc != SQLITE_OK) {
                ASSERT(0);
                return rows;
            }

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                rows.dicts = (Dict **)memory_alloc(memory, 1 * sizeof(Dict *));

                p = (char *)memory_in_use(memory);
                Dict *dict = (Dict *)p;
                p += sizeof(Dict);
                dict->start_addr = p;

                rows.length = 1;
                rows.dicts[0] = dict;

                int session_id = sqlite3_column_int(stmt, 0);
                int day = sqlite3_column_int(stmt, 1);
                int month = sqlite3_column_int(stmt, 2);
                int year = sqlite3_column_int(stmt, 3);
                const unsigned char *time = sqlite3_column_text(stmt, 4);

                const char *day_str = str_day_of_the_week(get_day_of_the_week(day));
                const char *month_str = str_month(get_month(month));

                strncpy(p, "session_id", strlen("session_id") + 1);
                p += strlen(p) + 1;
                sprintf(p, "%d", session_id);
                p += strlen(p) + 1;

                strncpy(p, "expires_at", strlen("expires_at") + 1);
                p += strlen(p) + 1;
                sprintf(p, "%s, %d %s %d %s GTM", day_str, day, month_str, year, time);
                p += strlen(p) + 1;

                dict->end_addr = p;
                memory_out_of_use(memory, p);
            }

            sqlite3_finalize(stmt);

            return rows;
        }
        case CREATE_USER: {
            const char *email = find_value("email", query_params);
            const char *hashed_password = find_value("hashed_password", query_params);

            const char *sql = "INSERT INTO users (email, password) VALUES (?, ?) RETURNING id;";

            rc = sqlite3_prepare_v2((sqlite3 *)db, sql, strlen(sql) + 1, &stmt, NULL);
            if (rc != SQLITE_OK) {
                ASSERT(0);
                return rows;
            }

            rc = sqlite3_bind_text(stmt, 1, email, strlen(email), SQLITE_STATIC);
            if (rc != SQLITE_OK) {
                ASSERT(0);
                return rows;
            }

            rc = sqlite3_bind_text(stmt, 2, hashed_password, strlen(hashed_password), SQLITE_STATIC);
            if (rc != SQLITE_OK) {
                ASSERT(0);
                return rows;
            }

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                Dict query_params2 = {0};

                query_params2.start_addr = p = (char *)memory_in_use(memory);

                int user_id = sqlite3_column_int(stmt, 0);
                strncpy(p, "user_id", strlen("user_id") + 1);
                p += strlen(p) + 1;
                sprintf(p, "%d", user_id);
                p += strlen(p) + 1;

                query_params2.end_addr = p;
                memory_out_of_use(memory, p);

                rows = query(memory, db, CREATE_USER_SESSION, query_params2);
            }

            sqlite3_finalize(stmt);

            return rows;
        }
        case LOGOUT: {
            const char *session_id = find_value("session_id", query_params);

            const char *sql = "DELETE FROM users_sessions WHERE id = ?;";

            rc = sqlite3_prepare_v2((sqlite3 *)db, sql, strlen(sql) + 1, &stmt, NULL);
            if (rc != SQLITE_OK) {
                ASSERT(0);
                return rows;
            }

            rc = sqlite3_bind_int(stmt, 1, atoi(session_id));
            if (rc != SQLITE_OK) {
                ASSERT(0);
                return rows;
            }

            rows.dicts = (Dict **)memory_alloc(memory, 1 * sizeof(Dict *));

            p = (char *)memory_in_use(memory);

            Dict *dict = (Dict *)p;
            p += sizeof(Dict);
            dict->start_addr = p;

            rows.length = 1;
            rows.dicts[0] = dict;

            strncpy(p, "is_logged_out", strlen("is_logged_out") + 1);
            p += strlen(p) + 1;

            if (sqlite3_step(stmt) == SQLITE_DONE) {
                *p = 't';
            } else {
                *p = 'f';
            }

            p += sizeof(char) + 1;
            dict->end_addr = p;

            memory_out_of_use(memory, p);

            sqlite3_finalize(stmt);

            return rows;
        }
        default: {
            return rows;
        }
    }
}
