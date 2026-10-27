#ifndef VIVIDLOG_DB_H
#define VIVIDLOG_DB_H

/*
 * db.h — 对应 Python 版 vividlog/storage.py
 *
 * 所有权约定（C 没有 GC，读代码前先理解这个）：
 *   - 返回 char * 的函数：新分配的内存，调用者必须 free()
 *   - 返回结构体数组的函数：配套 db_free_xxx() 释放
 *   - 所有函数返回 0 表示成功，-1 表示失败（错误信息打印到 stderr）
 */

/* 对应 init_db()：建库建表，幂等 */
int db_init(void);

/* 对应 clear_database()：清空所有业务数据，保留表结构 */
int db_clear(void);

/* 对应 save_token()/load_token()。load_token 返回 NULL 表示未保存 */
int  db_save_token(const char *token);
char *db_load_token(void);          /* 调用者 free；未保存返回 NULL */

/* 对应 save_config()/load_config()。
 * config 内容是 JSON 原文，本模块不做解析（第 3 步引入 cJSON 后再解析） */
int  db_save_config(const char *json_data);
char *db_load_config(void);         /* 调用者 free；未保存返回 NULL */

/* 对应 insert_log() */
int db_insert_log(const char *level, const char *message);

/* 一条日志记录，对应 Python 的 dict(created_at, level, message) */
struct log_row {
    char *created_at;   /* 调用 db_free_logs 时统一释放 */
    char *level;
    char *message;
};

/* 对应 get_logs()。*out_rows 指向新分配的数组（*out_count 条），
 * 用完后调用 db_free_logs()。无数据时 *out_rows 为 NULL、*out_count 为 0 */
int db_get_logs(int limit, struct log_row **out_rows, int *out_count);
void db_free_logs(struct log_row *rows, int count);

/* 一条提交记录，对应 insert_submit_record() 的参数与 submit_record 表 */
struct submit_record {
    int   id;
    char *created_at;
    char *date;
    char *project_id;
    char *project_name; /* 可为 NULL */
    double timeslot;
    char *note;         /* 可为 NULL */
    char *status;
    char *response;     /* 可为 NULL */
};

/* 对应 insert_submit_record()。name/note/response 允许传 NULL */
int db_insert_submit_record(const char *date, const char *project_id,
                            const char *project_name, double timeslot,
                            const char *note, const char *status,
                            const char *response);

/* 对应 get_submit_records()。start_date/end_date 允许传 NULL（不过滤） */
int db_get_submit_records(const char *start_date, const char *end_date,
                          int limit, struct submit_record **out_rows,
                          int *out_count);
void db_free_submit_records(struct submit_record *rows, int count);

#endif /* VIVIDLOG_DB_H */
