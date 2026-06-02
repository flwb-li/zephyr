# RAK3162 Zephyr 开发指南

> **适用对象**：首次使用 RAK3162 模块的开发者。
> **基准工程**：`~/rak_zephyr/zephyrproject/`（Zephyr v4.4.99，已集成 RAK3162 板级支持）。

---

## 目录

1. [RAK3162 模块概述](#1-rak3162-模块概述)
2. [开发环境搭建（从零开始）](#2-开发环境搭建从零开始)
3. [工程结构说明](#3-工程结构说明)
4. [编译第一个程序](#4-编译第一个程序)
5. [烧录固件](#5-烧录固件)
6. [创建自己的应用](#6-创建自己的应用)
    - [6.1 应用基本结构](#61-应用基本结构)
    - [6.2 基础示例：Hello LED](#62-基础示例hello-led)
    - [6.3 LoRaWAN 应用（集成 USP-Zephyr）](#63-lorawan-应用集成-usp-zephyr)
7. [常见问题排查](#7-常见问题排查)
8. [命令速查](#8-命令速查)

---

## 1. RAK3162 模块概述

RAK3162 是 RAKwireless WisDuo 系列 LPWAN 模块，基于 **Nordic nRF54L15** SoC（ARM Cortex-M33），集成 **Semtech SX1262** LoRa 收发器。

### 1.1 硬件规格

| 项目 | 参数 |
|------|------|
| 主控芯片 | Nordic nRF54L15 (Cortex-M33) |
| Flash | 1.4 MB |
| SRAM | 188 KB |
| LoRa 收发器 | Semtech SX1262 |
| 无线能力 | BLE 5.4 / LoRa / 802.15.4 |
| 晶振 | 32 MHz HFXO + 32.768 kHz LFXO |
| 用户 LED | 2 个 |
| 调试接口 | SWD |
| 工作温度 | -40°C ~ +85°C |

### 1.2 引脚分配

#### LED

| 名称 | GPIO |
|------|------|
| LED0 | P2.09 |
| LED1 | P2.10 |

#### 串口

| 名称 | 功能 | TX | RX |
|------|------|-----|-----|
| UART0 | 调试控制台 | P1.06 | P1.07 |
| UART1 | 辅助串口 | P2.08 | P2.07 |

调试串口默认波特率 **115200**，8N1。

#### I2C / SPI / LoRa 控制引脚

| 接口 | 用途 | SCK | MOSI | MISO | CS |
|------|------|-----|------|------|-----|
| SPI22 | LoRa SX1262 | P1.11 | P1.10 | P1.09 | P1.12 |
| SPI00 | 外部扩展 | P2.01 | P2.02 | P2.04 | P2.05 |

| 信号 | GPIO | 说明 |
|------|------|------|
| SX1262 RESET | P0.04 | 复位 |
| SX1262 BUSY | P1.13 | 忙信号 |
| SX1262 DIO1 | P0.01 | 中断 |
| ANT_SW | P0.00 | RF 天线开关 (RTC66006) |
| I2C SDA / SCL | P0.02 / P0.03 | — |

### 1.3 Flash 分区布局

```
0x000000 ┌──────────────┐
         │   MCUboot    │  64 KB  (bootloader)
0x010000 ├──────────────┤
         │   image-0    │  664 KB (slot0, 主固件)
0x0B6000 ├──────────────┤
         │   image-1    │  664 KB (slot1, OTA 升级)
0x15C000 ├──────────────┤
         │   storage    │  36 KB  (用户数据存储)
0x160000 └──────────────┘
```

模块使用 **MCUboot** 作为 bootloader，支持 OTA 固件升级。

---

## 2. 开发环境搭建（从零开始）

本章假设你已从 RAK 获取 Zephyr 源码包并解压到 `~/rak_zephyr/zephyrproject/`，但系统中尚未安装编译工具、SDK、Python 依赖。

### 2.1 确认 WSL2 已就绪

在 Windows PowerShell（管理员）中：

```powershell
wsl --install -d Ubuntu-22.04
```

重启后打开 Ubuntu，首次启动会提示创建用户名和密码。进入 WSL 后更新系统：

```bash
sudo apt update && sudo apt upgrade -y
```

### 2.2 安装系统依赖

```bash
sudo apt install --no-install-recommends \
    git cmake ninja-build gperf \
    ccache dfu-util device-tree-compiler \
    wget python3-dev python3-pip python3-setuptools \
    python3-tk python3-venv python3-wheel \
    xz-utils file make gcc gcc-multilib \
    g++-multilib libsdl2-dev libmagic1 udev \
    curl lcov
```

**验证关键工具：**

```bash
cmake --version    # >= 3.20.0
dtc --version
python3 --version  # Zephyr v4.4/main 需要 >= 3.12
```

Ubuntu 22.04 默认 Python 3.10 不满足要求，需安装 Python 3.12：

```bash
sudo apt install software-properties-common
sudo add-apt-repository ppa:deadsnakes/ppa
sudo apt update
sudo apt install python3.12 python3.12-venv python3.12-dev
```

### 2.3 安装 Zephyr SDK

Zephyr v4.4 需要 SDK >= 1.0.0（GCC 14.3）。

```bash
cd ~
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v1.0.1/zephyr-sdk-1.0.1_linux-x86_64_gnu.tar.xz

# 可选：校验下载完整性
wget -O - https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v1.0.1/sha256.sum | \
    sha256sum --check --ignore-missing

tar xf zephyr-sdk-1.0.1_linux-x86_64_gnu.tar.xz
cd zephyr-sdk-1.0.1
./setup.sh
```

安装脚本提示选择工具链时，**RAK3162 至少需要勾选 `arm-zephyr-eabi`**。其他架构按需选择。

安装 udev 规则（避免 sudo）：

```bash
sudo cp ~/zephyr-sdk-1.0.1/sysroots/x86_64-pokysdk-linux/usr/share/openocd/contrib/60-openocd.rules \
    /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

验证：

```bash
~/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc --version
# 应输出: arm-zephyr-eabi-gcc (Zephyr SDK 1.0.1) 14.3.0
```

### 2.4 Python 虚拟环境与 west

```bash
cd ~/rak_zephyr/zephyrproject

# 创建虚拟环境（必须使用 Python 3.12+）
python3.12 -m venv .venv
source .venv/bin/activate

# 安装 west
pip install --upgrade pip
pip install west

# 安装 Zephyr 编译所需的 Python 包
pip install -r zephyr/scripts/requirements.txt
```

### 2.5 获取源码：manifest 管理

Zephyr 项目的源码通过 `west.yml`（manifest）管理。有两种搭建方式：

| 方式 | 适用场景 |
|------|---------|
| **A: 使用 Zephyr 自带 west.yml** | 快速开始，workspace 即 `zephyrproject/` |
| **B: 自定义 manifest（推荐）** | 团队协作、需固定模块版本的生产工程 |

#### 方式 A：使用 Zephyr 自带 manifest

```bash
cd ~/rak_zephyr/zephyrproject
west init -l zephyr/        # 创建 .west/config，指向 zephyr/west.yml
west update                  # 拉取所有模块
west zephyr-export
```

如需添加额外模块（如 USP-Zephyr），后续直接在 `zephyr/west.yml` 中追加（见 6.3.2 节）。

#### 方式 B：自定义 manifest（推荐）

创建独立 workspace，用自己的 manifest 统一管理 Zephyr 版本和所有模块：

```text
~/rak3162_workspace/
├── manifest/west.yml       # 你的自定义 manifest
├── zephyr/                 # west update 拉取
├── modules/                # west update 拉取
├── bootloader/
└── my-app/                 # 你的应用
```

最小 `west.yml` 示例：

```yaml
manifest:
  remotes:
    - name: zephyrproject
      url-base: https://github.com/zephyrproject-rtos
    - name: lora-net
      url-base: https://github.com/Lora-net

  projects:
    - name: zephyr
      remote: zephyrproject
      repo-path: zephyr
      revision: main
      import: true            # 继续导入 Zephyr 官方的模块依赖

    - name: usp_zephyr
      remote: lora-net
      repo-path: usp_zephyr
      revision: v1.1.2-feature-202604
      path: modules/lib/usp_zephyr

    - name: usp
      remote: lora-net
      repo-path: usp
      revision: v1.1.2-feature-202604
      path: modules/lib/usp
      submodules: true

  self:
    path: manifest
```

初始化：

```bash
mkdir -p ~/rak3162_workspace/manifest
# 将 west.yml 写入 ~/rak3162_workspace/manifest/west.yml
cd ~/rak3162_workspace
west init -l manifest
west update
west zephyr-export
```

如果你的 RAK3162 board 在自定义 Zephyr fork 中，把 `zephyr` 项目的 `url-base` 和 `revision` 指向你的 fork 即可。

### 2.6 安装 J-Link 工具

```bash
wget --post-data "accept_license_agreement=accepted" \
    https://www.segger.com/downloads/jlink/JLink_Linux_x86_64.deb
sudo dpkg -i JLink_Linux_x86_64.deb

# 配置库搜索路径
echo 'export LD_LIBRARY_PATH=/opt/SEGGER/JLink:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc
```

### 2.7 USB 设备转发（Windows → WSL2）

WSL2 无法直接访问宿主机 USB 硬件。在 Windows PowerShell（管理员）中：

```powershell
winget install usbipd                           # 安装（只需一次）
usbipd list                                      # 找到 SEGGER J-Link (1366:0105) 的 BUSID
usbipd bind --busid <BUSID>                      # 绑定（只需一次）
usbipd attach --wsl --busid <BUSID>              # 附加到 WSL（每次 WSL 重启后需重新执行）
```

在 WSL 中验证：

```bash
lsusb | grep -i segger
# 应输出: Bus 001 Device XXX: ID 1366:0105 SEGGER J-Link
```

### 2.8 硬件连接

```
J-Link 调试器 (20-pin)        RAK3162 模块
═══════════════════════════    ══════════════
Pin 1  (VTref)    ─────────→   VDD (3.3V)
Pin 7  (SWDIO)    ─────────→   SWDIO
Pin 9  (SWCLK)    ─────────→   SWCLK
Pin 4  (GND)      ─────────→   GND
```

### 2.9 环境激活（日常使用）

以上安装步骤只需执行一次。**每次新开终端编译时**，建议将以下内容保存为 `~/rak_zephyr/env.sh`：

```bash
source ~/rak_zephyr/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/rak_zephyr/zephyrproject/zephyr
export LD_LIBRARY_PATH=/opt/SEGGER/JLink:$LD_LIBRARY_PATH
```

以后每次只需 `source ~/rak_zephyr/env.sh`。

验证环境：

```bash
west --version
cmake -P $ZEPHYR_BASE/cmake/verify-toolchain.cmake 2>&1 | grep -E "SDK_VERSION|ZEPHYR_SDK"
# 应输出: SDK_VERSION: 1.0.1
```

---

## 3. 工程结构说明

```
~/rak_zephyr/
└── zephyrproject/                ← Zephyr 主工程
    ├── .venv/                    → Python 虚拟环境
    ├── .west/                    → west 配置
    ├── zephyr/                   → Zephyr RTOS 内核源码 (v4.4.99)
    │   ├── boards/rakwireless/   → RAK 系列板级定义
    │   │   ├── rak3162/          → ★ RAK3162 板级支持包
    │   │   ├── rak4631/          → RAK4631 (nRF52840)
    │   │   └── ...
    │   ├── samples/              → Zephyr 官方示例
    │   └── modules/              → 模块 Kconfig 胶水
    ├── modules/
    │   ├── hal/                  → 硬件抽象层 (Nordic, STM32...)
    │   └── lib/                  → 协议库 (lora-basics-modem 等)
    ├── bootloader/mcuboot/       → MCUboot bootloader
    └── tools/
```

### 3.1 RAK3162 板级文件一览

| 文件 | 作用 |
|------|------|
| `board.yml` | 板卡元信息（名称、SoC） |
| `board.cmake` | 烧录/调试器配置（J-Link, pyOCD） |
| `rak3162_nrf54l15_cpuapp.dts` | 设备树顶层（Flash分区、外设使能） |
| `rak3162_common.dtsi` | 外设引脚与参数定义（LED、SPI、I2C、LoRa） |
| `rak3162-pinctrl.dtsi` | 引脚复用配置 |
| `rak3162_nrf54l15_cpuapp_defconfig` | 默认 Kconfig 配置 |
| `Kconfig.defconfig` | 板级 Kconfig 默认选项 |

以上文件均位于 `boards/rakwireless/rak3162/`。

---

## 4. 编译第一个程序

### 4.1 Hello World（验证环境）

```bash
source ~/rak_zephyr/env.sh
cd ~/rak_zephyr/zephyrproject/zephyr

west build -b rak3162/nrf54l15/cpuapp samples/hello_world --pristine always
```

产物在 `build/zephyr/` 下：

| 产物 | 文件名 | 用途 |
|------|--------|------|
| HEX 固件 | `zephyr.hex` | J-Link 烧录 |
| BIN 固件 | `zephyr.bin` | 原始二进制 |
| ELF 文件 | `zephyr.elf` | 调试符号 |

### 4.2 编译参数说明

```bash
west build -b rak3162/nrf54l15/cpuapp <应用路径> [选项]
```

| 参数 | 说明 |
|------|------|
| `-b rak3162/nrf54l15/cpuapp` | 目标板：RAK3162，nRF54L15 应用核 |
| `--pristine always` | 每次编译前清空 build 目录 |
| `--no-sysbuild` | 不使用多镜像系统构建（单应用场景） |
| `-- -DCONFIG_XXX=y` | 通过命令行覆盖 Kconfig 选项 |

### 4.3 在任意目录编译

应用可以放在 `zephyrproject` 之外的任意目录，`west build` 通过 `$ZEPHYR_BASE` 找到内核即可。最小示例见 [6.2 节](#62-基础示例hello-led)。

---

## 5. 烧录固件

RAK3162 应用固件默认链接到 `slot0_partition`（地址 `0x10000`）。全新模块需先烧录 MCUboot（见 5.3 节）。

### 5.1 烧录方式

| Runner | 命令 | 适用场景 |
|--------|------|----------|
| **J-Link** | `west flash --runner jlink` | WSL 环境首选 |
| pyOCD | `west flash --runner pyocd` | 原生 Linux |
| nrfjprog | `west flash --runner nrfjprog` | Nordic 官方工具 |

> **重要**：WSL2 环境下必须使用 J-Link runner。pyOCD 在 WSL2 中会因 `ctypes` 加载 `libjlinkarm.so` 时段错误（SIGSEGV），详见第 7 章。

### 5.2 烧录命令

```bash
west flash --runner jlink                  # 在 build 目录执行
west flash --runner jlink --build-dir build
```

**一键编译+烧录：**

```bash
west build -b rak3162/nrf54l15/cpuapp samples/hello_world --pristine always \
    && west flash --runner jlink
```

### 5.3 编译和烧录 MCUboot

MCUboot 占据 Flash 0x000000~0x010000（64 KB），全新模块需先烧录，只需一次。

```bash
cd ~/rak_zephyr/zephyrproject/bootloader/mcuboot/boot/zephyr
source ~/rak_zephyr/env.sh

west build -b rak3162/nrf54l15/cpuapp . --pristine always
west flash --runner jlink
```

烧录后连接串口（UART0, 115200），按复位键，应看到：

```
*** Booting Zephyr OS build v4.4.99 ***
I: Starting bootloader...
I: Jumping to the first image slot
```

> 后续日常开发只需编译和烧录应用固件到 slot0，MCUboot 无需重复烧录。

### 5.4 查看串口输出

```bash
ls /dev/ttyACM* /dev/ttyUSB*               # 确认设备名
minicom -D /dev/ttyACM0 -b 115200          # J-Link 虚拟串口
# 或 USB-TTL 模块: /dev/ttyUSB0
```

**Hello World 预期输出：**

```
*** Booting Zephyr OS build v4.4.99 ***
Hello World! rak3162/nrf54l15/cpuapp
```

---

## 6. 创建自己的应用

### 6.1 应用基本结构

一个 Zephyr 应用的最小目录结构：

```
my-app/
├── CMakeLists.txt              ← 构建入口
├── prj.conf                    ← 应用级 Kconfig 配置
├── boards/                     ← 板级定制（可选）
│   └── rak3162_nrf54l15_cpuapp.overlay
└── src/
    └── main.c                  ← 应用入口
```

**CMakeLists.txt** — 通过 `find_package(Zephyr)` 拉入内核和所有已注册模块：

```cmake
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(my_app)
target_sources(app PRIVATE src/main.c)
```

**prj.conf** — 应用级 Kconfig：

```ini
CONFIG_SERIAL=y
CONFIG_CONSOLE=y
CONFIG_UART_CONSOLE=y
CONFIG_GPIO=y
CONFIG_LOG=y
```

**boards/*.overlay** — 设备树覆盖文件，文件名必须与目标板匹配。基础示例不需要 overlay。

应用目录可以放在任意位置，不要求放在 `zephyrproject` 里面。`west build` 通过 `$ZEPHYR_BASE` 找到内核即可。

### 6.2 基础示例：Hello LED

让 RAK3162 的 LED0 以 500 ms 间隔闪烁。

**目录结构：**

```
~/my-rak-app/
├── CMakeLists.txt
├── prj.conf
└── src/
    └── main.c
```

**CMakeLists.txt：**

```cmake
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_led)
target_sources(app PRIVATE src/main.c)
```

**prj.conf：**

```ini
CONFIG_SERIAL=y
CONFIG_CONSOLE=y
CONFIG_UART_CONSOLE=y
CONFIG_GPIO=y
CONFIG_LOG=y
```

**src/main.c：**

```c
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)

int main(void)
{
    const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

    if (!gpio_is_ready_dt(&led)) {
        LOG_ERR("LED not ready");
        return 0;
    }

    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

    while (1) {
        gpio_pin_toggle_dt(&led);
        k_sleep(K_MSEC(500));
    }
    return 0;
}
```

**编译和烧录：**

```bash
west build -b rak3162/nrf54l15/cpuapp ~/my-rak-app --pristine always
west flash --runner jlink
```

---

### 6.3 LoRaWAN 应用（集成 USP-Zephyr）

USP-Zephyr 是 Semtech 提供的 Zephyr 集成层，将 LoRa Basics Modem (LBM) 协议栈对接到 Zephyr RTOS。

```
┌──────────────────────────────────────────┐
│  你的应用程序 (main.c)                    │
│  调用 smtc_modem_init / smtc_modem_xxx   │
├──────────────────────────────────────────┤
│  usp_zephyr (Zephyr 集成层)              │  ← github.com/Lora-net/usp_zephyr
│  线程管理 / 消息队列 / HAL 适配 / Kconfig │
├──────────────────────────────────────────┤
│  usp (Universal Serial Protocol)         │  ← github.com/Lora-net/usp
│  RAC (Radio Abstraction Core)            │
├──────────────────────────────────────────┤
│  lora-basics-modem (LBM)                │  ← 已在 modules/lib/ 中
│  LoRaWAN 协议栈 (JOIN/TX/RX/MAC)        │
├──────────────────────────────────────────┤
│  SX1262 驱动 + Zephyr RTOS + RAK3162    │
└──────────────────────────────────────────┘
```

> **注意**：RAK3162 板级默认使用 Zephyr 原生 `semtech,sx1262` 绑定；使用 USP-Zephyr 时需通过 overlay 切换为 `semtech,sx1262-new`（见 6.3.4 节）。

#### 6.3.1 获取 USP-Zephyr 模块

**推荐：通过 manifest 管理。** 在 `west.yml` 中添加 USP 模块（详见 2.5 节方式 B 的完整示例），然后：

```bash
cd ~/rak3162_workspace   # 或你的 workspace 目录
west update
```

核心是在 `west.yml` 的 `projects` 中添加：

```yaml
- name: usp_zephyr
  remote: lora-net
  repo-path: usp_zephyr
  revision: v1.1.2-feature-202604
  path: modules/lib/usp_zephyr

- name: usp
  remote: lora-net
  repo-path: usp
  revision: v1.1.2-feature-202604
  path: modules/lib/usp
  submodules: true
```

**备选：手动 clone（仅限临时调试）。**

```bash
cd ~/rak_zephyr/zephyrproject
git clone https://github.com/Lora-net/usp_zephyr.git modules/lib/usp_zephyr
git clone https://github.com/Lora-net/usp.git modules/lib/usp
cd modules/lib/usp && git submodule update --init --recursive

# 在 CMakeLists.txt 的 find_package(Zephyr) 之前添加：
# set(ZEPHYR_EXTRA_MODULES "modules/lib/usp_zephyr" "modules/lib/usp" CACHE STRING "" FORCE)
```

#### 6.3.2 应用配置（prj.conf）

```ini
# ==================== 关闭 Zephyr 原生 LoRa/LoRaWAN ====================
# CONFIG_LORA is not set
# CONFIG_LORAWAN is not set

# ==================== 日志 ====================
CONFIG_LOG=y
CONFIG_LOG_MODE_IMMEDIATE=y

# ==================== LoRa 射频驱动 ====================
CONFIG_LORA_BASICS_MODEM_DRIVERS=y
CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_GLOBAL_THREAD=y

# ==================== USP 核心 ====================
CONFIG_USP=y
CONFIG_ZEPHYR_USP_MODULE=y
CONFIG_USP_THREADS_MUTEXES=y

# ==================== LoRaWAN 协议栈 ====================
CONFIG_USP_LORA_BASICS_MODEM=y

# ==================== 线程 ====================
CONFIG_USP_MAIN_THREAD=y
CONFIG_USP_MAIN_THREAD_STACK_SIZE=4096
CONFIG_USP_MAIN_THREAD_PRIORITY=-4

# ==================== C 库（USP 需要完整 libc） ====================
CONFIG_NEWLIB_LIBC=y
CONFIG_PICOLIBC_USE_MODULE=n

# ==================== 基础外设 ====================
CONFIG_SPI=y
CONFIG_GPIO=y
CONFIG_SERIAL=y
CONFIG_HEAP_MEM_POOL_SIZE=4096
CONFIG_MAIN_STACK_SIZE=4096

# ==================== 低功耗（可选） ====================
CONFIG_PM_DEVICE=y
CONFIG_PM_DEVICE_POWER_DOMAIN=y
CONFIG_REBOOT=y
```

**Zephyr 原生 vs USP 关键 Kconfig 对照：**

| 功能 | Zephyr 原生 | USP-Zephyr |
|------|------------|------------|
| 启用 LoRa/LoRaWAN | `CONFIG_LORA=y` / `CONFIG_LORAWAN=y` | 均设为 `n` |
| 射频驱动 | 内置于 `loramac-node` | `CONFIG_LORA_BASICS_MODEM_DRIVERS=y` |
| 协议栈 | `subsys/lorawan` | `CONFIG_USP=y` + `CONFIG_USP_LORA_BASICS_MODEM=y` |
| C 库 | 默认 picolibc | `CONFIG_NEWLIB_LIBC=y`（必须） |

#### 6.3.3 设备树 Overlay

在应用目录下创建 **一个** `boards/rak3162_nrf54l15_cpuapp.overlay`，同时负责：密钥/区域配置、切换 USP 绑定、添加 `chosen` 节点。

```dts
/*
 * USP-Zephyr overlay for RAK3162.
 * Replaces Zephyr's native "semtech,sx1262" binding with USP's "semtech,sx1262-new".
 */
#include <zephyr/dt-bindings/usp/sx126x.h>

/ {
    chosen {
        zephyr,lorawan-transceiver = &lora;
    };

    zephyr,user {
        /* LoRaWAN Device EUI (8 bytes, LSB first) */
        user-lorawan-device-eui = <0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08>;

        /* LoRaWAN Join EUI (8 bytes) */
        user-lorawan-join-eui = <0x0E 0x22 0x0D 0x38 0x41 0x01 0x23 0x0E>;

        /* LoRaWAN Gen App Key (16 bytes) */
        user-lorawan-gen_app-key = <0x2B 0x7E 0x15 0x52 0x28 0xAE 0xD2 0x55
                                     0xAB 0xF7 0x33 0x88 0x09 0xCF 0x00 0x3E>;

        /* LoRaWAN App Key / Nwk Key (16 bytes) */
        user-lorawan-app-key = <0x2B 0x7E 0x15 0x52 0x28 0xAE 0xD2 0x55
                                 0xAB 0xF7 0x33 0x88 0x09 0xCF 0x00 0x3E>;

        /* LoRaWAN 区域 */
        user-lorawan-region = "AS_923_GRP1";
    };
};

&lora {
    compatible = "semtech,sx1262-new";

    reset-gpios = <&gpio0 4 GPIO_ACTIVE_LOW>;
    busy-gpios  = <&gpio1 13 GPIO_ACTIVE_HIGH>;
    dio1-gpios  = <&gpio0 1 (GPIO_ACTIVE_HIGH | GPIO_PULL_DOWN)>;

    /delete-property/ dio2-tx-enable;
    dio2-as-rf-switch;

    /delete-property/ dio3-tcxo-voltage;
    dio3-as-tcxo-control;
    tcxo-voltage = <SX126X_TCXO_SUPPLY_1_8V>;

    /delete-property/ tcxo-power-startup-delay-ms;
    tcxo-wakeup-time = <5>;

    reg-mode = <SX126X_REG_MODE_LDO>;
};

&ieee802154 {
    status = "disabled";
};
```

**两套绑定的关键属性对照：**

| 含义 | Zephyr 原生 (`semtech,sx1262`) | USP (`semtech,sx1262-new`) |
|------|-------------------------------|---------------------------|
| 绑定头文件 | `<zephyr/dt-bindings/lora/sx126x.h>` | `<zephyr/dt-bindings/usp/sx126x.h>` |
| DIO2 控制 RF 开关 | `dio2-tx-enable;` | `dio2-as-rf-switch;` |
| DIO3 TCXO 控制 | (由 `dio3-tcxo-voltage` 隐含) | `dio3-as-tcxo-control;` |
| TCXO 电压 | `dio3-tcxo-voltage = <SX126X_DIO3_TCXO_1V8>;` | `tcxo-voltage = <SX126X_TCXO_SUPPLY_1_8V>;` |
| TCXO 启动延时 | `tcxo-power-startup-delay-ms = <5>;` | `tcxo-wakeup-time = <5>;` |
| 电源模式 | **无此属性** | `reg-mode = <SX126X_REG_MODE_LDO>;` **（必须）** |

> **关键**：如果不改 `compatible`，USP 驱动不会被启用，运行时 `smtc_rac_open_radio()` 将失败。

**支持的频段区域：** `EU_868` / `US_915` / `AS_923_GRP1` / `AU_915` / `CN_470` / `IN_865` / `KR_920` / `RU_864`

#### 6.3.4 完整 LoRaWAN 示例

> 不同 `usp_zephyr` / `usp` 版本的 API 可能存在差异，请以实际使用的 sample 和头文件为准。

**目录结构：**

```
~/my-lorawan-app/
├── CMakeLists.txt
├── prj.conf                         ← 见 6.3.2 节
├── boards/
│   └── rak3162_nrf54l15_cpuapp.overlay  ← 见 6.3.3 节
└── src/
    └── main.c
```

**src/main.c：**

```c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <smtc_modem_api.h>
#include <smtc_modem_utilities.h>
#include <smtc_modem_hal.h>
#include <smtc_zephyr_usp_api.h>
#include <smtc_sw_platform_helper.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define STACK_ID 0

/* 从设备树读取密钥 */
static const uint8_t dev_eui[8]  = DT_PROP(DT_PATH(zephyr_user), user_lorawan_device_eui);
static const uint8_t join_eui[8] = DT_PROP(DT_PATH(zephyr_user), user_lorawan_join_eui);
static const uint8_t app_key[16] = DT_PROP(DT_PATH(zephyr_user), user_lorawan_gen_app_key);
static const uint8_t nwk_key[16] = DT_PROP(DT_PATH(zephyr_user), user_lorawan_app_key);

#define REGION DT_CAT(SMTC_MODEM_REGION_, \
    DT_STRING_UNQUOTED(DT_PATH(zephyr_user), user_lorawan_region))

/* LoRaWAN 事件回调 - 在 USP/RAC 线程中运行 */
static void modem_event_callback(void)
{
    smtc_modem_event_t event = { 0 };
    uint8_t pending = 0;

    smtc_modem_get_event(&event, &pending);

    switch (event.event_type) {
    case SMTC_MODEM_EVENT_RESET:
        LOG_INF("RESET - configuring and joining...");
        smtc_modem_set_deveui(STACK_ID, dev_eui);
        smtc_modem_set_joineui(STACK_ID, join_eui);
        smtc_modem_set_appkey(STACK_ID, app_key);
        smtc_modem_set_nwkkey(STACK_ID, nwk_key);
        smtc_modem_set_region(STACK_ID, REGION);
        smtc_modem_join_network(STACK_ID);
        break;

    case SMTC_MODEM_EVENT_JOINED:
        LOG_INF("JOINED - network joined!");
        break;

    case SMTC_MODEM_EVENT_TXDONE:
        LOG_INF("TXDONE - uplink sent");
        break;

    case SMTC_MODEM_EVENT_DOWNDATA:
        LOG_INF("DOWNDATA - downlink received");
        break;

    case SMTC_MODEM_EVENT_JOINFAIL:
        LOG_WRN("JOINFAIL - retrying...");
        break;

    default:
        break;
    }
}

int main(void)
{
    LOG_INF("LoRaWAN app starting...");

    SMTC_SW_PLATFORM_INIT();
    SMTC_SW_PLATFORM_VOID(smtc_rac_init());
    SMTC_SW_PLATFORM_VOID(smtc_modem_init(&modem_event_callback));

    while (1) {
        k_sleep(K_SECONDS(1));
    }
    return 0;
}
```

**编译：**

```bash
cd ~/rak_zephyr/zephyrproject
west build -b rak3162/nrf54l15/cpuapp /path/to/my-lorawan-app --pristine always
west flash --runner jlink
```

#### 6.3.5 USP 线程模型

**模式 A：独立线程（推荐，`CONFIG_USP_MAIN_THREAD=y`）**

```
IRQ (射频中断)
  └─→ USP/RAC 线程 (优先级高)
        └─→ 处理射频事务
        └─→ 发信号量给主线程
              └─→ 主线程
                    └─→ modem_event_callback()
                    └─→ 用户业务逻辑
```

USP 框架自动管理 RAC 线程，用户只需在回调中处理 LoRaWAN 事件。

**模式 B：手动轮询（`CONFIG_USP_MAIN_THREAD=n`）**

```c
while (1) {
    uint32_t sleep_ms = smtc_modem_run_engine();
    smtc_rac_run_engine();
    if (smtc_rac_is_irq_flag_pending()) {
        continue;
    }
    k_sleep(K_MSEC(sleep_ms));
}
```

适用于简单场景，用户自行在循环中驱动 USP 引擎。

#### 6.3.6 从 Zephyr 原生 LoRaWAN 迁移到 USP

Zephyr 内核自带的 LoRaWAN 子系统（`subsys/lorawan`，基于 `loramac-node`）与 USP-Zephyr **完全互不兼容**。迁移涉及以下四个层面：

| # | 文件 | 改动 | 不正确的后果 |
|---|------|------|-------------|
| 1 | `west.yml` / `CMakeLists.txt` | 通过 manifest 添加 `usp_zephyr` + `usp`（见 6.3.1 节）；手动 clone 时在 `find_package` 前声明 `ZEPHYR_EXTRA_MODULES` | `CONFIG_USP` 等符号不可见，Kconfig 报错 |
| 2 | `prj.conf` | 关闭 `CONFIG_LORA`/`CONFIG_LORAWAN`，启用 `CONFIG_USP` + `CONFIG_NEWLIB_LIBC`（见 6.3.2 节） | 链接找不到 `floorf` 等数学函数，或符号冲突 |
| 3 | `boards/*.overlay` | **改 `compatible` 为 `semtech,sx1262-new`**，适配属性名差异，设置 `reg-mode`（见 6.3.3 节） | USP 驱动不被启用，`smtc_rac_open_radio()` 失败 |
| 4 | `src/*.c` | 用 `smtc_modem_*` / `smtc_rac_*` 替换 `lorawan_*` API（见 6.3.4 节示例） | 编译错误，API 不存在 |

**USP 驱动为何必须改 compatible：** `usp_zephyr` 的 SX126x 驱动通过 `DT_HAS_SEMTECH_SX1262_NEW_ENABLED` 符号启用，该符号由构建系统根据设备树中是否存在 `compatible = "semtech,sx1262-new"` 自动生成。使用 Zephyr 原生 `"semtech,sx1262"` 不会生成此符号，USP 驱动被静默跳过。

#### 6.3.7 版本兼容性说明

`usp_zephyr` 和 `usp` 是 Semtech 维护的外部仓库，**强依赖 Zephyr 内核版本**。三个仓库必须作为一个整体进行版本管理。

| Zephyr 版本 | usp_zephyr / usp 版本 | 状态 |
|------------|----------------------|------|
| v4.3.0 | `v1.1.2-feature-202604` | 验证通过 |
| **v4.4.99** | **`v1.1.2-feature-202604`** | **验证通过（本工程版本）** |
| v4.4.x | main 分支（v1.0.0） | 不兼容（board.yml 格式不符） |
| v3.x | - | 不兼容 |

v4.4.99 + `v1.1.2-feature-202604` 编译通过，需注意：
- overlay 必须覆盖 `compatible` 为 `semtech,sx1262-new` 并删除旧属性
- overlay 必须添加 `chosen { zephyr,lorawan-transceiver = &lora; }`
- 会有 `NEWLIB_LIBC` 选择冲突和 `NRF_PLATFORM_LUMOS` 废弃警告，不影响编译

**更换版本：** 修改 `west.yml` 中 `usp_zephyr` 和 `usp` 的 `revision`，然后：

```bash
cd ~/rak_zephyr/zephyrproject
west update usp_zephyr usp
rm -rf build
west build -b rak3162/nrf54l15/cpuapp <应用路径> --pristine always
```

**排查版本不匹配：**

```bash
cat $ZEPHYR_BASE/VERSION                              # Zephyr 版本
git -C modules/lib/usp_zephyr describe --tags --always # usp_zephyr 版本
git -C modules/lib/usp describe --tags --always        # usp 版本
```

| 症状 | 可能原因 |
|------|---------|
| `CONFIG_USP` 在 menuconfig 中不可见 | usp_zephyr 未注册或版本不匹配 |
| `arm-zephyr-eabi-gcc: error: ...usp_zephyr/...c: No such file` | usp 子模块未初始化 |
| 链接报错 `undefined reference to __device_dts_ord_...` | 设备树绑定解析不兼容 |
| 编译报错 `implicit declaration of function ...` | Zephyr 头文件/API 版本变化 |

---

## 7. 常见问题排查

### 7.1 环境问题

**Q: `west: command not found`**

```bash
source ~/rak_zephyr/zephyrproject/.venv/bin/activate
# 如果 .venv 中没有 west：pip install west
```

**Q: `CMake Error: ZEPHYR_BASE not set`**

```bash
export ZEPHYR_BASE=~/rak_zephyr/zephyrproject/zephyr
```

**Q: `arm-zephyr-eabi-gcc: not found`**

确认 SDK 已安装：`ls ~/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc`。若未安装，参考 2.3 节。

**Q: Python 模块缺失 `ModuleNotFoundError`**

```bash
source ~/rak_zephyr/zephyrproject/.venv/bin/activate
pip install -r ~/rak_zephyr/zephyrproject/zephyr/scripts/requirements.txt
```

### 7.2 烧录问题

**Q: `lsusb` 看不到 J-Link**

USB 未从 Windows 转发到 WSL。在 Windows PowerShell（管理员）：

```powershell
usbipd list                        # 找到 J-Link (1366:0105) 的 BUSID
usbipd attach --wsl --busid <BUSID>
```

**Q: pyOCD 段错误（SIGSEGV）**

WSL2 下 `ctypes` 加载 `libjlinkarm.so` 时内存布局异常导致崩溃。**解决方案：使用 J-Link runner。**

```bash
west flash --runner jlink
```

**Q: `JLinkExe` 报 `Cannot connect to J-Link`**

关闭 Windows 端的 J-Link 相关程序；或 WSL 重启后重新执行 `usbipd attach`。

**Q: 测试 J-Link 能否连接芯片**

```bash
echo -e "connect\nexit" | JLinkExe -device nRF54L15_M33 -if SWD -speed 4000 \
    -autoconnect 1 -NoGui 1 -CommandFile /dev/stdin 2>&1 | head -20
```

成功标志：`Connecting to J-Link via USB...O.K.` / `Device "NRF54L15_M33" selected.`

### 7.3 编译问题

**Q: `ERROR: board rak3162/nrf54l15/cpuapp not found`**

```bash
ls ~/rak_zephyr/zephyrproject/zephyr/boards/rakwireless/rak3162/
# 文件存在则检查 $ZEPHYR_BASE 是否正确
```

**Q: Kconfig 找不到 `CONFIG_USP`**

USP-Zephyr 模块未被构建系统发现：

```bash
# 确认 usp_zephyr 存在
ls ~/rak_zephyr/zephyrproject/modules/lib/usp_zephyr/module.yml
# 确认 usp 子模块已初始化
ls ~/rak_zephyr/zephyrproject/modules/lib/usp/smtc_rac_lib/
# 通过 manifest 管理则执行 west update
west update usp_zephyr usp
```

**Q: 链接时找不到 `floorf` 等数学函数**

```ini
CONFIG_NEWLIB_LIBC=y
CONFIG_PICOLIBC_USE_MODULE=n
```

### 7.4 运行时问题

**Q: `smtc_rac_open_radio()` 返回 `RAC_INVALID_RADIO_ID`**

USP 驱动未被启用，通常因为 overlay 没改 `compatible`：

```bash
grep -r "sx1262" build/zephyr/zephyr.dts | head -5
# 应看到 compatible = "semtech,sx1262-new"，不是 "semtech,sx1262"
```

**Q: 加入网络失败 (JOINFAIL)**

1. 检查密钥与 LoRaWAN 网络服务器一致
2. 检查 `user-lorawan-region` 与实际地区匹配
3. 确保附近有对应频段的网关在运行
4. 确认天线正确连接

**Q: 串口没有输出**

```bash
ls /dev/ttyACM* /dev/ttyUSB*          # 确认设备存在（WSL 中串口也需 usbipd 转发）
minicom -D /dev/ttyACM0 -b 115200     # 乱码则检查波特率
```

---

## 8. 命令速查

| 场景 | 命令 |
|------|------|
| 激活环境 | `source ~/rak_zephyr/env.sh` |
| 编译 Hello World | `west build -b rak3162/nrf54l15/cpuapp samples/hello_world --pristine always` |
| 编译 MCUboot | `cd bootloader/mcuboot/boot/zephyr && west build -b rak3162/nrf54l15/cpuapp . --pristine always` |
| 烧录 (J-Link) | `west flash --runner jlink` |
| 编译 + 烧录 | `west build ... && west flash --runner jlink` |
| 编译 USP 应用 | `west build -b rak3162/nrf54l15/cpuapp <应用路径> --pristine always` |
| 串口查看 | `minicom -D /dev/ttyACM0 -b 115200`（或 `/dev/ttyUSB0`） |
| 清除编译产物 | `rm -rf build` |
| 查看 J-Link 连接 | `lsusb \| grep -i segger` |
| USB 转发到 WSL | `usbipd attach --wsl --busid <BUSID>` (PowerShell) |
| 测试 J-Link 连芯片 | `echo -e "connect\nexit" \| JLinkExe -device nRF54L15_M33 -if SWD -speed 4000 -autoconnect 1 -NoGui 1 -CommandFile /dev/stdin` |
| 更新 USP 模块 | `cd ~/rak_zephyr/zephyrproject && west update usp_zephyr usp` |
| 查看 USP 版本 | `git -C modules/lib/usp_zephyr describe --tags --always` |
| 验证工具链 | `cmake -P $ZEPHYR_BASE/cmake/verify-toolchain.cmake` |
| 重载 udev | `sudo udevadm control --reload-rules && sudo udevadm trigger` |
