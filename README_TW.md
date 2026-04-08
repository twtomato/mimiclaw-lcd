# MimiClaw-LCD

<p align="center">
  <img src="assets/banner.png" alt="MimiClaw-LCD" width="500" />
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License: MIT"></a>
</p>

<p align="center">
  <strong><a href="README.md">English</a> | <a href="README_CN.md">简中</a> | <a href="README_TW.md">繁中</a> | <a href="README_JA.md">日本語</a></strong>
</p>

> **Fork 自 [MimiClaw](https://github.com/memovai/mimiclaw)，原作者 [memovai](https://github.com/memovai)。**
> 本 fork 新增 TFT LCD 顯示支援、CJK 字型渲染及若干穩定性改善。
> 核心 AI Agent 架構均來自原始 MimiClaw 專案。

---

MimiClaw-LCD 把一塊小小的 ESP32-S3 開發板變成你的私人 AI 助理，並可在 TFT 顯示器上即時呈現 WeChat 風格的對話氣泡 UI，不需要手機也能看到完整對話。以 MimiClaw 成熟的 Agent 迴圈為基礎，不需要 Linux，不需要 Node.js，純 C 語言。

## 本 Fork 新增功能

| 功能 | 說明 |
|------|------|
| **RM68140 TFT LCD** | 320×480 8-bit 並列，WeChat 風格對話氣泡 UI |
| **ILI9341 TFT LCD** | 240×320 SPI，同款對話氣泡 UI |
| **CJK 字型支援** | Source Han Sans 16 — 繁體/簡體中文 + 日文（平假名、片假名、漢字）|
| **Latin-1 Supplement** | 涵蓋 `·`、`©`、`°` 等常見非 ASCII 字元 |
| **字型 Kconfig 選項** | 編譯時選擇 CJK（~1.1 MB）或僅 Latin（Montserrat 14，節省 ~1.1 MB flash）|
| **LCD GPIO 保護** | 顯示器腳位自動封鎖，AI 無法誤操作 |
| **WiFi scan 修正** | 掃描不再干擾自動重連邏輯 |
| **重試計數重置** | WiFi 掃描或憑證變更後重試計數正確歸零 |

## 認識 MimiClaw

- **小巧** — 沒有 Linux，沒有 Node.js，沒有臃腫依賴 — 純 C
- **好用** — 在 Telegram 發訊息，剩下的它來搞定
- **忠誠** — 從記憶中學習，跨重啟也不會忘
- **省電** — USB 供電，0.5W，24/7 運行
- **可愛** — 一塊 ESP32-S3 開發板，約 $10，沒了

## 運作原理

![](assets/mimiclaw.png)

你在 Telegram 發一則訊息，ESP32-S3 透過 WiFi 收到後送進 Agent 迴圈 — LLM 思考、呼叫工具、讀取記憶 — 再把回覆發回來。若連接了 TFT 顯示器，對話會即時以氣泡形式呈現。支援 **Anthropic (Claude)**、**OpenAI (GPT)** 和 **OpenRouter** 三種 provider，執行時可切換。

## 實機展示

| 實際對話 | 多語言顯示 |
|---|---|
| ![對話示範](docs/images/demo_chat.jpg) | ![多語言](docs/images/demo_multilingual.jpg) |

*RM68140 320×480 — WeChat 風格對話氣泡，支援 CJK 字型（中文 + 日文）*

## 快速開始

### 你需要

- 一塊 **ESP32-S3 開發板**，16MB Flash + 8MB PSRAM（例如小智 AI 開發板，約 $10）
- 一條 **USB Type-C 傳輸線**
- 一個 **Telegram Bot Token** — 在 Telegram 找 [@BotFather](https://t.me/BotFather) 建立
- 一個 **Anthropic API Key** — 從 [console.anthropic.com](https://console.anthropic.com) 取得，或 **OpenAI API Key**、**OpenRouter API Key**
- *（選用）* RM68140 或 ILI9341 TFT 顯示器

### 安裝

```bash
# 需要先安裝 ESP-IDF v5.5+：
# https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/

git clone https://github.com/twtomato/mimiclaw-lcd.git
cd mimiclaw-lcd

idf.py set-target esp32s3
```

<details>
<summary>Ubuntu 安裝</summary>

建議基線：

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

Ubuntu 安裝與建置：

```bash
sudo apt-get update
sudo apt-get install -y git wget flex bison gperf python3 python3-pip python3-venv \
  cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

./scripts/setup_idf_ubuntu.sh
./scripts/build_ubuntu.sh
```

</details>

<details>
<summary>macOS 安裝</summary>

建議基線：

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

macOS 安裝與建置：

```bash
xcode-select --install
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

./scripts/setup_idf_macos.sh
./scripts/build_macos.sh
```

</details>

### 設定

MimiClaw-LCD 使用**兩層設定**：`mimi_secrets.h` 提供編譯時預設值，序列埠 CLI 可在執行時覆蓋。CLI 設定的值存在 NVS Flash 中，優先級高於編譯時值。

```bash
cp main/mimi_secrets.h.example main/mimi_secrets.h
```

編輯 `main/mimi_secrets.h`：

```c
#define MIMI_SECRET_WIFI_SSID       "你的WiFi名稱"
#define MIMI_SECRET_WIFI_PASS       "你的WiFi密碼"
#define MIMI_SECRET_TG_TOKEN        "123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11"
#define MIMI_SECRET_API_KEY         "sk-ant-api03-xxxxx"
#define MIMI_SECRET_MODEL_PROVIDER  "anthropic"     // "anthropic"、"openai" 或 "openrouter"
#define MIMI_SECRET_SEARCH_KEY      ""              // 選用：Brave Search API key
#define MIMI_SECRET_TAVILY_KEY      ""              // 選用：Tavily API key（優先）
#define MIMI_SECRET_PROXY_HOST      ""              // 選用：例如 "10.0.0.1"
#define MIMI_SECRET_PROXY_PORT      ""              // 選用：例如 "7897"
```

然後建置並燒錄：

```bash
# 完整建置（修改 mimi_secrets.h 後必須 fullclean）
idf.py fullclean && idf.py build

# 查找序列埠
ls /dev/cu.usb*          # macOS
ls /dev/ttyACM*          # Linux

# 燒錄並監控（將 PORT 替換為你的序列埠）
idf.py -p PORT flash monitor
```

> **注意：請插對 USB 口！** 大多數 ESP32-S3 開發板有兩個 Type-C 接口，必須插標有 **USB** 的那個口（原生 USB Serial/JTAG），**不要**插標有 **COM** 的口（外部 UART 橋接）。插錯口會導致燒錄失敗。
>
> <details>
> <summary>查看參考圖片</summary>
>
> <img src="assets/esp32s3-usb-port.jpg" alt="請插 USB 口，不要插 COM 口" width="480" />
>
> </details>

### 代理設定（受限網路）

若需要代理才能存取 Telegram 和 API，MimiClaw-LCD 內建 HTTP CONNECT 隧道支援。可在 `mimi_secrets.h` 編譯時設定，也可透過序列埠 CLI 隨時修改：

```
mimi> set_proxy 192.168.1.83 7897   # 設定代理
mimi> clear_proxy                    # 清除代理
```

### CLI 指令（透過 UART/COM 口連接）

**執行時設定**（存入 NVS，覆蓋編譯時預設值）：

```
mimi> wifi_set MySSID MyPassword   # 換 WiFi
mimi> set_tg_token 123456:ABC...   # 換 Telegram Bot Token
mimi> set_api_key sk-ant-api03-... # 換 API Key
mimi> set_model_provider openai    # 切換 provider（anthropic|openai|openrouter）
mimi> set_model gpt-4o             # 換模型
mimi> set_proxy 192.168.1.83 7897  # 設定代理
mimi> clear_proxy                  # 清除代理
mimi> set_search_key BSA...        # 設定 Brave Search API Key
mimi> set_tavily_key tvly-...      # 設定 Tavily API Key（優先）
mimi> config_show                  # 查看所有設定（敏感資訊遮罩）
mimi> config_reset                 # 清除 NVS，恢復編譯時預設值
```

**除錯與維運：**

```
mimi> wifi_status              # 是否已連線？
mimi> memory_read              # 看看它記住了什麼
mimi> memory_write "內容"       # 寫入 MEMORY.md
mimi> heap_info                # 還剩多少記憶體？
mimi> session_list             # 列出所有會話
mimi> session_clear 12345      # 刪除一個會話
mimi> heartbeat_trigger        # 手動觸發一次心跳檢查
mimi> cron_start               # 立即啟動 cron 排程器
mimi> restart                  # 重啟
```

### USB（JTAG）vs UART：哪個口做什麼

| 接口 | 用途 |
|------|------|
| **USB**（JTAG） | `idf.py flash`、JTAG 除錯 |
| **COM**（UART） | **REPL 命令列**、序列埠控制台 |

> **REPL 必須連接 UART（COM）口。**

## 顯示器

MimiClaw-LCD 支援三種顯示後端，透過 `idf.py menuconfig → MimiClaw Display` 在編譯時選擇：

| 後端 | 解析度 | 介面 | 說明 |
|------|--------|------|------|
| **RM68140** | 320×480 | 8-bit 並列 | TFT — 完整聊天氣泡 UI（WeChat 風格）|
| **ILI9341** | 240×320 | SPI | TFT — 完整聊天氣泡 UI |
| SSD1306 | 128×64 | I2C | OLED — 僅顯示狀態資訊 |
| 無 | — | — | 預設 — 不接顯示器 |

GPIO 腳位均可在 menuconfig 中設定。LCD 佔用的腳位會自動封鎖，AI 無法誤操作。

### 字型選項（RM68140 / ILI9341 限定）

`idf.py menuconfig → MimiClaw Display → CJK font`：

| 選項 | Flash 用量 | 涵蓋範圍 |
|------|-----------|----------|
| **CJK（預設）** | ~1.1 MB | 繁/簡中文、日文（平假名 + 片假名 + 漢字）、Latin-1 |
| Latin only | < 50 KB | ASCII + Latin Extended（Montserrat 14）|

## 記憶

| 檔案 | 說明 |
|------|------|
| `SOUL.md` | 機器人的人設 — 編輯它來改變行為方式 |
| `USER.md` | 關於你的資訊 — 姓名、喜好、語言 |
| `MEMORY.md` | 長期記憶 — 它應該一直記住的事 |
| `HEARTBEAT.md` | 待辦清單 — 機器人定期檢查並自主執行 |
| `cron.json` | 排程任務 — AI 建立的週期性或一次性任務 |
| `2026-02-05.md` | 每日筆記 — 今天發生了什麼 |
| `tg_12345.jsonl` | 聊天記錄 — 你和它的對話 |

## 工具

| 工具 | 說明 |
|------|------|
| `web_search` | 透過 Tavily（優先）或 Brave 搜尋網頁 |
| `get_current_time` | 取得當前日期和時間，並設定系統時鐘 |
| `cron_add` | 建立週期、一次性或每日定時任務 |
| `cron_list` | 列出所有已排程的 cron 任務 |
| `cron_remove` | 依 ID 刪除 cron 任務 |
| `set_config` | 執行時修改模型、provider 或 API Key |
| `gpio_read` | 讀取 GPIO 腳位當前電位 |
| `gpio_write` | 設定 GPIO 腳位高低電位 |
| `files_read` | 讀取 SPIFFS 上的檔案 |
| `files_write` | 寫入 SPIFFS 上的檔案 |

## 排程任務（Cron）

內建 cron 排程器讓 AI 可以自主安排任務。任務觸發時訊息會注入 Agent 迴圈，AI 自動醒來處理並回覆。任務持久化存儲在 SPIFFS（`cron.json`），重啟後不會遺失。

## 心跳（Heartbeat）

心跳服務定期讀取 `HEARTBEAT.md`，發現未完成項目就向 Agent 迴圈發送提示，讓 AI 自主處理。預設每 30 分鐘一次。

## 直接指令

訊息以 `!` 開頭可繞過 LLM，立即執行對應指令：

```
/help               — 顯示所有指令
/config             — 查看當前設定（API Key 遮罩）
/model <name>       — 立即切換 LLM 模型
/provider <name>    — 切換 provider（anthropic / openai / openrouter）
/apikey <key>       — 更新 API Key
/tavily <key>       — 更新 Tavily 搜尋 Key
/heap               — 顯示剩餘記憶體
/restart            — 重啟裝置
```

## 其他功能

- **WebSocket 閘道** — 連接埠 18789，區域網路內用任意 WebSocket 客戶端連接
- **OTA 更新** — WiFi 遠端燒錄韌體，無需 USB
- **雙核心** — 網路 I/O 和 AI 處理分別跑在不同 CPU 核心
- **HTTP 代理** — CONNECT 隧道，適配受限網路
- **多 provider** — 支援 Anthropic (Claude)、OpenAI (GPT) 和 OpenRouter，執行時可切換
- **排程任務** — AI 可自主建立週期性、一次性和每日定時任務
- **心跳服務** — 定期檢查任務檔案，驅動 AI 自主執行
- **工具呼叫** — ReAct Agent 迴圈，支援搜尋、GPIO、檔案、設定、排程任務

## 開發者

- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** — 系統設計、模組劃分、任務佈局、記憶體分配、協定、Flash 分區
- **[docs/TODO.md](docs/TODO.md)** — 功能差距和路線圖
- **[docs/WIFI_ONBOARDING_AP.md](docs/WIFI_ONBOARDING_AP.md)** — 本地 onboarding 熱點使用說明
- **[docs/tool-setup/](docs/tool-setup/README.md)** — 外部服務設定指南

## 授權

MIT

## 致謝

本專案 fork 自 **[MimiClaw](https://github.com/memovai/mimiclaw)**，原作者 [memovai](https://github.com/memovai)。核心 AI Agent 架構、工具系統、記憶體管理與通訊頻道均來自原始 MimiClaw 專案，本 fork 新增 TFT LCD 顯示支援及相關改善。

MimiClaw 本身靈感來自 [OpenClaw](https://github.com/openclaw/openclaw) 和 [Nanobot](https://github.com/HKUDS/nanobot)。
