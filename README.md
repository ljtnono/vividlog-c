# vividlog-c

用纯 C 仿写 [vividlog](../vividlog)（维拉工时日报工具）的学习项目。

## 学习目标

- 掌握 C 的核心心智模型：指针、手动内存管理、错误码、所有权
- 熟悉常用 C 库：sqlite3 / libcurl / libgit2 / cJSON / GTK4
- 学会组织多文件 C 项目和 Makefile 构建

## 推进路线（对照 Python 原版）

| 步骤 | 模块 | C 技术点 | 状态 |
|---|---|---|---|
| 1 | storage + cli 骨架 | sqlite3 C API、getopt、动态字符串 | ✅ |
| 2 | collector | libgit2、回调、迭代器 | ⬜ |
| 3 | client / report / submitter | libcurl、cJSON、时间处理 | ⬜ |
| 4 | GUI（gtk4） | GObject、信号回调、GLib 容器 | ⬜ |
| 5 | 登录流程 | socket、本机 HTTP 回调服务器 | ⬜ |

架构原则：核心逻辑（core/）与界面（cli/、gui/）分层，两边共用同一套 core。

## 构建与运行

```sh
make            # 构建
make check      # valgrind 内存检查（需安装 valgrind）
./vividlog-c help
```

## CLion 使用提示

打开项目后，运行配置选 **vividlog-c**（Makefile 目标），不要选 "C/C++ File"
（单文件配置只编译 main.c，会报 undefined reference）。Debug 用系统 gdb。

## 第 1 步已实现的命令

```sh
./vividlog-c token set <jwt>      # 保存 token（对应 save_token）
./vividlog-c token show           # 读取 token（对应 load_token）
./vividlog-c config set '<json>'  # 保存配置 JSON 原文
./vividlog-c config show          # 显示配置 JSON 原文
./vividlog-c log list [-n N]      # 查看日志
./vividlog-c record list [-s 起始 -e 结束 -n N]  # 查看提交记录
./vividlog-c clear                # 清空数据库
```

数据文件与 Python 版共用：`~/.local/share/VividLog/vividlog.db`。

## 给 Java 程序员的阅读指引

- **所有权**：凡是返回 `char *` 或结构体数组的函数，注释都标了"调用者负责 free"
  （对应 Java 里你从不操心的 GC）。配套释放函数 `db_free_xxx`。
- **错误处理**：所有函数返回 `0` 成功 / `-1` 失败，内部统一用 `goto cleanup`
  模式（C 界的 try-finally）。
- **没有字符串类型**：`db.c` 里所有 SQL 参数绑定都是 `sqlite3_bind_text` + 手动算长度，
  注意 `strlen` 和 `-1`（让 SQLite 自己算）的区别。

## 许可证

[MIT](LICENSE)
