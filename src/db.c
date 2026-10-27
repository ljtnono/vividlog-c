/*
 * db.c — vividlog/storage.py 的 C 对照实现
 *
 * 教学点（对照 Python 版阅读）：
 *   Python                        C
 *   --------------------------    ----------------------------------------
 *   sqlite3.connect(path)         sqlite3_open(path, &db)
 *   conn.execute(sql, params)     sqlite3_prepare_v2 + bind + step + finalize
 *   conn.executescript(sql)       sqlite3_exec（不支持参数绑定）
 *   row["token"]                  sqlite3_column_text(stmt, 0)
 *   finally: conn.close()         sqlite3_close(db)（我们用 goto cleanup 保证）
 *   datetime.now().isoformat()    now_iso8601()（见 util.c）
 *
 * 注意：sqlite3_column_text 返回的指针由 SQLite 持有，
 * 用 xstrdup 拷贝一份再返回给调用者——这是"所有权"的第一次实战。
 */
#include "db.h"
#include "util.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define DB_NAME "vividlog.db"

/* ---- 数据库路径（对应 _get_db_path） ---- */

static const char *get_db_path(void)
{
    /* 对应 Python 的 _get_db_path()，三分支显式拼接，不玩花样 */
    static char path[1024];

    if (getenv("LOCALAPPDATA") && *getenv("LOCALAPPDATA"))
        snprintf(path, sizeof(path), "%s\\VividLog", getenv("LOCALAPPDATA"));
    else if (getenv("XDG_DATA_HOME") && *getenv("XDG_DATA_HOME"))
        snprintf(path, sizeof(path), "%s/VividLog", getenv("XDG_DATA_HOME"));
    else if (getenv("HOME") && *getenv("HOME"))
        snprintf(path, sizeof(path), "%s/.local/share/VividLog", getenv("HOME"));
    else
        return DB_NAME;

    /* 等价 Python 的 data_dir.mkdir(parents=True, exist_ok=True) */
    for (char *p = path + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(path, 0755);
            *p = '/';
        }
    }
    mkdir(path, 0755);

    size_t len = strlen(path);
    snprintf(path + len, sizeof(path) - len, "/%s", DB_NAME);
    return path;
}

/* ---- 连接 ---- */

static int open_db(sqlite3 **out_db)
{
    int rc = sqlite3_open(get_db_path(), out_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "打开数据库失败: %s\n", sqlite3_errmsg(*out_db));
        sqlite3_close(*out_db);
        *out_db = NULL;
        return -1;
    }
    return 0;
}

/* ---- 建表（对应 init_db） ---- */

int db_init(void)
{
    static const char *schema =
        "CREATE TABLE IF NOT EXISTS token ("
        "    id INTEGER PRIMARY KEY CHECK (id = 1),"
        "    token TEXT NOT NULL,"
        "    updated_at TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS config ("
        "    id INTEGER PRIMARY KEY CHECK (id = 1),"
        "    data TEXT NOT NULL,"
        "    updated_at TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS log ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    created_at TEXT NOT NULL,"
        "    level TEXT NOT NULL,"
        "    message TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS submit_record ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    created_at TEXT NOT NULL,"
        "    date TEXT NOT NULL,"
        "    project_id TEXT NOT NULL,"
        "    project_name TEXT,"
        "    timeslot REAL NOT NULL,"
        "    note TEXT,"
        "    status TEXT NOT NULL,"
        "    response TEXT"
        ");";

    sqlite3 *db;
    char *errmsg = NULL;
    int ret = -1;

    if (open_db(&db) != 0)
        return -1;

    if (sqlite3_exec(db, schema, NULL, NULL, &errmsg) != SQLITE_OK) {
        fprintf(stderr, "建表失败: %s\n", errmsg);
        sqlite3_free(errmsg);
        goto cleanup;
    }
    ret = 0;

cleanup:
    sqlite3_close(db);
    return ret;
}

int db_clear(void)
{
    static const char *sql =
        "DELETE FROM token;"
        "DELETE FROM config;"
        "DELETE FROM log;"
        "DELETE FROM submit_record;";

    sqlite3 *db;
    char *errmsg = NULL;
    int ret = -1;

    if (open_db(&db) != 0)
        return -1;

    if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK) {
        fprintf(stderr, "清空失败: %s\n", errmsg);
        sqlite3_free(errmsg);
        goto cleanup;
    }
    ret = 0;

