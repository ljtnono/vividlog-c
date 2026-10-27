/*
 * main.c — CLI 入口，对应 Python 版的 cli.py + GUI 的命令行部分
 *
 * 教学点：
 *   - getopt_long 对应 argparse，但 C 里要自己写循环和处理
 *   - 每个子命令一个 static 函数，对应 Python 的 if args.command == ...
 *   - collect / run 是第 2、3 步的桩（stub），骨架先跑通
 */
#include "db.h"
#include "util.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- 帮助 ---------- */

static void print_help(void)
{
    puts("vividlog-c — 维拉工时日报工具（C 学习版）\n"
         "用法: vividlog-c <命令> [选项]\n"
         "\n"
         "第 1 步已实现:\n"
         "  token set <jwt>        保存 JWT token\n"
         "  token show             显示已保存的 token\n"
         "  config set '<json>'    保存配置（JSON 原文）\n"
         "  config show            显示配置\n"
         "  log list [-n N]        查看运行日志（默认 500 条）\n"
         "  record list [-s 日期] [-e 日期] [-n N]\n"
         "                         查看工时提交记录\n"
         "  clear                  清空本地数据库\n"
         "\n"
         "后续步骤待实现:\n"
         "  collect                收集 Git 提交（第 2 步，libgit2）\n"
         "  run                    生成并填写日报（第 3 步）\n");
}

/* ---------- token 子命令 ---------- */

static int cmd_token(int argc, char **argv)
{
    if (argc < 1) {
        fputs("用法: vividlog-c token <set|show>\n", stderr);
        return 1;
    }
    if (strcmp(argv[0], "set") == 0) {
        if (argc < 2) {
            fputs("用法: vividlog-c token set <jwt>\n", stderr);
            return 1;
        }
        if (db_save_token(argv[1]) != 0)
            return 1;
        db_insert_log("INFO", "token 已更新");
        puts("token 已保存");
        return 0;
    }
    if (strcmp(argv[0], "show") == 0) {
        char *token = db_load_token();   /* 所有权移交给我们，必须 free */
        if (!token) {
            puts("尚未保存 token");
            return 0;
        }
        printf("%s\n", token);
        free(token);
        return 0;
    }
    fprintf(stderr, "未知 token 子命令: %s\n", argv[0]);
    return 1;
}

/* ---------- config 子命令 ---------- */

static int cmd_config(int argc, char **argv)
{
    if (argc < 1) {
        fputs("用法: vividlog-c config <set|show>\n", stderr);
        return 1;
    }
    if (strcmp(argv[0], "set") == 0) {
        if (argc < 2) {
            fputs("用法: vividlog-c config set '<json>'\n", stderr);
            return 1;
        }
        if (db_save_config(argv[1]) != 0)
            return 1;
        db_insert_log("INFO", "config 已更新");
        puts("config 已保存");
        return 0;
    }
    if (strcmp(argv[0], "show") == 0) {
        char *data = db_load_config();
        if (!data) {
            puts("尚未保存 config");
            return 0;
        }
        printf("%s\n", data);
        free(data);
        return 0;
    }
    fprintf(stderr, "未知 config 子命令: %s\n", argv[0]);
    return 1;
}

/* ---------- log list 子命令 ---------- */

static int cmd_log_list(int argc, char **argv)
{
    int limit = 500;
    int opt;
    /* 子命令参数从 argv[3] 开始，直接挪 optind 而不是移动 argv 指针，
     * 否则 getopt 会把参数值当成程序名跳过（本次冒烟测试抓到的真 bug） */
    optind = 3;
    /* getopt_long 对应 Python 的 parser.add_argument("-n", ...) */
    static const struct option opts[] = {
        {"limit", required_argument, NULL, 'n'},
        {NULL, 0, NULL, 0},
    };
    while ((opt = getopt_long(argc, argv, "n:", opts, NULL)) != -1) {
        switch (opt) {
        case 'n':
            limit = atoi(optarg);
            break;
        default:
            return 1;
        }
    }

    struct log_row *rows = NULL;
    int count = 0;
    if (db_get_logs(limit, &rows, &count) != 0)
        return 1;

    for (int i = 0; i < count; i++)
        printf("[%s] %-5s %s\n", rows[i].created_at, rows[i].level,
               rows[i].message);
    printf("共 %d 条\n", count);

    db_free_logs(rows, count);   /* 配对释放，养成习惯 */
    return 0;
}

/* ---------- record list 子命令 ---------- */

static int cmd_record_list(int argc, char **argv)
{
    const char *start = NULL, *end = NULL;
    int limit = 500, opt;
    static const struct option opts[] = {
        {"start", required_argument, NULL, 's'},
        {"end",   required_argument, NULL, 'e'},
        {"limit", required_argument, NULL, 'n'},
        {NULL, 0, NULL, 0},
    };
    while ((opt = getopt_long(argc, argv, "s:e:n:", opts, NULL)) != -1) {
        switch (opt) {
        case 's': start = optarg; break;
        case 'e': end = optarg; break;
        case 'n': limit = atoi(optarg); break;
        default: return 1;
        }
    }

    struct submit_record *rows = NULL;
    int count = 0;
    if (db_get_submit_records(start, end, limit, &rows, &count) != 0)
        return 1;

    for (int i = 0; i < count; i++) {
        struct submit_record *r = &rows[i];
        printf("#%d [%s] %s 项目=%s 时长=%.2f 状态=%s\n",
               r->id, r->created_at, r->date, r->project_id,
               r->timeslot, r->status);
        if (r->project_name)
            printf("     项目名=%s\n", r->project_name);
        if (r->note)
            printf("     备注=%s\n", r->note);
    }
    printf("共 %d 条\n", count);

    db_free_submit_records(rows, count);
    return 0;
}

/* ---------- 后续步骤的桩 ---------- */

static int cmd_stub(const char *name, int step)
{
    printf("「%s」是第 %d 步的内容，骨架尚未实现。\n", name, step);
    return 0;
}

/* ---------- 入口 ---------- */

int main(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "help") == 0 ||
        strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_help();
        return 0;
    }

    /* 所有命令都依赖数据库，先初始化（幂等，对应 Python 各处的 init_db()） */
    if (db_init() != 0)
        return 1;

    const char *cmd = argv[1];
    /* 把剩余参数传给子命令。getopt 会重排 argv，所以传 argc-2 即可 */
    if (strcmp(cmd, "token") == 0)
        return cmd_token(argc - 2, argv + 2);
    if (strcmp(cmd, "config") == 0)
        return cmd_config(argc - 2, argv + 2);
    if (strcmp(cmd, "log") == 0 && argc >= 3 && strcmp(argv[2], "list") == 0)
        return cmd_log_list(argc, argv);
    if (strcmp(cmd, "record") == 0 && argc >= 3 && strcmp(argv[2], "list") == 0)
        return cmd_record_list(argc, argv);
    if (strcmp(cmd, "clear") == 0) {
        if (db_clear() == 0)
            puts("数据库已清空");
        return 0;
    }
    if (strcmp(cmd, "collect") == 0)
        return cmd_stub("collect", 2);
    if (strcmp(cmd, "run") == 0)
        return cmd_stub("run", 3);

    fprintf(stderr, "未知命令: %s（用 vividlog-c help 查看帮助）\n", cmd);
    return 1;
}
