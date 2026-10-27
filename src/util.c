/*
 * util.c — 通用小工具
 * 教学点：xmalloc/xstrdup 存在的意义是"把失败处理收敛到一个地方"，
 * 业务代码里就不需要每次写 if (p == NULL) 检查。
 */
#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void *xmalloc(size_t size)
{
    void *p = malloc(size ? size : 1);
    if (!p)
        die("内存不足：malloc(%zu) 失败", size);
    return p;
}

char *xstrdup(const char *s)
{
    if (!s)
        return NULL;
    size_t len = strlen(s) + 1;
    char *copy = xmalloc(len);
    memcpy(copy, s, len);
    return copy;
}

char *xstrjoin(const char *a, const char *b)
{
    size_t la = strlen(a), lb = strlen(b);
    char *p = xmalloc(la + lb + 1);
    memcpy(p, a, la);
    memcpy(p + la, b, lb + 1);
    return p;
}

char *now_iso8601(void)
{
    /* 对应 Python 的 datetime.now().isoformat()
     * 格式：2026-10-27T21:30:00.123456（本地时间） */
    struct timespec ts;
    struct tm tm;

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);

    char *buf = xmalloc(64);
    snprintf(buf, 64, "%04d-%02d-%02dT%02d:%02d:%02d.%06ld",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000);
    return buf;
}

void die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("致命错误: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}
