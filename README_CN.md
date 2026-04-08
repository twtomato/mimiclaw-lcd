# MimiClaw-LCD

<p align="center">
  <img src="assets/banner.png" alt="MimiClaw-LCD" width="500" />
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License: MIT"></a>
</p>

<p align="center">
  <strong><a href="README.md">English</a> | <a href="README_CN.md">中文</a> | <a href="README_TW.md">繁中</a> | <a href="README_JA.md">日本語</a></strong>
</p>

> **Fork 自 [MimiClaw](https://github.com/memovai/mimiclaw)，原作者 [memovai](https://github.com/memovai)。**
> 本 fork 新增 TFT LCD 显示支持、CJK 字体渲染及若干稳定性改进。
> 核心 AI Agent 架构均来自原始 MimiClaw 项目。

---

MimiClaw-LCD 把一块小小的 ESP32-S3 开发板变成你的私人 AI 助理，并可在 TFT 显示屏上实时呈现微信风格的对话气泡 UI，不需要手机也能看到完整对话。以 MimiClaw 成熟的 Agent 循环为基础，没有 Linux，没有 Node.js，纯 C。

## 本 Fork 新增功能

| 功能 | 说明 |
|------|------|
| **RM68140 TFT LCD** | 320×480 8-bit 并口，微信风格对话气泡 UI |
| **ILI9341 TFT LCD** | 240×320 SPI，同款对话气泡 UI |
| **CJK 字体支持** | Source Han Sans 16 — 繁/简体中文 + 日文（平假名、片假名、汉字）|
| **Latin-1 Supplement** | 涵盖 `·`、`©`、`°` 等常见非 ASCII 字符 |
| **字体 Kconfig 选项** | 编译时选择 CJK（~1.1 MB）或仅 Latin（Montserrat 14，节省 ~1.1 MB flash）|
| **LCD GPIO 保护** | 显示器引脚自动屏蔽，AI 无法误操作 |
| **WiFi scan 修复** | 扫描不再干扰自动重连逻辑 |
| **重试计数重置** | WiFi 扫描或凭据更改后重试计数正确归零 |

## 认识 MimiClaw

- **小巧** — 没有 Linux，没有 Node.js，没有臃肿依赖 — 纯 C
- **好用** — 在 Telegram 发消息，剩下的它来搞定
- **忠诚** — 从记忆中学习，跨重启也不会忘
- **省电** — USB 供电，0.5W，24/7 运行
- **可爱** — 一块 ESP32-S3 开发板，约 $10，没了

## 工作原理

![](assets/mimiclaw.png)

你在 Telegram 发一条消息，ESP32-S3 通过 WiFi 收到后送进 Agent 循环 — LLM 思考、调用工具、读取记忆 — 再把回复发回来。如果连接了 TFT 显示屏，对话会实时以气泡形式呈现。支持 **Anthropic (Claude)**、**OpenAI (GPT)** 和 **OpenRouter** 三种提供商，运行时可切换。

## 演示

| 实际对话 | 多语言显示 |
|---|---|
| ![对话演示](docs/images/demo_chat.jpg) | ![多语言](docs/images/demo_multilingual.jpg) |

*RM68140 320×480 — 微信风格对话气泡，支持 CJK 字体（中文 + 日文）*

## 快速开始

### 你需要

- 一块 **ESP32-S3 开发板**，16MB Flash + 8MB PSRAM（如小智 AI 开发板，~¥30）
- 一根 **USB Type-C 数据线**
- 一个 **Telegram Bot Token** — 在 Telegram 找 [@BotFather](https://t.me/BotFather) 创建
- 一个 **Anthropic API Key**、**OpenAI API Key** 或 **OpenRouter API Key**
- *（可选）* RM68140 或 ILI9341 TFT 显示屏

### 安装

```bash
# 需要先安装 ESP-IDF v5.5+:
# https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/

git clone https://github.com/YOUR_USERNAME/mimiclaw-lcd.git
cd mimiclaw-lcd

idf.py set-target esp32s3
```

<details>
<summary>Ubuntu 安装</summary>

建议基线：

- Ubuntu 22.04/24.04
- Python >= 3.10
- CMake >= 3.16
- Ninja >= 1.10
- Git >= 2.34
- flex >= 2.6
- bison >= 3.8
- gperf >= 3.1
- dfu-util >= 0.11
- `libusb-1.0-0`、`libffi-dev`、`libssl-dev`

Ubuntu 安装与构建：

```bash
sudo apt-get update
sudo apt-get install -y git wget flex bison gperf python3 python3-pip python3-venv \
  cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

./scripts/setup_idf_ubuntu.sh
./scripts/build_ubuntu.sh
```

</details>

<details>
<summary>macOS 安装</summary>

建议基线：

- macOS 12/13/14
- Xcode Command Line Tools
- Homebrew
- Python >= 3.10
- CMake >= 3.16
- Ninja >= 1.10
- Git >= 2.34
- flex >= 2.6
- bison >= 3.8
- gperf >= 3.1
- dfu-util >= 0.11
- `libusb`、`libffi`、`openssl`

macOS 安装与构建：

```bash
xcode-select --install
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

./scripts/setup_idf_macos.sh
./scripts/build_macos.sh
```

</details>

### 配置

MimiClaw-LCD 使用**两层配置**：`mimi_secrets.h` 提供编译时默认值，串口 CLI 可在运行时覆盖。CLI 设置的值存在 NVS Flash 中，优先级高于编译时值。

```bash
cp main/mimi_secrets.h.example main/mimi_secrets.h
```

编辑 `main/mimi_secrets.h`：

```c
#define MIMI_SECRET_WIFI_SSID       "你的WiFi名"
#define MIMI_SECRET_WIFI_PASS       "你的WiFi密码"
#define MIMI_SECRET_TG_TOKEN        "123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11"
#define MIMI_SECRET_API_KEY         "sk-ant-api03-xxxxx"
#define MIMI_SECRET_MODEL_PROVIDER  "anthropic"     // "anthropic"、"openai" 或 "openrouter"
#define MIMI_SECRET_SEARCH_KEY      ""              // 可选：Brave Search API key
#define MIMI_SECRET_TAVILY_KEY      ""              // 可选：Tavily API key（优先）
#define MIMI_SECRET_PROXY_HOST      ""              // 可选：代理地址
#define MIMI_SECRET_PROXY_PORT      ""              // 可选：代理端口
```

然后编译烧录：

```bash
# 完整编译（修改 mimi_secrets.h 后必须 fullclean）
idf.py fullclean && idf.py build

# 查找串口
ls /dev/cu.usb*          # macOS
ls /dev/ttyACM*          # Linux

# 烧录并监控（将 PORT 替换为你的串口）
idf.py -p PORT flash monitor
```

> **注意：请插对 USB 口！** 大多数 ESP32-S3 开发板有两个 Type-C 接口，必须插标有 **USB** 的那个口（原生 USB Serial/JTAG），**不要**插标有 **COM** 的口。插错口会导致烧录失败。
>
> <details>
> <summary>查看参考图片</summary>
>
> <img src="assets/esp32s3-usb-port.jpg" alt="请插 USB 口，不要插 COM 口" width="480" />
>
> </details>

### 代理配置（国内用户）

在国内需要代理才能访问 Telegram 和 API。MimiClaw-LCD 内置 HTTP CONNECT 隧道支持：

```
mimi> set_proxy 192.168.1.83 7897   # 设置代理
mimi> clear_proxy                    # 清除代理
```

### CLI 命令（通过 UART/COM 口连接）

**运行时配置**（存入 NVS，覆盖编译时默认值）：

```
mimi> wifi_set MySSID MyPassword   # 换 WiFi
mimi> set_tg_token 123456:ABC...   # 换 Telegram Bot Token
mimi> set_api_key sk-ant-api03-... # 换 API Key
mimi> set_model_provider openai    # 切换提供商（anthropic|openai|openrouter）
mimi> set_model gpt-4o             # 换模型
mimi> set_proxy 192.168.1.83 7897  # 设置代理
mimi> clear_proxy                  # 清除代理
mimi> set_search_key BSA...        # 设置 Brave Search API Key
mimi> set_tavily_key tvly-...      # 设置 Tavily API Key（优先）
mimi> config_show                  # 查看所有配置（脱敏显示）
mimi> config_reset                 # 清除 NVS，恢复编译时默认值
```

**调试与运维：**

```
mimi> wifi_status              # 连上了吗？
mimi> memory_read              # 看看它记住了什么
mimi> memory_write "内容"       # 写入 MEMORY.md
mimi> heap_info                # 还剩多少内存？
mimi> session_list             # 列出所有会话
mimi> session_clear 12345      # 删除一个会话
mimi> heartbeat_trigger        # 手动触发一次心跳检查
mimi> cron_start               # 立即启动 cron 调度器
mimi> restart                  # 重启
```

### USB (JTAG) 与 UART：哪个口做什么

| 端口 | 用途 |
|------|------|
| **USB**（JTAG） | `idf.py flash`、JTAG 调试 |
| **COM**（UART） | **REPL 命令行**、串口控制台 |

> **REPL 必须连接 UART（COM）口。**

## 显示屏

MimiClaw-LCD 支持三种显示后端，通过 `idf.py menuconfig → MimiClaw Display` 在编译时选择：

| 后端 | 分辨率 | 接口 | 说明 |
|------|--------|------|------|
| **RM68140** | 320×480 | 8-bit 并口 | TFT — 完整聊天气泡 UI（微信风格）|
| **ILI9341** | 240×320 | SPI | TFT — 完整聊天气泡 UI |
| SSD1306 | 128×64 | I2C | OLED — 仅显示状态信息 |
| 无 | — | — | 默认 — 不接显示屏 |

每个后端的 GPIO 引脚均可在 menuconfig 中配置。LCD 占用的引脚会自动屏蔽，AI 无法误操作。

### 字体选项（RM68140 / ILI9341 限定）

`idf.py menuconfig → MimiClaw Display → CJK font`：

| 选项 | Flash 占用 | 覆盖范围 |
|------|-----------|----------|
| **CJK（默认）** | ~1.1 MB | 繁/简中文、日文（平假名 + 片假名 + 汉字）、Latin-1 |
| Latin only | < 50 KB | ASCII + Latin Extended（Montserrat 14）|

## 记忆

| 文件 | 说明 |
|------|------|
| `SOUL.md` | 机器人的人设 — 编辑它来改变行为方式 |
| `USER.md` | 关于你的信息 — 姓名、偏好、语言 |
| `MEMORY.md` | 长期记忆 — 它应该一直记住的事 |
| `HEARTBEAT.md` | 待办清单 — 机器人定期检查并自主执行 |
| `cron.json` | 定时任务 — AI 创建的周期性或一次性任务 |
| `2026-02-05.md` | 每日笔记 — 今天发生了什么 |
| `tg_12345.jsonl` | 聊天记录 — 你和它的对话 |

## 工具

| 工具 | 说明 |
|------|------|
| `web_search` | 通过 Tavily（优先）或 Brave 搜索网页 |
| `get_current_time` | 获取当前日期和时间，并设置系统时钟 |
| `cron_add` | 创建周期、一次性或每日定时任务 |
| `cron_list` | 列出所有已调度的 cron 任务 |
| `cron_remove` | 按 ID 删除 cron 任务 |
| `set_config` | 运行时修改模型、提供商或 API Key |
| `gpio_read` | 读取 GPIO 引脚当前电平 |
| `gpio_write` | 设置 GPIO 引脚高低电平 |
| `files_read` | 读取 SPIFFS 上的文件 |
| `files_write` | 写入 SPIFFS 上的文件 |

## 定时任务（Cron）

内置 cron 调度器让 AI 可以自主安排任务。任务触发时消息会注入 Agent 循环，AI 自动醒来处理并回复。任务持久化存储在 SPIFFS（`cron.json`），重启后不会丢失。

## 心跳（Heartbeat）

心跳服务定期读取 `HEARTBEAT.md`，发现未完成项目就向 Agent 循环发送提示，让 AI 自主处理。默认每 30 分钟一次。

## 直接指令

消息以 `!` 开头可跳过 LLM，立即执行对应指令：

```
/help               — 显示所有指令
/config             — 查看当前配置（API Key 脱敏）
/model <name>       — 立即切换 LLM 模型
/provider <name>    — 切换提供商（anthropic / openai / openrouter）
/apikey <key>       — 更新 API Key
/tavily <key>       — 更新 Tavily 搜索 Key
/heap               — 显示剩余内存
/restart            — 重启设备
```

## 其他功能

- **WebSocket 网关** — 端口 18789，局域网内用任意 WebSocket 客户端连接
- **OTA 更新** — WiFi 远程刷固件，无需 USB
- **双核** — 网络 I/O 和 AI 处理分别跑在不同 CPU 核心
- **HTTP 代理** — CONNECT 隧道，适配受限网络
- **多提供商** — 支持 Anthropic (Claude)、OpenAI (GPT) 和 OpenRouter，运行时可切换
- **定时任务** — AI 可自主创建周期性、一次性和每日定时任务
- **心跳服务** — 定期检查任务文件，驱动 AI 自主执行
- **工具调用** — ReAct Agent 循环，支持搜索、GPIO、文件、配置、定时任务

## 开发者

- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** — 系统设计、模块划分、任务布局、内存分配、协议、Flash 分区
- **[docs/TODO.md](docs/TODO.md)** — 功能差距和路线图
- **[docs/WIFI_ONBOARDING_AP.md](docs/WIFI_ONBOARDING_AP.md)** — 本地 onboarding 热点使用说明
- **[docs/tool-setup/](docs/tool-setup/README.md)** — 外部服务配置指南

## 许可证

MIT

## 致谢

本项目 fork 自 **[MimiClaw](https://github.com/memovai/mimiclaw)**，原作者 [memovai](https://github.com/memovai)。核心 AI Agent 架构、工具系统、内存管理与通信渠道均来自原始 MimiClaw 项目，本 fork 新增 TFT LCD 显示支持及相关改进。

MimiClaw 本身灵感来自 [OpenClaw](https://github.com/openclaw/openclaw) 和 [Nanobot](https://github.com/HKUDS/nanobot)。
