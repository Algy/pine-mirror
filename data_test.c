#include <stdio.h>
#include <stdlib.h>
#include <poll.h>

#include "data.h"

#define mysql_fatal(mysql) do {\
    printf("(MYSQL)%s at [%s:%d]\n", mysql_error(mysql), __FILE__, __LINE__); \
    abort(); \
} while (0)

static int wait_read(int fd, int timeout) {
    struct pollfd pfd = { .fd = fd, .events = POLLIN};
    return poll(&pfd, 1, timeout * 1000);
}

static int wait_write(int fd, int timeout) {
    struct pollfd pfd = { .fd = fd, .events = POLLOUT};
    return poll(&pfd, 1, timeout * 1000);
}

int main(int argc, char** argv) {
    ConnCtx conn;
    conn.wait_read_hook = wait_read;
    conn.wait_write_hook = wait_write;

    MYSQL mysql_mem;
    conn.mysql = mysql_init(&mysql_mem);
    mysql_options(conn.mysql, MYSQL_OPT_NONBLOCK, 0);

    if (!mysql_real_connect(conn.mysql, NULL, "root", NULL, "test", 0, "/tmp/mysql.sock", 0)) {
        mysql_fatal(conn.mysql);
    }
    struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    conn.redis = redisConnectWithTimeout("localhost",6379, timeout);
    if (conn.redis == NULL || conn.redis->err) {
        if (conn.redis) {
            printf("Connection error: %s\n", conn.redis->errstr);
            redisFree(conn.redis);
        } else {
            printf("Connection error: can't allocate redis context\n");
        }
        exit(1);
    }
    char *docnames[] = {"take", "A", "T", "고만해", "고사리", "고만해 미친놈들아", "고사랄라", 
                        "take", "A", "T", "고만해", "고사리", "고만해 미친놈들아", "고사랄라", 
                        "take", "A", "T", "고만해", "고사리", "고만해 미친놈들아", "고사랄라", 
                        "take", "A", "T", "고만해", "고사리", "고만해 미친놈들아", "고사랄라", 
                        "take", "A", "T", "고만해", "고사리", "고만해 미친놈들아", "고사랄라", 
                        "take", "A", "T", "고만해", "고사리", "고만해 미친놈들아", "고사랄라", 
                        "take", "A", "T", "고만해", "고사리", "고만해 미친놈들아", "고사랄라", 
                        "take", "A", "T", "고만해", "고사리", "고만해 미친놈들아", "고사랄라", 
                        "take", "A", "T", "고만해", "고사리", "고만해 미친놈들아", "고사랄라", 
                        "take", "A", "T", "고만해", "고사리", "고만해 미친놈들아", "고사랄라", 
                        "take", "A", "T", "고만해", "고사리", "고만해 미친놈들아", "고사랄라", 
                        "take", "A", "T", "고만해", "고사리", "고만해 미친놈들아", "고사랄라"};
    int name_cnt = sizeof(docnames) / sizeof(docnames[0]);
    bool results[name_cnt];

    clock_t stc = clock();
    documents_exist(&conn, name_cnt, docnames, results);
    clock_t edc = clock();
    printf("Time: %.2lfms\n", (double)(edc - stc) / CLOCKS_PER_SEC * 1000.);
    int idx = 0;
    for (idx = 0; idx < name_cnt; idx++) {
        printf("%s=%s\n", docnames[idx], results[idx]?"true":"false");
    }
    printf("N = %d\n", name_cnt);

    for (idx = 0; idx < 1; idx ++) {
        Document doc;
        Document_init(&doc);
        clock_t stc = clock();
        bool success;
        if ((success = find_document(&conn, "A", &doc))) {
        } else {
            printf("Cannot locate the document\n");
        }
        clock_t edc = clock();
        printf("Time: %.2lfms\n", (double)(edc - stc) / CLOCKS_PER_SEC * 1000.);
        if (success)
            printf("%s\n", doc.source);
        Document_remove(&doc);
    }

    mysql_close(conn.mysql);
    redisFree(conn.redis);
    return 0;
}

