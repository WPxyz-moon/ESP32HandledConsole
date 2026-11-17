# Firmware · Stage 0 Skeleton

三个独立 ESP-IDF 工程，共享 `components/`：

```text
HandledConsole/
  CMakeLists.txt     # factory 主系统工程
  main/              # factory 应用入口
  components/        # bsp_board, svc_event, app_core, svc_ota
  game/              # 游戏系统（ota_0）
  ai/                # AI 系统（ota_1）
  partitions.csv     # 16MB 分区表
```

Stage 0 只验证：

- 三个工程都能编译、烧录；
- 共享组件能同时编进三个固件；
- `factory → game → ai → factory` 分区切换可用；
- 串口能看到当前分区、PSRAM 大小、事件总线收发。

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
