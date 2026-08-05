# Windows 原生界面模拟器

该程序在 Windows 上直接预览 SmallDesktopDisplay 的 240 × 240 显示界面，不需要 ESP8266 或屏幕。它使用固件仓库内的真实天气图标、温湿度图标、三套动画、LineAtom 数字字模和 VLW 中文字体，并通过 RGB565 帧缓冲复现屏幕合成。

## 直接使用便携包

1. 解压 `SmallDesktopDisplaySimulator-1.5.3-windows-x64.zip`，不要只单独复制 EXE。
2. 双击 `SmallDesktopDisplaySimulator.exe`。
3. 用右侧按钮切换场景、亮度、方向、动画、DHT、温度、湿度与 AQI。

便携目录中的 `SDL3.dll` 必须与 EXE 放在同一目录。程序使用静态 MSVC 运行库，目标电脑不需要另装 Visual C++ Redistributable。

## 从源码构建

要求 64 位 Windows 10/11，以及带“使用 C++ 的桌面开发”工作负载的 Visual Studio 2022 Build Tools 或 Visual Studio。脚本会自动定位 MSVC、CMake 和 Ninja，并从固定版本与 SHA-256 的官方归档构建 SDL3。

在仓库根目录运行：

```powershell
.\tools\build_simulator.ps1 -Clean -Run
```

首次构建需要联网下载 SDL3。后续产物位于：

```text
build\simulator\bin\Release\SmallDesktopDisplaySimulator.exe
```

运行测试与生成便携 ZIP：

```powershell
.\tools\test_simulator.ps1
.\tools\package_simulator.ps1 -SkipBuild
```

ZIP 输出到 `build\packages`。

## 操作

| 操作 | 鼠标/键盘 |
| --- | --- |
| 选择四个预置场景 | `1`、`2`、`3`、`4`，或 Scenario 按钮 |
| 亮度 | `B` |
| 旋转方向 | `R` |
| 太空人/胡桃/初音/关闭动画 | `A` |
| DHT 室内温湿度 | `D` |
| 暂停时钟与动画 | `Space` |
| 单步动画 | `Right` |
| 保存 240 × 240 BMP 截图 | `S` |
| 恢复当前场景 | `Home` |
| 退出 | `Esc` |

右侧按钮还能直接调整天气温度、湿度和 AQI，用于观察边界颜色、文字宽度和布局。

## 命令行与自动验证

```powershell
SmallDesktopDisplaySimulator.exe --self-test
SmallDesktopDisplaySimulator.exe --headless --frames 10 --screenshot preview.bmp
```

`--self-test` 校验字体、动画目录、JPEG 解码和帧缓冲转换；`--headless` 可在不打开窗口时生成确定性截图。

## 模拟范围

模拟器用于界面布局和资源回归，不是 ESP8266 指令级或外设时序模拟器。它不会模拟真实 Wi-Fi、HTTP/TLS、NTP、EEPROM/SPI 时序、DHT 电气行为、背光 PWM 或内存压力。固件发布前仍需执行 PlatformIO 构建矩阵和真机验收。
