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

> **[MimiClaw v0.1.1](https://github.com/memovai/mimiclaw)（作者：[memovai](https://github.com/memovai)）をベースにしています。**
> このフォークでは TFT LCD ディスプレイサポート、CJK フォントレンダリング、および安定性の改善が追加されています。
> コア AI エージェントアーキテクチャはすべてオリジナルの MimiClaw プロジェクトに由来します。

---

MimiClaw-LCD は小さな ESP32-S3 ボードをパーソナル AI アシスタントに変え、TFT ディスプレイにリアルタイムで WeChat 風チャットバブル UI を表示します。スマートフォンがなくても会話を直接確認できます。MimiClaw の実績あるエージェントループをベースに、Linux 不要、Node.js 不要、純粋な C のみで動作します。

## このフォークの新機能

| 機能 | 説明 |
|------|------|
| **RM68140 TFT LCD** | 320×480 8-bit パラレル、WeChat 風チャットバブル UI |
| **ILI9341 TFT LCD** | 240×320 SPI、同じチャットバブル UI |
| **CJK フォントサポート** | Source Han Sans 16 — 繁/簡体字中国語 + 日本語（ひらがな、カタカナ、漢字）|
| **Latin-1 Supplement** | `·`、`©`、`°` などの一般的な非 ASCII 文字をカバー |
| **フォント Kconfig オプション** | コンパイル時に CJK（~1.1 MB）または Latin のみ（Montserrat 14、~1.1 MB 節約）を選択 |
| **LCD GPIO 保護** | ディスプレイピンを自動ブロック、AI の誤操作を防止 |
| **WiFi スキャン修正** | スキャンが自動再接続ロジックを妨げないよう修正 |
| **リトライカウントリセット** | WiFi スキャンまたは認証情報変更後にリトライカウントが正しくリセット |

## MimiClaw の特徴

- **超小型** — Linux 不要、Node.js 不要、無駄なし — 純粋な C のみ
- **便利** — Telegram でメッセージを送るだけ、あとはお任せ
- **忠実** — メモリから学習し、再起動しても忘れない
- **省エネ** — USB 給電、0.5W、24 時間 365 日稼働
- **お手頃** — ESP32-S3 ボード 1 枚、約 $10、それだけ

## 仕組み

![](assets/mimiclaw.png)

Telegram でメッセージを送ると、ESP32-S3 が WiFi 経由で受信し、エージェントループに送ります — LLM が思考し、ツールを呼び出し、メモリを読み取り — 返答を送り返します。TFT ディスプレイが接続されている場合、会話はリアルタイムでバブルとして表示されます。**Anthropic (Claude)**、**OpenAI (GPT)**、**OpenRouter** の 3 つのプロバイダーをサポートし、実行時に切り替え可能です。

## デモ

| 会話デモ | 多言語表示 |
|---|---|
| ![会話デモ](docs/images/demo_chat.jpg) | ![多言語](docs/images/demo_multilingual.jpg) |

*RM68140 320×480 — WeChat 風チャットバブル、CJK フォント対応（中国語 + 日本語）*

## クイックスタート

### 必要なもの

- **ESP32-S3 開発ボード**（16MB Flash + 8MB PSRAM 搭載、例：小智 AI ボード、約 $10）
- **USB Type-C ケーブル**
- **Telegram Bot トークン** — Telegram で [@BotFather](https://t.me/BotFather) に話しかけて作成
- **Anthropic API キー**、**OpenAI API キー**、または **OpenRouter API キー**
- *（オプション）* RM68140 または ILI9341 TFT ディスプレイ

### インストール

```bash
# まず ESP-IDF v5.5+ をインストールしてください:
# https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/

git clone https://github.com/twtomato/mimiclaw-lcd.git
cd mimiclaw-lcd

idf.py set-target esp32s3
```

<details>
<summary>Ubuntu インストール</summary>

推奨ベースライン:

- Ubuntu 22.04/24.04
- Python >= 3.10
- CMake >= 3.16
- Ninja >= 1.10
- Git >= 2.34
- flex >= 2.6
- bison >= 3.8
- gperf >= 3.1
- dfu-util >= 0.11
- `libusb-1.0-0`, `libffi-dev`, `libssl-dev`

Ubuntu でのインストールとビルド:

```bash
sudo apt-get update
sudo apt-get install -y git wget flex bison gperf python3 python3-pip python3-venv \
  cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

./scripts/setup_idf_ubuntu.sh
./scripts/build_ubuntu.sh
```

</details>

<details>
<summary>macOS インストール</summary>

推奨ベースライン:

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
- `libusb`, `libffi`, `openssl`

macOS でのインストールとビルド:

```bash
xcode-select --install
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

./scripts/setup_idf_macos.sh
./scripts/build_macos.sh
```

</details>

### 設定

MimiClaw-LCD は**2 層設定**を採用しています：`mimi_secrets.h` でビルド時のデフォルト値を設定し、シリアル CLI で実行時にオーバーライドできます。

```bash
cp main/mimi_secrets.h.example main/mimi_secrets.h
```

`main/mimi_secrets.h` を編集：

```c
#define MIMI_SECRET_WIFI_SSID       "WiFi名"
#define MIMI_SECRET_WIFI_PASS       "WiFiパスワード"
#define MIMI_SECRET_TG_TOKEN        "123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11"
#define MIMI_SECRET_API_KEY         "sk-ant-api03-xxxxx"
#define MIMI_SECRET_MODEL_PROVIDER  "anthropic"     // "anthropic"、"openai"、または "openrouter"
#define MIMI_SECRET_SEARCH_KEY      ""              // 任意：Brave Search API キー
#define MIMI_SECRET_TAVILY_KEY      ""              // 任意：Tavily API キー（優先）
#define MIMI_SECRET_PROXY_HOST      ""              // 任意：例 "10.0.0.1"
#define MIMI_SECRET_PROXY_PORT      ""              // 任意：例 "7897"
```

ビルドとフラッシュ：

```bash
# フルビルド（mimi_secrets.h 変更後は fullclean 必須）
idf.py fullclean && idf.py build

# シリアルポートを確認
ls /dev/cu.usb*          # macOS
ls /dev/ttyACM*          # Linux

# フラッシュとモニター（PORT をあなたのポートに置き換え）
idf.py -p PORT flash monitor
```

> **重要：正しい USB ポートに接続してください！** ほとんどの ESP32-S3 ボードには 2 つの USB-C ポートがあります。**USB**（ネイティブ USB Serial/JTAG）と書かれたポートを使用し、**COM** ポートは使わないでください。
>
> <details>
> <summary>参考画像を表示</summary>
>
> <img src="assets/esp32s3-usb-port.jpg" alt="USB ポートに接続" width="480" />
>
> </details>

### CLI コマンド（UART/COM ポート経由）

**実行時設定**（NVS に保存、ビルド時デフォルト値をオーバーライド）：

```
mimi> wifi_set MySSID MyPassword   # WiFi ネットワークを変更
mimi> set_tg_token 123456:ABC...   # Telegram Bot トークンを変更
mimi> set_api_key sk-ant-api03-... # API キーを変更
mimi> set_model_provider openai    # プロバイダーを切替（anthropic|openai|openrouter）
mimi> set_model gpt-4o             # LLM モデルを変更
mimi> set_proxy 127.0.0.1 7897     # HTTP プロキシを設定
mimi> clear_proxy                  # プロキシを削除
mimi> set_search_key BSA...        # Brave Search API キーを設定
mimi> set_tavily_key tvly-...      # Tavily API キーを設定（優先）
mimi> config_show                  # 全設定を表示（マスク付き）
mimi> config_reset                 # NVS をクリア、ビルド時デフォルトに戻す
```

**デバッグ・メンテナンス：**

```
mimi> wifi_status              # 接続されていますか？
mimi> memory_read              # ボットが何を覚えているか確認
mimi> memory_write "内容"       # MEMORY.md に書き込み
mimi> heap_info                # 空き RAM はどれくらい？
mimi> session_list             # 全チャットセッションを一覧
mimi> session_clear 12345      # 会話を削除
mimi> heartbeat_trigger        # ハートビートチェックを手動トリガー
mimi> cron_start               # cron スケジューラを今すぐ開始
mimi> restart                  # 再起動
```

### USB（JTAG）vs UART：どのポートで何をするか

| ポート | 用途 |
|--------|------|
| **USB**（JTAG） | `idf.py flash`、JTAG デバッグ |
| **COM**（UART） | **REPL CLI**、シリアルコンソール |

> **REPL には UART（COM）ポートが必要です。**

## ディスプレイ

MimiClaw-LCD は 3 つの表示バックエンドをサポートしており、`idf.py menuconfig → MimiClaw Display` でコンパイル時に選択します：

| バックエンド | 解像度 | インターフェース | 説明 |
|------------|--------|----------------|------|
| **RM68140** | 320×480 | 8-bit パラレル | TFT — チャットバブル UI（WeChat 風）|
| **ILI9341** | 240×320 | SPI | TFT — チャットバブル UI |
| SSD1306 | 128×64 | I2C | OLED — ステータス情報のみ表示 |
| なし | — | — | デフォルト — ディスプレイなし |

各バックエンドの GPIO ピンは menuconfig で設定可能です。LCD が使用するピンは AI の GPIO アクセスから自動的にブロックされます。

### フォントオプション（RM68140 / ILI9341 限定）

`idf.py menuconfig → MimiClaw Display → CJK font`：

| オプション | Flash 使用量 | カバー範囲 |
|-----------|------------|-----------|
| **CJK（デフォルト）** | ~1.1 MB | 繁/簡体字中国語、日本語（ひらがな + カタカナ + 漢字）、Latin-1 |
| Latin only | < 50 KB | ASCII + Latin Extended（Montserrat 14）|

## メモリ

| ファイル | 説明 |
|----------|------|
| `SOUL.md` | ボットの性格 — 編集して振る舞いを変更 |
| `USER.md` | あなたの情報 — 名前、好み、言語 |
| `MEMORY.md` | 長期記憶 — ボットが常に覚えておくべきこと |
| `HEARTBEAT.md` | タスクリスト — ボットが定期的にチェックして自律的に実行 |
| `cron.json` | スケジュールジョブ — AI が作成した定期・単発タスク |
| `2026-02-05.md` | 日次メモ — 今日あったこと |
| `tg_12345.jsonl` | チャット履歴 — ボットとの会話 |

## ツール

| ツール | 説明 |
|--------|------|
| `web_search` | Tavily（優先）または Brave でウェブ検索 |
| `get_current_time` | 現在の日時を取得し、システムクロックを設定 |
| `cron_add` | 定期・単発・毎日指定時刻タスクをスケジュール |
| `cron_list` | スケジュール済みの cron ジョブを一覧表示 |
| `cron_remove` | ID で cron ジョブを削除 |
| `set_config` | 実行時にモデル・プロバイダー・API キーを変更 |
| `gpio_read` | GPIO ピンの現在の電圧レベルを読み取り |
| `gpio_write` | GPIO ピンを HIGH/LOW に設定 |
| `files_read` | SPIFFS ストレージからファイルを読み取り |
| `files_write` | SPIFFS ストレージにファイルを書き込み |

## Cron タスク

内蔵 cron スケジューラにより AI が自律的にタスクをスケジュールできます。ジョブが発火するとメッセージがエージェントループに注入され、AI が起動してタスクを処理・応答します。ジョブは SPIFFS（`cron.json`）に永続化され、再起動後も保持されます。

## ハートビート

ハートビートサービスは `HEARTBEAT.md` を定期的に読み取り、未完了の項目が見つかるとエージェントループにプロンプトを送信し、AI が自律的に処理します。デフォルトは 30 分ごとです。

## ダイレクトコマンド

`!` を先頭に付けてメッセージを送ると、LLM をバイパスしてコマンドを即時実行できます：

```
/help               — 全コマンドを表示
/config             — 現在の設定を表示（API キーはマスク）
/model <name>       — LLM モデルを即時切り替え
/provider <name>    — プロバイダーを切替（anthropic / openai / openrouter）
/apikey <key>       — API キーを更新
/tavily <key>       — Tavily 検索キーを更新
/heap               — 空き RAM を表示
/restart            — デバイスを再起動
```

## その他の機能

- **WebSocket ゲートウェイ** — ポート 18789、LAN 内から任意の WebSocket クライアントで接続
- **OTA アップデート** — WiFi 経由でファームウェア更新、USB 不要
- **デュアルコア** — ネットワーク I/O と AI 処理が別々の CPU コアで動作
- **HTTP プロキシ** — CONNECT トンネル対応、制限付きネットワークに対応
- **マルチプロバイダー** — Anthropic (Claude)、OpenAI (GPT)、OpenRouter をサポート、実行時に切り替え可能
- **Cron スケジューラ** — AI が定期・単発・毎日指定時刻タスクを自律的にスケジュール
- **ハートビート** — タスクファイルを定期チェックし、AI を自律的に駆動
- **ツール呼び出し** — ReAct エージェントループ、ウェブ検索・GPIO・ファイル・設定・Cron など対応

## 開発者向け

- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** — システム設計、モジュール構成、タスクレイアウト、メモリバジェット、プロトコル、Flash パーティション
- **[docs/TODO.md](docs/TODO.md)** — 機能ギャップとロードマップ
- **[docs/DISPLAY_RM68140.md](docs/DISPLAY_RM68140.md)** — RM68140 320×480 パラレル LCD：配線、bit-bang ドライバ、UI レイアウト
- **[docs/DISPLAY_ILI9341.md](docs/DISPLAY_ILI9341.md)** — ILI9341 240×320 SPI LCD：配線、DMA ドライバ、バイトスワップ、UI レイアウト
- **[docs/WIFI_ONBOARDING_AP.md](docs/WIFI_ONBOARDING_AP.md)** — ローカル onboarding アクセスポイントの使い方
- **[docs/tool-setup/](docs/tool-setup/README.md)** — 外部サービス設定ガイド

## ライセンス

MIT

## 謝辞

本プロジェクトは **[MimiClaw v0.1.1](https://github.com/memovai/mimiclaw)**（作者：[memovai](https://github.com/memovai)）をベースにしています。コア AI エージェントアーキテクチャ、ツールシステム、メモリ管理、通信チャネルはすべてオリジナルの MimiClaw プロジェクトに由来します。このフォークでは TFT LCD ディスプレイサポートと関連する改善が追加されています。

MimiClaw 自体は [OpenClaw](https://github.com/openclaw/openclaw) と [Nanobot](https://github.com/HKUDS/nanobot) にインスパイアされています。
