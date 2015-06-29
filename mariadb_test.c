#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sds.h"
#include "mysql.h"

#define fatal(mysql) do {\
    printf("(fatal)%s at [%s:%d]\n", mysql_error(mysql), __FILE__, __LINE__); \
    abort(); \
} while (0)


static sds escape_sql_str(MYSQL *mysql, char* s) {
        sds escaped_s = sdsnewlen(NULL, strlen(s) * 2); 
            mysql_real_escape_string(mysql, escaped_s, s, strlen(s));
                sdsupdatelen(escaped_s);
                    return escaped_s;
}


int main( ) {
    MYSQL *mysql = mysql_init(NULL);
    if (!mysql) 
        fatal(mysql);
    if (!mysql_real_connect(mysql, NULL, "root", NULL, "test", 0, "/tmp/mysql.sock", 0)) {
        mysql_close(mysql);
        fatal(mysql);
    }
    char str[100];
    scanf("%s", str);
    sds escaped_str = escape_sql_str(mysql, str);
    sds s = sdscatprintf(sdsempty(), "SELECT t FROM ang LIMIT 100");
    printf("%s\n", s);
    sdsfree(escaped_str);

    clock_t st = clock();
    if (mysql_query(mysql, s))  {
        fatal(mysql);
    }
    clock_t ed = clock();
    printf("time: %.2lfus\n", (double)(ed - st) / CLOCKS_PER_SEC * 1000 * 1000);
    sdsfree(s);
    MYSQL_RES *res = mysql_use_result(mysql);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        printf("%20s\n\n\n", row[0]);
    }
    mysql_free_result(res);
    mysql_close(mysql);
    return 0;
}
