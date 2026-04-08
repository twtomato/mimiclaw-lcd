# RM68140 TFT Display Integration

## 硬體規格

| 項目 | 說明 |
|---|---|
| 顯示器型號 | RM68140（或相容型號） |
| 解析度 | 320 × 480 |
| 介面 | 8-bit 8080 parallel（i80） |
| MCU | ESP32-S3 |

## GPIO 接線（Kconfig 預設值）

| 訊號 | GPIO |
|---|---|
| CS | 6 |
| DC | 7 |
| RST | 5 |
| WR | 1 |
| RD | 2（接 HIGH，僅寫入） |
| BL | -1（背光直接接 VCC） |
| D0–D7 | 21, 15, 18, 17, 19, 20, 3, 14 |

可透過 `idf.py menuconfig → MimiClaw Display` 修改。

---

## 架構設計

### 為何使用 bit-bang GPIO 而非 esp_lcd i80？

原本採用 ESP-IDF `esp_lcd` 框架的 i80 硬體周邊（DMA 傳輸），但無論如何調整參數，畫面始終全白。診斷過程確認 i80 匯流排的資料確實送達顯示器（DMA 啟用後雪花消失），但顯示器無法正確解讀。

改用純 GPIO bit-bang（與 Arduino TFT_eSPI 相同方式）後立即正常顯示。

**結論：此型號顯示器模組與 ESP32-S3 i80 DMA 硬體周邊不相容，使用 bit-bang 為必要選擇。**

### 效能優化

原始 bit-bang 使用 `gpio_set_level()`，每個 byte 需要 10 次函數呼叫（~2000 ns），全畫面刷新約 600 ms。

改用 **GPIO 暫存器直寫 + 256 entry lookup table**：

```c
// 初始化時預計算 byte → GPIO bitmask
static uint32_t s_byte_mask[256];
static uint32_t s_data_mask;

// 寫入一個 byte（4 次暫存器寫入，每次約 4 CPU cycles）
static inline void write8(uint8_t byte) {
    REG_WRITE(GPIO_OUT_W1TC_REG, s_data_mask);           // 清除 D0-D7
    REG_WRITE(GPIO_OUT_W1TS_REG, s_byte_mask[byte]);     // 設定 D0-D7
    REG_WRITE(GPIO_OUT_W1TC_REG, 1u << MIMI_RM_WR_PIN); // WR low
    REG_WRITE(GPIO_OUT_W1TS_REG, 1u << MIMI_RM_WR_PIN); // WR high
}
```

效能提升約 10×，全畫面刷新降至 ~30–60 ms。

---

## 初始化序列

```
SWRESET (0x01)  → 等待 200 ms
SLPOUT  (0x11)  → 等待 250 ms
COLMOD  (0x3A)  → 0x55（16-bit RGB565）
MADCTL  (0x36)  → 0x48（MX + BGR，與 TFT_eSPI 預設一致）
DISPON  (0x29)  → 等待 50 ms
```

---

## 相關檔案

| 檔案 | 說明 |
|---|---|
| `main/display/rm68140.h` | 公開 API |
| `main/display/rm68140.c` | bit-bang GPIO 驅動 |
| `main/display/display_service.c` | LVGL 整合與 UI 邏輯 |
| `main/Kconfig.projbuild` | menuconfig GPIO 設定 |
| `sdkconfig.defaults` | 預設啟用 RM68140 + LVGL 設定 |

---

## LVGL 整合

移除 `esp_lvgl_port`，改用原生 LVGL API：

- `lv_disp_drv_t` 直接註冊顯示驅動
- flush callback 呼叫 `rm68140_set_window()` + `rm68140_write_pixels()`
- LVGL task 運行於 CPU1（避免與系統任務爭用 WDT）
- 畫面更新 task 運行於 CPU0
- 渲染緩衝區：320 × 20 rows，靜態 SRAM，單緩衝

---

## UI Layout（320 × 480）

```
y= 14   MimiClaw                  標題（Montserrat 20，白色）
y= 50   ──────────────────────    分隔線
y= 66   Provider:  openrouter     lbl_a（主要資訊）
y=100   Model:     minimax-m2.5   lbl_b（次要資訊）
y=130   IP:        192.168.x.x    lbl_bot（IP，灰色）
y=150   Thinking... / 192.168.4.1 lbl_c（僅 Thinking / WiFi 失敗狀態顯示）
```

Key/value 雙色顯示採用 LVGL recolor 語法：

```c
lv_label_set_recolor(lbl, true);
lv_label_set_text(lbl, "#808080 Provider:# openrouter");
//                       ^灰色 key^  ^預設色 value^
```

---

## 獨立測試專案

`d:\Work\rm68140_test\` — 不含 MimiClaw 業務邏輯的最小化測試專案，可用於驗證硬體接線與顯示器相容性。
