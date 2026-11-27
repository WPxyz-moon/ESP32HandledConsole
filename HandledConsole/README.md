# Firmware · Stage 0 Skeleton

三个独立 ESP-IDF 工程，共享 `components/`：

```text
HandledConsole/
  CMakeLists.txt     # factory 主系统工程
  main/              # factory 应用入口
  components/        # bsp_* 驱动、svc_fw_role、svc_event、svc_ota、
                     # svc_system、svc_settings、app_core
  game/              # 游戏系统（ota_0）
  ai/                # AI 系统（ota_1）
  partitions.csv     # 16MB 分区表
```

Stage 0 只验证：

- 三个工程都能编译、烧录；
- 共享组件能同时编进三个固件；
- `factory → game → ai → factory` 分区切换可用；
- 串口能看到当前分区、PSRAM 大小、事件总线收发。

## BSP 驱动层（阶段 1）

`components/bsp_*` 已实现并可编译：

| 组件 | 内容 |
|---|---|
| `bsp_board` | 早期初始化 + 完整 bring-up 顺序：NVS、屏、键、IMU、音频、存储、电源 |
| `bsp_display` | ST7789 240×280、SPI2 DMA、背光 LEDC、初始化重试 3 次 |
| `bsp_input` | 8 键中断 + 20 ms 消抖 + 长按 + 100 ms 重复 |
| `bsp_imu` | LSM6DS3TR-C I2C 轮询；连续 3 次失败复位 I2C |
| `bsp_audio` | I2S0 麦克风 + I2S1 功放、环形缓冲、音量/采样率 |
| `bsp_storage` | NVS / microSD / assets SPIFFS，SD 启动重试 3 次 |
| `bsp_power` | LED 与 Light Sleep 接口 |

`bsp_board_init()` 会依次初始化以上模块，但**不点亮背光**；应用在显示第一帧后
调用 `bsp_display_set_backlight()`。如果固件只用部分外设，可直接调用单独的
`bsp_*_init()`，不经过 `bsp_board_init()`。

控制台固定使用 **USB Serial/JTAG（GPIO19/20）**。UART0 的 GPIO43/44 是
LCD_DC/LCD_BL，不能再用来打日志。驱动接口与验收见
[06-驱动层设计](../docs/06-驱动层设计.md)。

## VS Code

用 VS Code **直接打开 `HandledConsole` 文件夹**作为工作区，不要打开仓库根目录。

ESP-IDF 扩展把当前工作区根目录当成工程目录；如果打开的是仓库根目录，它会执行：

```text
cmake -S D:\Develop\Porjects\ESP32HandledConsole ...
```

于是报 `source directory ... does not appear to contain CMakeLists.txt`。打开 `HandledConsole` 后，`-S` 才会指向包含 `CMakeLists.txt` 的目录。

`game` / `ai` 是独立工程，分别在 VS Code 里打开 `HandledConsole/game`、`HandledConsole/ai` 编译。

## 构建

每个工程独立构建：

```powershell
cd HandledConsole
idf.py set-target esp32s3
idf.py build

cd game
idf.py set-target esp32s3
idf.py build

cd ../ai
idf.py set-target esp32s3
idf.py build
```

## 烧录

先烧 `factory`（会写入 bootloader、分区表和 factory 应用）：

```powershell
cd HandledConsole
idf.py -p COM5 flash monitor
```

`game` / `ai` 用 esptool 按分区偏移写入，**不要用 `idf.py flash`**，否则会覆盖 `factory`：

```powershell
esptool.py --chip esp32s3 -p COM5 -b 921600 write_flash 0x410000 game/build/game.bin
esptool.py --chip esp32s3 -p COM5 -b 921600 write_flash 0x810000 ai/build/ai.bin
```

如果 `esptool.py` 不在 PATH，使用 `python -m esptool`。

## 预期日志

```text
I (xxx) factory: ==== ESP32-S3 Handheld Rev.A boot ====
I (xxx) factory: running partition: factory
I (xxx) factory: event bus ok, type=0
I (xxx) app_manager: started app: skeleton
I (xxx) ota_manager: boot partition set to ota_0, restarting
I (xxx) game: ==== ESP32-S3 Handheld Rev.A boot ====
...
I (xxx) ai: ==== ESP32-S3 Handheld Rev.A boot ====
...
```

## 关闭自动切换

Stage 0 默认每 3 秒切到下一个固件，用于验证 OTA 切换。正式开发时把各工程 `main/main.c` 里的：

```c
#define AUTO_SWITCH 1
```

改成 `0`。

## 注意

- 三个工程必须使用同一份 `HandledConsole/partitions.csv`。
- `sdkconfig`、`build/` 不提交；`sdkconfig.defaults` 提交。
- 本阶段不初始化屏幕、按键、音频；这些在 Stage 1 加。
- 当前硬件无电量计，固件不做电量显示、低电保护和充电状态显示。