cleanup:
    sqlite3_close(db);
    return ret;
}

/* ---- token（对应 save_token / load_token） ---- */

int db_save_token(const char *token)
{
    static const char *upsert =
        "INSERT INTO token (id, token, updated_at) VALUES (1, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET token=excluded.token, "
        "updated_at=excluded.updated_at";

    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    char *now = now_iso8601();
    int ret = -1;

    if (open_db(&db) != 0)
        goto cleanup;

    if (sqlite3_prepare_v2(db, upsert, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL 错误: %s\n", sqlite3_errmsg(db));
        goto cleanup;
    }
    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, now, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "写入 token 失败: %s\n", sqlite3_errmsg(db));
        goto cleanup;
    }
    ret = 0;

cleanup:
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    free(now);
    return ret;
}

char *db_load_token(void)
{
    static const char *sql = "SELECT token FROM token WHERE id = 1";

    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    char *result = NULL;
    int rc;

    if (open_db(&db) != 0)
        goto cleanup;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL 错误: %s\n", sqlite3_errmsg(db));
        goto cleanup;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
        result = xstrdup((const char *)sqlite3_column_text(stmt, 0));
    /* rc == SQLITE_DONE 表示无记录，保持 result = NULL 即可 */

cleanup:
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;   /* 调用者 free；未保存时为 NULL */
}

/* ---- config（对应 save_config / load_config，JSON 原文存取） ---- */

int db_save_config(const char *json_data)
{
    static const char *upsert =
        "INSERT INTO config (id, data, updated_at) VALUES (1, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET data=excluded.data, "
        "updated_at=excluded.updated_at";

    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    char *now = now_iso8601();
    int ret = -1;

    if (open_db(&db) != 0)
        goto cleanup;

    if (sqlite3_prepare_v2(db, upsert, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL 错误: %s\n", sqlite3_errmsg(db));
        goto cleanup;
    }
    sqlite3_bind_text(stmt, 1, json_data, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, now, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "写入 config 失败: %s\n", sqlite3_errmsg(db));
        goto cleanup;
    }
    ret = 0;

cleanup:
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    free(now);
    return ret;
}

char *db_load_config(void)
{
    static const char *sql = "SELECT data FROM config WHERE id = 1";

    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    char *result = NULL;

    if (open_db(&db) != 0)
        goto cleanup;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL 错误: %s\n", sqlite3_errmsg(db));
        goto cleanup;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = xstrdup((const char *)sqlite3_column_text(stmt, 0));

cleanup:
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}

/* ---- log（对应 insert_log / get_logs） ---- */

int db_insert_log(const char *level, const char *message)
{
    static const char *sql =
        "INSERT INTO log (created_at, level, message) VALUES (?, ?, ?)";

    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    char *now = now_iso8601();
    int ret = -1;

    if (open_db(&db) != 0)
        goto cleanup;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL 错误: %s\n", sqlite3_errmsg(db));
        goto cleanup;
    }
    sqlite3_bind_text(stmt, 1, now, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, level, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, message, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "写入日志失败: %s\n", sqlite3_errmsg(db));
        goto cleanup;
    }
    ret = 0;

cleanup:
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    free(now);
    return ret;
}

int db_get_logs(int limit, struct log_row **out_rows, int *out_count)
{
    static const char *sql =
        "SELECT created_at, level, message FROM log "
        "ORDER BY id DESC LIMIT ?";

    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    struct log_row *rows = NULL;
    int count = 0, cap = 0, ret = -1;

    *out_rows = NULL;
    *out_count = 0;

    if (open_db(&db) != 0)
        goto cleanup;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL 错误: %s\n", sqlite3_errmsg(db));
        goto cleanup;
    }
    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count == cap) {   /* 动态数组扩容，对应 Python list 的自动增长 */
            cap = cap ? cap * 2 : 16;
            rows = realloc(rows, (size_t)cap * sizeof(*rows));
            if (!rows)
                die("内存不足");
        }
        rows[count].created_at = xstrdup((const char *)sqlite3_column_text(stmt, 0));
        rows[count].level      = xstrdup((const char *)sqlite3_column_text(stmt, 1));
        rows[count].message    = xstrdup((const char *)sqlite3_column_text(stmt, 2));
        count++;
    }

    *out_rows = rows;
    *out_count = count;
    ret = 0;
    rows = NULL;   /* 所有权移交给调用者 */

cleanup:
    if (rows)
        db_free_logs(rows, count);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ret;
}

