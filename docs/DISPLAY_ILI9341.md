# ILI9341 TFT Display Integration

## 硬體規格

| 項目 | 說明 |
|---|---|
| 顯示器型號 | ILI9341（或相容型號） |
| 解析度 | 240 × 320 |
| 介面 | SPI（SPI2 / HSPI），硬體 DMA |
| MCU | ESP32-S3 |

## GPIO 接線（Kconfig 預設值）

| 訊號 | GPIO |
|---|---|
| CS | 12 |
| DC | 11 |
| RST | 13 |
| MOSI | 10 |
| SCLK | 46 |
| BL | -1（背光直接接 VCC） |

SPI 時脈預設 **40 MHz**。可透過 `idf.py menuconfig → MimiClaw Display` 修改所有腳位。

---

## 架構設計

### SPI 硬體 DMA

ILI9341 使用 ESP-IDF `spi_master` 驅動（SPI2_HOST），搭配 `SPI_DMA_CH_AUTO`，每次傳輸最大 4092 bytes。大於此值時自動分塊傳輸：

```c
while (rem > 0) {
    uint32_t chunk = (rem > 4092) ? 4092 : rem;
    spi_transaction_t t = { .length = chunk * 8, .tx_buffer = p };
    spi_device_polling_transmit(s_spi, &t);
    p += chunk; rem -= chunk;
}
```

與 RM68140 的 bit-bang GPIO 相比，SPI DMA 傳輸速度更快，ESP32-S3 不需要逐 byte 手動操作。

### Byte-swap（RGB565 端序）

LVGL 在記憶體中以 little-endian 儲存 RGB565（低 byte 先），但 SPI 以 MSB-first 傳輸。若不處理，畫面顏色會全部錯誤。

`display_service.c` 的 flush callback 在送出前對每個 pixel 做 byte-swap：

```c
// LVGL flush callback
for (int i = 0; i < w * h; i++) {
    uint16_t px = buf[i];
    buf[i] = (px >> 8) | (px << 8);   // byte-swap
}
ili9341_write_pixels(buf, w * h);
```

---

## 初始化序列

```
SWRESET (0x01)              → 等待 150 ms
SLPOUT  (0x11)              → 等待 120 ms
COLMOD  (0x3A) = 0x55       → 16-bit RGB565
MADCTL  (0x36) = 0x48       → MX | BGR，直向顯示
PWCTR1  (0xC0) = 0x23
PWCTR2  (0xC1) = 0x10
VMCTR1  (0xC5) = 0x3E 0x28
VMCTR2  (0xC7) = 0x86
FRMCTR1 (0xB1) = 0x00 0x18  → 70 Hz frame rate
DFUNCTR (0xB6) = 0x08 0x82 0x27
GAMMASET(0x26) = 0x01
GMCTRP1 (0xE0)              → 15 bytes positive gamma
GMCTRN1 (0xE1)              → 15 bytes negative gamma
SLPOUT  (0x11)              → 等待 120 ms（第二次，確保穩定）
DISPON  (0x29)              → 等待 20 ms
```

---

## 相關檔案

| 檔案 | 說明 |
|---|---|
| `main/display/ili9341.h` | 公開 API |
| `main/display/ili9341.c` | SPI DMA 驅動 |
| `main/display/display_service.c` | LVGL 整合、byte-swap、UI 邏輯 |
| `main/Kconfig.projbuild` | menuconfig GPIO 及 SPI 頻率設定 |

---

## LVGL 整合

- `lv_disp_drv_t` 直接註冊顯示驅動
- flush callback 執行 byte-swap 後呼叫 `ili9341_set_window()` + `ili9341_write_pixels()`
- 渲染緩衝區：240 × 20 rows，靜態 SRAM，單緩衝

---

## UI Layout（240 × 320）

```
y=  0– 23   Banner（24px）— 純文字「MimiClaw」（Montserrat 20）
y= 24–107   Status 區（84px）
              lbl_a: #808080 Provider:# openrouter
              lbl_b: #808080 Model:# minimax-m2.5
              lbl_ip: #808080 IP:# 192.168.x.x
y=108–119   漸層分隔線（12px）
y=120–319   對話氣泡區（200px，LVGL scroll container）
              User 訊息：右對齊，藍紫色泡泡（#5B6BE8）
              AI 訊息：左對齊，深色泡泡（#1E2C3A）
              System 訊息：置中，灰藍色泡泡（#2A3A4A）
```

> **注意**：ILI9341 解析度較小（240px 寬），Banner 改用純文字而非圖片（RM68140 的 320px banner 圖片不適用）。
