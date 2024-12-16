#include "headers.h"

void create_connection_pool(Dict envs) {
    uint8_t i;

    char *database = find_value("DB_NAME", envs);
    char *user = find_value("DB_USER", envs);
    char *password = find_value("PASSWORD", envs);
    char *host = find_value("HOST", envs);

    const char *keys[] = {"dbname", "user", "password", "host", NULL};
    const char *values[5];
    values[0] = database;
    values[1] = user;
    values[2] = password;
    values[3] = host;
    values[4] = NULL;

    for (i = 0; i < CONNECTION_POOL_SIZE; i++) {
        connection_pool[i].conn = PQconnectStartParams(keys, values, 0);

        assert(PQstatus(connection_pool[i].conn) != CONNECTION_BAD); /** Connection failed */

        PQsetnonblocking(connection_pool[i].conn, 1);

        connection_pool[i].type = DB_SOCKET;
        connection_pool[i].index = i;

        int fd = PQsocket(connection_pool[i].conn);

        event.events = EPOLLOUT;
        event.data.ptr = &(connection_pool[i]);
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    }

    int count = 0;
    while (count < CONNECTION_POOL_SIZE) {
        nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, BLOCK_EXECUTION);
        assert(nfds != -1);

        for (i = 0; i < nfds; i++) {
            FDType *type = (FDType *)events[i].data.ptr;

            if (*type == DB_SOCKET) {
                DBConnection *connection = (DBConnection *)type;
                PostgresPollingStatusType poll_status = PQconnectPoll(connection->conn);

                int fd = PQsocket(connection->conn);

                if (poll_status == PGRES_POLLING_READING) {
                    event.events = EPOLLIN;
                    event.data.ptr = connection;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
                } else if (poll_status == PGRES_POLLING_WRITING) {
                    event.events = EPOLLOUT;
                    event.data.ptr = connection;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
                } else if (poll_status == PGRES_POLLING_OK) {
                    printf("Connection established!\n");
                    count++;
                    break;
                } else {
                    fprintf(stderr, "Connection failed: %s\n", PQerrorMessage(connection->conn));
                    break;
                }
            }
        }
    }
}

/**
 * Searches the connection pool for an available connection (where `client.fd` is 0) and assigns
 * the current request (represented by `scratch_arena`) to it; returns `NULL` if the pool is full.
 *
 * Searches the request queue for an available slot (where `client.fd` is 0) and associates the
 * current request (represented by `scratch_arena`) with it, marking the request as queued;
 * returns a pointer to the assigned queue slot.
 */
DBConnection *get_available_connection(Arena *scratch_arena) {
    RequestCtx *request_ctx = (RequestCtx *)((uint8_t *)scratch_arena + (sizeof(Arena) + sizeof(Socket)));

    int i;

    for (i = 0; i < CONNECTION_POOL_SIZE; i++) {
        if (connection_pool[i].client.fd == 0) {

            connection_pool[i].client.fd = request_ctx->client_socket;
            connection_pool[i].client.request_ctx = request_ctx;

            DBConnection *connection = &(connection_pool[i]);

            return connection;
        }
    }

    for (i = 0; i < MAX_CLIENT_CONNECTIONS; i++) {
        if (queue[i].client.fd == 0) {
            /* Available spot in the queue */

            queue[i].client.fd = request_ctx->client_socket;
            queue[i].client.request_ctx = request_ctx;
            queue[i].client.queued = 1;

            QueuedRequest *queued = &(queue[i]);

            int r = setjmp(queued->client.jmp_buf);
            if (r == 0) {
                longjmp(ctx, 1);
            }

            int index = 1000; /** FIX */

            DBConnection *connection = &(connection_pool[index]);

            return connection;
        }
    }

    assert(0);
}

PGresult *WPQsendQueryParams(DBConnection *connection, const char *command, int nParams, const Oid *paramTypes, const char *const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat) {
    if (PQsendQueryParams(connection->conn, command, nParams, paramTypes, paramValues, paramLengths, paramFormats, resultFormat) == 0) {
        fprintf(stderr, "Query failed to send: %s\n", PQerrorMessage(connection->conn));
        int conn_fd = PQsocket(connection->conn);
        printf("socket: %d", conn_fd);
    }

    int conn_fd = PQsocket(connection->conn);

    event.events = EPOLLIN | EPOLLET;
    event.data.ptr = connection;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn_fd, &event);

    if (connection->client.queued) {
        int r = setjmp(connection->client.jmp_buf);
        if (r == 0) {
            longjmp(db_ctx, 1);
        }
    } else {
        int r = setjmp(connection->client.jmp_buf);
        if (r == 0) {
            longjmp(ctx, 1);
        }
    }

    PGresult *result = get_result(connection);

    return result;
}

/**
 * Encapsulates boilerplate logic for processing query results in an asynchronous
 * PostgreSQL connection. Ensures all input is consumed, waits for the query to finish,
 * and retrieves the first valid result from the connection. The returned `PGresult`
 * must be cleared with `PQclear` after use to avoid memory leaks.
 */
PGresult *get_result(DBConnection *connection) {
    if (!PQconsumeInput(connection->conn)) {
        assert(0);
    }

    while (PQisBusy(connection->conn)) {
        if (!PQconsumeInput(connection->conn)) {
            assert(0);
        }
    }

    PGresult *result = NULL;

    PGresult *ptr = NULL;
    int did_set_ptr = 0;
    while ((ptr = PQgetResult(connection->conn)) != NULL) {
        if (did_set_ptr == 0) {
            result = ptr;
            did_set_ptr = 1;
        }

        if (PQresultStatus(ptr) != PGRES_TUPLES_OK && PQresultStatus(ptr) != PGRES_COMMAND_OK) {
            fprintf(stderr, "Query failed: %s\n", PQerrorMessage(connection->conn));

            assert(0);
        }
    }

    return result;
}

/**
 * Encapsulates boilerplate logic for printing the result of a PostgreSQL query in a formatted table.
 */
void print_query_result(PGresult *query_result) {
    PQprintOpt options = {0};          /* Initialize to zero to avoid garbage values */
    options.header = 1;                /* Print headers */
    options.align = 1;                 /* Align output nicely */
    options.html3 = 0;                 /* Don't print HTML table format */
    options.expanded = 0;              /* Normal table format */
    options.pager = 0;                 /* Don't use pager */
    options.fieldSep = " | ";          /* Field separator */
    options.tableOpt = "border=1";     /* Table option if using HTML */
    options.caption = "Query Results"; /* Caption for the table */

    PQprint(stdout, query_result, &options);
}
