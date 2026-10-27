#ifndef VIVIDLOG_UTIL_H
#define VIVIDLOG_UTIL_H

#include <stddef.h>

/*
 * 通用小工具：内存分配包装与当前时间。
 * 教学点：C 没有异常，malloc 失败不会抛 OutOfMemoryError，
 * 所以我们用 xmalloc 失败直接退出进程（Java 里崩溃的等价物）。
 */

/* malloc 失败即退出进程的分配函数，永远返回非 NULL */
void *xmalloc(size_t size);
/* strdup 的 xmalloc 版本 */
char *xstrdup(const char *s);
/* 拼接两个字符串，返回新分配的内存（调用者 free） */
char *xstrjoin(const char *a, const char *b);
/* 当前时间的 ISO8601 格式（如 2026-10-27T21:30:00.123456），调用者 free */
char *now_iso8601(void);
/* 打印错误并退出进程（对应 Java: throw new RuntimeException） */
void die(const char *fmt, ...);

#endif /* VIVIDLOG_UTIL_H */