void db_free_logs(struct log_row *rows, int count)
{
    if (!rows)
        return;
    for (int i = 0; i < count; i++) {
        free(rows[i].created_at);
        free(rows[i].level);
        free(rows[i].message);
    }
    free(rows);
}

/* ---- submit_record（对应 insert_submit_record / get_submit_records） ---- */

int db_insert_submit_record(const char *date, const char *project_id,
                            const char *project_name, double timeslot,
                            const char *note, const char *status,
                            const char *response)
{
    static const char *sql =
        "INSERT INTO submit_record "
        "(created_at, date, project_id, project_name, timeslot, note, "
        " status, response) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";

    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    char *now = now_iso8601();
    int ret = -1;

    if (open_db(&db) != 0)
        goto cleanup;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL 错误: %s\n", sqlite3_errmsg(db));
        goto cleanup;
    }
    sqlite3_bind_text(stmt, 1, now, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, project_id, -1, SQLITE_TRANSIENT);
    /* 可空字段：传 NULL 而不是空字符串，对应 Python 的 None */
    if (project_name)
        sqlite3_bind_text(stmt, 4, project_name, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 4);
    sqlite3_bind_double(stmt, 5, timeslot);
    if (note)
        sqlite3_bind_text(stmt, 6, note, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 6);
    sqlite3_bind_text(stmt, 7, status, -1, SQLITE_TRANSIENT);
    if (response)
        sqlite3_bind_text(stmt, 8, response, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 8);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "写入提交记录失败: %s\n", sqlite3_errmsg(db));
        goto cleanup;
    }
    ret = 0;

cleanup:
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    free(now);
    return ret;
}

/* 把可空文本列拷贝成 char*（NULL 列保持 NULL） */
static char *dup_col_text(sqlite3_stmt *stmt, int col)
{
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL)
        return NULL;
    return xstrdup((const char *)sqlite3_column_text(stmt, col));
}

int db_get_submit_records(const char *start_date, const char *end_date,
                          int limit, struct submit_record **out_rows,
                          int *out_count)
{
    /* 对应 Python 里拼 WHERE 子句 + params 列表的做法 */
    char sql[512];
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    struct submit_record *rows = NULL;
    int count = 0, cap = 0, idx = 1, ret = -1;

    *out_rows = NULL;
    *out_count = 0;

    strcpy(sql, "SELECT id, created_at, date, project_id, project_name, "
                "timeslot, note, status, response FROM submit_record "
                "WHERE 1=1");
    if (start_date)
        strcat(sql, " AND date >= ?");
    if (end_date)
        strcat(sql, " AND date <= ?");
    strcat(sql, " ORDER BY id DESC LIMIT ?");

    if (open_db(&db) != 0)
        goto cleanup;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL 错误: %s\n", sqlite3_errmsg(db));
        goto cleanup;
    }
    if (start_date)
        sqlite3_bind_text(stmt, idx++, start_date, -1, SQLITE_TRANSIENT);
    if (end_date)
        sqlite3_bind_text(stmt, idx++, end_date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, idx, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        struct submit_record *r;
        if (count == cap) {
            cap = cap ? cap * 2 : 16;
            rows = realloc(rows, (size_t)cap * sizeof(*rows));
            if (!rows)
                die("内存不足");
        }
        r = &rows[count];
        r->id           = sqlite3_column_int(stmt, 0);
        r->created_at   = dup_col_text(stmt, 1);
        r->date         = dup_col_text(stmt, 2);
        r->project_id   = dup_col_text(stmt, 3);
        r->project_name = dup_col_text(stmt, 4);
        r->timeslot     = sqlite3_column_double(stmt, 5);
        r->note         = dup_col_text(stmt, 6);
        r->status       = dup_col_text(stmt, 7);
        r->response     = dup_col_text(stmt, 8);
        count++;
    }

    *out_rows = rows;
    *out_count = count;
    ret = 0;
    rows = NULL;

cleanup:
    if (rows)
        db_free_submit_records(rows, count);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ret;
}

void db_free_submit_records(struct submit_record *rows, int count)
{
    if (!rows)
        return;
    for (int i = 0; i < count; i++) {
        free(rows[i].created_at);
        free(rows[i].date);
        free(rows[i].project_id);
        free(rows[i].project_name);
        free(rows[i].note);
        free(rows[i].status);
        free(rows[i].response);
    }
    free(rows);
}
