# ESP32 Handled Console

基于 ESP32-S3 的多模态 AI 游戏掌机项目：LVGL 图形界面 + NES 游戏 + AI 文本/语音助手 + 图形化 AI 宠物 + BLE HID + Wi-Fi 音频 + 三固件切换。

> 硬件基线：实际 PCB 见 `Gerber_PCB1_2026-10-27/`，引脚以该文件夹的数据文件为准。当前板**没有电池 ADC/电量计**，电量显示与低电保护需外挂 I2C 电量计或改板；电池必须带保护板。详见 [PCB 核对与改板建议](docs/05-PCB核对与改板建议.md)。

## 文档

- [文档索引](docs/README.md)
- [系统框图](docs/01-系统框图.md)
- [子系统流程图](docs/02-子系统流程图.md)
- [项目实现计划](docs/03-项目实现计划.md)
- [参考项目分析](docs/04-参考项目分析.md)
- [PCB 核对与改板建议](docs/05-PCB核对与改板建议.md)

## 参考项目

- [OSHWHub：ESP32S3 多功能手柄](https://oshwhub.com/zistar/project_fhhysqix)
- [GitHub：zistar001/ESP-Handheld](https://github.com/zistar001/ESP-Handheld)
