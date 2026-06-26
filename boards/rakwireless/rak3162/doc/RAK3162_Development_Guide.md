# RAK3162 Zephyr Development Guide

> **Audience**: Developers getting started with the RAK3162 module.
> **Base project**: `~/rak_zephyr/zephyrproject/` (Zephyr v4.4.99 with RAK3162 board support).

---

## Table of Contents

1. [RAK3162 Module Overview](#1-rak3162-module-overview)
2. [Development Environment Setup (from scratch)](#2-development-environment-setup-from-scratch)
3. [Project Structure](#3-project-structure)
4. [Building Your First Program](#4-building-your-first-program)
5. [Flashing Firmware](#5-flashing-firmware)
6. [Creating Your Own Application](#6-creating-your-own-application)
    - [6.1 Basic Application Structure](#61-basic-application-structure)
    - [6.2 Basic Example: Hello LED](#62-basic-example-hello-led)
    - [6.3 LoRaWAN Application (USP-Zephyr Integration)](#63-lorawan-application-usp-zephyr-integration)
7. [Troubleshooting](#7-troubleshooting)
8. [Command Quick Reference](#8-command-quick-reference)

---

## 1. RAK3162 Module Overview

The RAK3162 is a WisDuo series LPWAN module from RAKwireless, based on the **Nordic nRF54L15** SoC (ARM Cortex-M33), with an integrated **Semtech SX1262** LoRa transceiver.

### 1.1 Hardware Specifications

| Item | Parameter |
|------|-----------|
| MCU | Nordic nRF54L15 (Cortex-M33) |
| Flash | 1.4 MB |
| SRAM | 188 KB |
| LoRa Transceiver | Semtech SX1262 |
| Wireless | BLE 5.4 / LoRa / 802.15.4 |
| Crystal Oscillators | 32 MHz HFXO + 32.768 kHz LFXO |
| User LEDs | 2 |
| Debug Interface | SWD |
| Operating Temperature | -40°C ~ +85°C |

### 1.2 Pin Assignment

#### LED

| Name | GPIO |
|------|------|
| LED0 | P2.09 |
| LED1 | P2.10 |

#### Serial Ports

| Name | Function | TX | RX |
|------|----------|-----|-----|
| UART0 | Debug Console | P1.06 | P1.07 |
| UART1 | Auxiliary UART | P2.08 | P2.07 |

Debug UART default baud rate: **115200**, 8N1.

#### I2C / SPI / LoRa Control Pins

| Interface | Usage | SCK | MOSI | MISO | CS |
|-----------|-------|-----|------|------|-----|
| SPI22 | LoRa SX1262 | P1.11 | P1.10 | P1.09 | P1.12 |
| SPI00 | External Expansion | P2.01 | P2.02 | P2.04 | P2.05 |

| Signal | GPIO | Description |
|--------|------|-------------|
| SX1262 RESET | P0.04 | Reset |
| SX1262 BUSY | P1.13 | Busy signal |
| SX1262 DIO1 | P0.01 | Interrupt |
| ANT_SW | P0.00 | RF antenna switch (RTC66006) |
| I2C SDA / SCL | P0.02 / P0.03 | — |

### 1.3 Flash Partition Layout

```
0x000000 ┌──────────────┐
         │   MCUboot    │  64 KB  (bootloader)
0x010000 ├──────────────┤
         │   image-0    │  664 KB (slot0, primary firmware)
0x0B6000 ├──────────────┤
         │   image-1    │  664 KB (slot1, OTA upgrade)
0x15C000 ├──────────────┤
         │   storage    │  36 KB  (user data storage)
0x160000 └──────────────┘
```

The module uses **MCUboot** as the bootloader and supports OTA firmware upgrades.

---

## 2. Development Environment Setup (from scratch)

This chapter assumes you have obtained the Zephyr source package from RAK and extracted it to `~/rak_zephyr/zephyrproject/`, but the system does not yet have the build tools, SDK, or Python dependencies installed.

### 2.1 Verify WSL2 is Ready

In Windows PowerShell (as Administrator):

```powershell
wsl --install -d Ubuntu-22.04
```

After rebooting, open Ubuntu. The first launch will prompt you to create a username and password. Once inside WSL, update the system:

```bash
sudo apt update && sudo apt upgrade -y
```

### 2.2 Install System Dependencies

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

**Verify key tools:**

```bash
cmake --version    # >= 3.20.0
dtc --version
python3 --version  # Zephyr v4.4/main requires >= 3.12
```

Ubuntu 22.04 ships Python 3.10 by default, which does not meet the requirement. Install Python 3.12:

```bash
sudo apt install software-properties-common
sudo add-apt-repository ppa:deadsnakes/ppa
sudo apt update
sudo apt install python3.12 python3.12-venv python3.12-dev
```

### 2.3 Install Zephyr SDK

Zephyr v4.4 requires SDK >= 1.0.0 (GCC 14.3).

```bash
cd ~
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v1.0.1/zephyr-sdk-1.0.1_linux-x86_64_gnu.tar.xz

# Optional: verify download integrity
wget -O - https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v1.0.1/sha256.sum | \
    sha256sum --check --ignore-missing

tar xf zephyr-sdk-1.0.1_linux-x86_64_gnu.tar.xz
cd zephyr-sdk-1.0.1
./setup.sh
```

When the setup script prompts for toolchain selection, **RAK3162 requires at least `arm-zephyr-eabi`**. Select other architectures as needed.

Install udev rules (to avoid needing sudo):

```bash
sudo cp ~/zephyr-sdk-1.0.1/sysroots/x86_64-pokysdk-linux/usr/share/openocd/contrib/60-openocd.rules \
    /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Verify:

```bash
~/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc --version
# Expected output: arm-zephyr-eabi-gcc (Zephyr SDK 1.0.1) 14.3.0
```

### 2.4 Python Virtual Environment and west

```bash
cd ~/rak_zephyr/zephyrproject

# Create virtual environment (must use Python 3.12+)
python3.12 -m venv .venv
source .venv/bin/activate

# Install west
pip install --upgrade pip
pip install west

# Install Python packages required by Zephyr
pip install -r zephyr/scripts/requirements.txt
```

### 2.5 Obtaining Source Code: Manifest Management

The Zephyr project source code is managed via `west.yml` (manifest). There are two approaches:

| Approach | When to Use |
|----------|-------------|
| **A: Use Zephyr's built-in west.yml** | Quick start, workspace = `zephyrproject/` |
| **B: Custom manifest (recommended)** | Team collaboration, production projects requiring fixed module versions |

#### Approach A: Use Zephyr's Built-in Manifest

```bash
cd ~/rak_zephyr/zephyrproject
west init -l zephyr/        # Create .west/config, pointing to zephyr/west.yml
west update                  # Fetch all modules
west zephyr-export
```

If you need additional modules (e.g., USP-Zephyr), add them to `zephyr/west.yml` later (see section 6.3.2).

#### Approach B: Custom Manifest (Recommended)

Create an independent workspace with your own manifest to manage Zephyr version and all modules uniformly:

```text
~/rak3162_workspace/
├── manifest/west.yml       # Your custom manifest
├── zephyr/                 # Pulled by west update
├── modules/                # Pulled by west update
├── bootloader/
└── my-app/                 # Your application
```

Minimal `west.yml` example:

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
      import: true            # Continue importing Zephyr official module dependencies

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

Initialization:

```bash
mkdir -p ~/rak3162_workspace/manifest
# Write west.yml to ~/rak3162_workspace/manifest/west.yml
cd ~/rak3162_workspace
west init -l manifest
west update
west zephyr-export
```

If your RAK3162 board support is in a custom Zephyr fork, change the `zephyr` project's `url-base` and `revision` to point to your fork.

### 2.6 Install J-Link Tools

```bash
wget --post-data "accept_license_agreement=accepted" \
    https://www.segger.com/downloads/jlink/JLink_Linux_x86_64.deb
sudo dpkg -i JLink_Linux_x86_64.deb

# Configure library search path
echo 'export LD_LIBRARY_PATH=/opt/SEGGER/JLink:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc
```

### 2.7 USB Device Forwarding (Windows → WSL2)

WSL2 cannot directly access host USB hardware. In Windows PowerShell (as Administrator):

```powershell
winget install usbipd                           # Install (one-time)
usbipd list                                      # Find SEGGER J-Link (1366:0105) BUSID
usbipd bind --busid <BUSID>                      # Bind (one-time)
usbipd attach --wsl --busid <BUSID>              # Attach to WSL (re-run after each WSL restart)
```

Verify in WSL:

```bash
lsusb | grep -i segger
# Expected output: Bus 001 Device XXX: ID 1366:0105 SEGGER J-Link
```

### 2.8 Hardware Connection

```
J-Link Debugger (20-pin)        RAK3162 Module
═══════════════════════════      ══════════════
Pin 1  (VTref)    ─────────→     VDD (3.3V)
Pin 7  (SWDIO)    ─────────→     SWDIO
Pin 9  (SWCLK)    ─────────→     SWCLK
Pin 4  (GND)      ─────────→     GND
```

### 2.9 Environment Activation (Daily Use)

The above installation steps only need to be performed once. **For every new terminal session when building**, save the following as `~/rak_zephyr/env.sh`:

```bash
source ~/rak_zephyr/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/rak_zephyr/zephyrproject/zephyr
export LD_LIBRARY_PATH=/opt/SEGGER/JLink:$LD_LIBRARY_PATH
```

Afterwards, simply run `source ~/rak_zephyr/env.sh` each time.

Verify the environment:

```bash
west --version
cmake -P $ZEPHYR_BASE/cmake/verify-toolchain.cmake 2>&1 | grep -E "SDK_VERSION|ZEPHYR_SDK"
# Expected output: SDK_VERSION: 1.0.1
```

---

## 3. Project Structure

```
~/rak_zephyr/
└── zephyrproject/                ← Zephyr main project
    ├── .venv/                    → Python virtual environment
    ├── .west/                    → west configuration
    ├── zephyr/                   → Zephyr RTOS kernel source (v4.4.99)
    │   ├── boards/rakwireless/   → RAK series board definitions
    │   │   ├── rak3162/          → ★ RAK3162 board support package
    │   │   ├── rak4631/          → RAK4631 (nRF52840)
    │   │   └── ...
    │   ├── samples/              → Zephyr official samples
    │   └── modules/              → Module Kconfig glue
    ├── modules/
    │   ├── hal/                  → Hardware Abstraction Layer (Nordic, STM32...)
    │   └── lib/                  → Protocol libraries (lora-basics-modem, etc.)
    ├── bootloader/mcuboot/       → MCUboot bootloader
    └── tools/
```

### 3.1 RAK3162 Board File Overview

| File | Purpose |
|------|---------|
| `board.yml` | Board metadata (name, SoC) |
| `board.cmake` | Flashing/debugger configuration (J-Link, pyOCD) |
| `rak3162_nrf54l15_cpuapp.dts` | Top-level device tree (flash partitions, peripheral enable) |
| `rak3162_common.dtsi` | Peripheral pin and parameter definitions (LED, SPI, I2C, LoRa) |
| `rak3162-pinctrl.dtsi` | Pin mux configuration |
| `rak3162_nrf54l15_cpuapp_defconfig` | Default Kconfig configuration |
| `Kconfig.defconfig` | Board-level Kconfig default options |

All above files are located under `boards/rakwireless/rak3162/`.

---

## 4. Building Your First Program

### 4.1 Hello World (Verify Environment)

```bash
source ~/rak_zephyr/env.sh
cd ~/rak_zephyr/zephyrproject/zephyr

west build -b rak3162/nrf54l15/cpuapp samples/hello_world --pristine always
```

Build artifacts are under `build/zephyr/`:

| Artifact | Filename | Purpose |
|----------|----------|---------|
| HEX firmware | `zephyr.hex` | J-Link flashing |
| BIN firmware | `zephyr.bin` | Raw binary |
| ELF file | `zephyr.elf` | Debug symbols |

### 4.2 Build Parameters

```bash
west build -b rak3162/nrf54l15/cpuapp <app-path> [options]
```

| Parameter | Description |
|-----------|-------------|
| `-b rak3162/nrf54l15/cpuapp` | Target board: RAK3162, nRF54L15 application core |
| `--pristine always` | Clean build directory before each build |
| `--no-sysbuild` | Disable multi-image system build (single application scenario) |
| `-- -DCONFIG_XXX=y` | Override Kconfig options via command line |

### 4.3 Building from Any Directory

Applications can be placed in any directory outside `zephyrproject`. `west build` locates the kernel via `$ZEPHYR_BASE`. See [section 6.2](#62-basic-example-hello-led) for a minimal example.

---

## 5. Flashing Firmware

RAK3162 application firmware is linked to `slot0_partition` (address `0x10000`) by default. A brand-new module requires MCUboot to be flashed first (see section 5.3).

### 5.1 Flashing Methods

| Runner | Command | When to Use |
|--------|---------|-------------|
| **J-Link** | `west flash --runner jlink` | Preferred for WSL |
| pyOCD | `west flash --runner pyocd` | Native Linux |
| nrfjprog | `west flash --runner nrfjprog` | Nordic official tools |

> **Important**: In WSL2, you must use the J-Link runner. pyOCD will crash with a segfault (SIGSEGV) in WSL2 due to `ctypes` loading `libjlinkarm.so`. See chapter 7 for details.

### 5.2 Flashing Commands

```bash
west flash --runner jlink                  # Run from the build directory
west flash --runner jlink --build-dir build
```

**One-shot build + flash:**

```bash
west build -b rak3162/nrf54l15/cpuapp samples/hello_world --pristine always \
    && west flash --runner jlink
```

### 5.3 Building and Flashing MCUboot

MCUboot occupies flash 0x000000~0x010000 (64 KB). A brand-new module must have MCUboot flashed first — this is a one-time step.

```bash
cd ~/rak_zephyr/zephyrproject/bootloader/mcuboot/boot/zephyr
source ~/rak_zephyr/env.sh

west build -b rak3162/nrf54l15/cpuapp . --pristine always
west flash --runner jlink
```

After flashing, connect the serial port (UART0, 115200) and press reset. You should see:

```
*** Booting Zephyr OS build v4.4.99 ***
I: Starting bootloader...
I: Jumping to the first image slot
```

> For daily development, you only need to build and flash application firmware to slot0; MCUboot does not need to be re-flashed.

### 5.4 Viewing Serial Output

```bash
ls /dev/ttyACM* /dev/ttyUSB*               # Confirm device name
minicom -D /dev/ttyACM0 -b 115200          # J-Link virtual serial port
# Or USB-TTL module: /dev/ttyUSB0
```

**Hello World expected output:**

```
*** Booting Zephyr OS build v4.4.99 ***
Hello World! rak3162/nrf54l15/cpuapp
```

---

## 6. Creating Your Own Application

### 6.1 Basic Application Structure

Minimal directory structure for a Zephyr application:

```
my-app/
├── CMakeLists.txt              ← Build entry point
├── prj.conf                    ← Application-level Kconfig configuration
├── boards/                     ← Board-specific customization (optional)
│   └── rak3162_nrf54l15_cpuapp.overlay
└── src/
    └── main.c                  ← Application entry point
```

**CMakeLists.txt** — Pulls in the kernel and all registered modules via `find_package(Zephyr)`:

```cmake
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(my_app)
target_sources(app PRIVATE src/main.c)
```

**prj.conf** — Application-level Kconfig:

```ini
CONFIG_SERIAL=y
CONFIG_CONSOLE=y
CONFIG_UART_CONSOLE=y
CONFIG_GPIO=y
CONFIG_LOG=y
```

**boards/*.overlay** — Device tree overlay file. The filename must match the target board. The basic example does not need an overlay.

The application directory can be placed anywhere, not necessarily inside `zephyrproject`. `west build` locates the kernel via `$ZEPHYR_BASE`.

### 6.2 Basic Example: Hello LED

Blink LED0 on the RAK3162 at 500 ms intervals.

**Directory structure:**

```
~/my-rak-app/
├── CMakeLists.txt
├── prj.conf
└── src/
    └── main.c
```

**CMakeLists.txt:**

```cmake
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_led)
target_sources(app PRIVATE src/main.c)
```

**prj.conf:**

```ini
CONFIG_SERIAL=y
CONFIG_CONSOLE=y
CONFIG_UART_CONSOLE=y
CONFIG_GPIO=y
CONFIG_LOG=y
```

**src/main.c:**

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

**Build and flash:**

```bash
west build -b rak3162/nrf54l15/cpuapp ~/my-rak-app --pristine always
west flash --runner jlink
```

---

### 6.3 LoRaWAN Application (USP-Zephyr Integration)

USP-Zephyr is Semtech's Zephyr integration layer that bridges the LoRa Basics Modem (LBM) protocol stack to Zephyr RTOS.

```
┌──────────────────────────────────────────┐
│  Your Application (main.c)               │
│  Calls smtc_modem_init / smtc_modem_xxx  │
├──────────────────────────────────────────┤
│  usp_zephyr (Zephyr Integration Layer)   │  ← github.com/Lora-net/usp_zephyr
│  Thread mgmt / Message Queue / HAL /     │
│  Kconfig                                 │
├──────────────────────────────────────────┤
│  usp (Universal Serial Protocol)         │  ← github.com/Lora-net/usp
│  RAC (Radio Abstraction Core)            │
├──────────────────────────────────────────┤
│  lora-basics-modem (LBM)                 │  ← Already in modules/lib/
│  LoRaWAN Stack (JOIN/TX/RX/MAC)          │
├──────────────────────────────────────────┤
│  SX1262 Driver + Zephyr RTOS + RAK3162   │
└──────────────────────────────────────────┘
```

> **Note**: The RAK3162 board defaults to the Zephyr native `semtech,sx1262` binding. When using USP-Zephyr, you must switch to `semtech,sx1262-new` via an overlay (see section 6.3.4).

#### 6.3.1 Obtaining USP-Zephyr Modules

**Recommended: Manage via manifest.** Add USP modules to `west.yml` (see the complete example in section 2.5, approach B), then:

```bash
cd ~/rak3162_workspace   # or your workspace directory
west update
```

The key addition is in `west.yml` under `projects`:

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

**Alternative: Manual clone (temporary debugging only).**

```bash
cd ~/rak_zephyr/zephyrproject
git clone https://github.com/Lora-net/usp_zephyr.git modules/lib/usp_zephyr
git clone https://github.com/Lora-net/usp.git modules/lib/usp
cd modules/lib/usp && git submodule update --init --recursive

# Add before find_package(Zephyr) in CMakeLists.txt:
# set(ZEPHYR_EXTRA_MODULES "modules/lib/usp_zephyr" "modules/lib/usp" CACHE STRING "" FORCE)
```

#### 6.3.2 Application Configuration (prj.conf)

```ini
# ==================== Disable Zephyr Native LoRa/LoRaWAN ====================
# CONFIG_LORA is not set
# CONFIG_LORAWAN is not set

# ==================== Logging ====================
CONFIG_LOG=y
CONFIG_LOG_MODE_IMMEDIATE=y

# ==================== LoRa Radio Driver ====================
CONFIG_LORA_BASICS_MODEM_DRIVERS=y
CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_GLOBAL_THREAD=y

# ==================== USP Core ====================
CONFIG_USP=y
CONFIG_ZEPHYR_USP_MODULE=y
CONFIG_USP_THREADS_MUTEXES=y

# ==================== LoRaWAN Stack ====================
CONFIG_USP_LORA_BASICS_MODEM=y

# ==================== Threading ====================
CONFIG_USP_MAIN_THREAD=y
CONFIG_USP_MAIN_THREAD_STACK_SIZE=4096
CONFIG_USP_MAIN_THREAD_PRIORITY=-4

# ==================== C Library (USP requires full libc) ====================
CONFIG_NEWLIB_LIBC=y
CONFIG_PICOLIBC_USE_MODULE=n

# ==================== Basic Peripherals ====================
CONFIG_SPI=y
CONFIG_GPIO=y
CONFIG_SERIAL=y
CONFIG_HEAP_MEM_POOL_SIZE=4096
CONFIG_MAIN_STACK_SIZE=4096

# ==================== Low Power (optional) ====================
CONFIG_PM_DEVICE=y
CONFIG_PM_DEVICE_POWER_DOMAIN=y
CONFIG_REBOOT=y
```

**Zephyr Native vs USP key Kconfig comparison:**

| Function | Zephyr Native | USP-Zephyr |
|----------|---------------|-------------|
| Enable LoRa/LoRaWAN | `CONFIG_LORA=y` / `CONFIG_LORAWAN=y` | Both set to `n` |
| Radio Driver | Built into `loramac-node` | `CONFIG_LORA_BASICS_MODEM_DRIVERS=y` |
| Stack | `subsys/lorawan` | `CONFIG_USP=y` + `CONFIG_USP_LORA_BASICS_MODEM=y` |
| C Library | Default picolibc | `CONFIG_NEWLIB_LIBC=y` (required) |

#### 6.3.3 Device Tree Overlay

Create **one** `boards/rak3162_nrf54l15_cpuapp.overlay` in your application directory. It handles: key/region configuration, switching to USP binding, and adding the `chosen` node.

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

        /* LoRaWAN Region */
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

**Key property comparison between the two bindings:**

| Meaning | Zephyr Native (`semtech,sx1262`) | USP (`semtech,sx1262-new`) |
|---------|----------------------------------|----------------------------|
| Binding header | `<zephyr/dt-bindings/lora/sx126x.h>` | `<zephyr/dt-bindings/usp/sx126x.h>` |
| DIO2 RF switch control | `dio2-tx-enable;` | `dio2-as-rf-switch;` |
| DIO3 TCXO control | (implied by `dio3-tcxo-voltage`) | `dio3-as-tcxo-control;` |
| TCXO voltage | `dio3-tcxo-voltage = <SX126X_DIO3_TCXO_1V8>;` | `tcxo-voltage = <SX126X_TCXO_SUPPLY_1_8V>;` |
| TCXO startup delay | `tcxo-power-startup-delay-ms = <5>;` | `tcxo-wakeup-time = <5>;` |
| Power mode | **No such property** | `reg-mode = <SX126X_REG_MODE_LDO>;` **(required)** |

> **Critical**: If you do not change `compatible`, the USP driver will not be enabled, and `smtc_rac_open_radio()` will fail at runtime.

**Supported frequency regions:** `EU_868` / `US_915` / `AS_923_GRP1` / `AU_915` / `CN_470` / `IN_865` / `KR_920` / `RU_864`

#### 6.3.4 Complete LoRaWAN Example

> APIs may differ between `usp_zephyr` / `usp` versions. Always refer to the actual samples and headers for your version.

**Directory structure:**

```
~/my-lorawan-app/
├── CMakeLists.txt
├── prj.conf                         ← See section 6.3.2
├── boards/
│   └── rak3162_nrf54l15_cpuapp.overlay  ← See section 6.3.3
└── src/
    └── main.c
```

**src/main.c:**

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

/* Read keys from device tree */
static const uint8_t dev_eui[8]  = DT_PROP(DT_PATH(zephyr_user), user_lorawan_device_eui);
static const uint8_t join_eui[8] = DT_PROP(DT_PATH(zephyr_user), user_lorawan_join_eui);
static const uint8_t app_key[16] = DT_PROP(DT_PATH(zephyr_user), user_lorawan_gen_app_key);
static const uint8_t nwk_key[16] = DT_PROP(DT_PATH(zephyr_user), user_lorawan_app_key);

#define REGION DT_CAT(SMTC_MODEM_REGION_, \
    DT_STRING_UNQUOTED(DT_PATH(zephyr_user), user_lorawan_region))

/* LoRaWAN event callback - runs in USP/RAC thread context */
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

**Build:**

```bash
cd ~/rak_zephyr/zephyrproject
west build -b rak3162/nrf54l15/cpuapp /path/to/my-lorawan-app --pristine always
west flash --runner jlink
```

#### 6.3.5 USP Threading Model

**Mode A: Independent Threads (Recommended, `CONFIG_USP_MAIN_THREAD=y`)**

```
IRQ (Radio Interrupt)
  └─→ USP/RAC Thread (high priority)
        └─→ Handle radio tasks
        └─→ Signal semaphore to main thread
              └─→ Main Thread
                    └─→ modem_event_callback()
                    └─→ User business logic
```

The USP framework automatically manages the RAC thread. Users only need to handle LoRaWAN events in the callback.

**Mode B: Manual Polling (`CONFIG_USP_MAIN_THREAD=n`)**

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

Suitable for simple scenarios. Users drive the USP engine manually in a loop.

#### 6.3.6 Migrating from Zephyr Native LoRaWAN to USP

The Zephyr kernel's built-in LoRaWAN subsystem (`subsys/lorawan`, based on `loramac-node`) and USP-Zephyr are **completely incompatible**. Migration involves four layers:

| # | File | Change | Consequence if incorrect |
|---|------|--------|--------------------------|
| 1 | `west.yml` / `CMakeLists.txt` | Add `usp_zephyr` + `usp` via manifest (see 6.3.1); when cloning manually, declare `ZEPHYR_EXTRA_MODULES` before `find_package` | `CONFIG_USP` and other symbols are invisible; Kconfig errors |
| 2 | `prj.conf` | Disable `CONFIG_LORA`/`CONFIG_LORAWAN`, enable `CONFIG_USP` + `CONFIG_NEWLIB_LIBC` (see 6.3.2) | Linker cannot find `floorf` and other math functions, or symbol conflicts |
| 3 | `boards/*.overlay` | **Change `compatible` to `semtech,sx1262-new`**, adapt property name differences, set `reg-mode` (see 6.3.3) | USP driver is not enabled; `smtc_rac_open_radio()` fails |
| 4 | `src/*.c` | Replace `lorawan_*` API with `smtc_modem_*` / `smtc_rac_*` (see 6.3.4 example) | Compilation error, API does not exist |

**Why the USP driver requires a compatible change:** The `usp_zephyr` SX126x driver is enabled via the `DT_HAS_SEMTECH_SX1262_NEW_ENABLED` symbol, which is auto-generated by the build system when `compatible = "semtech,sx1262-new"` is present in the device tree. Using the Zephyr native `"semtech,sx1262"` does not generate this symbol, and the USP driver is silently skipped.

#### 6.3.7 Version Compatibility

`usp_zephyr` and `usp` are external repositories maintained by Semtech. They **strongly depend on the Zephyr kernel version**. All three repositories must be versioned as a unit.

| Zephyr Version | usp_zephyr / usp Version | Status |
|----------------|--------------------------|--------|
| v4.3.0 | `v1.1.2-feature-202604` | Verified |
| **v4.4.99** | **`v1.1.2-feature-202604`** | **Verified (this project version)** |
| v4.4.x | main branch (v1.0.0) | Incompatible (board.yml format mismatch) |
| v3.x | - | Incompatible |

v4.4.99 + `v1.1.2-feature-202604` builds successfully. Notes:
- The overlay must override `compatible` to `semtech,sx1262-new` and delete old properties
- The overlay must add `chosen { zephyr,lorawan-transceiver = &lora; }`
- There will be `NEWLIB_LIBC` selection conflicts and `NRF_PLATFORM_LUMOS` deprecation warnings; these do not affect the build

**Switching versions:** Modify `revision` for `usp_zephyr` and `usp` in `west.yml`, then:

```bash
cd ~/rak_zephyr/zephyrproject
west update usp_zephyr usp
rm -rf build
west build -b rak3162/nrf54l15/cpuapp <app-path> --pristine always
```

**Diagnosing version mismatches:**

```bash
cat $ZEPHYR_BASE/VERSION                              # Zephyr version
git -C modules/lib/usp_zephyr describe --tags --always # usp_zephyr version
git -C modules/lib/usp describe --tags --always        # usp version
```

| Symptom | Likely Cause |
|---------|--------------|
| `CONFIG_USP` not visible in menuconfig | usp_zephyr not registered or version mismatch |
| `arm-zephyr-eabi-gcc: error: ...usp_zephyr/...c: No such file` | usp submodule not initialized |
| Linker error `undefined reference to __device_dts_ord_...` | Device tree binding resolution incompatible |
| Compile error `implicit declaration of function ...` | Zephyr header/API version changed |

---

## 7. Troubleshooting

### 7.1 Environment Issues

**Q: `west: command not found`**

```bash
source ~/rak_zephyr/zephyrproject/.venv/bin/activate
# If west is not in .venv: pip install west
```

**Q: `CMake Error: ZEPHYR_BASE not set`**

```bash
export ZEPHYR_BASE=~/rak_zephyr/zephyrproject/zephyr
```

**Q: `arm-zephyr-eabi-gcc: not found`**

Confirm the SDK is installed: `ls ~/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc`. If not installed, see section 2.3.

**Q: Python module missing `ModuleNotFoundError`**

```bash
source ~/rak_zephyr/zephyrproject/.venv/bin/activate
pip install -r ~/rak_zephyr/zephyrproject/zephyr/scripts/requirements.txt
```

### 7.2 Flashing Issues

**Q: `lsusb` does not show J-Link**

USB has not been forwarded from Windows to WSL. In Windows PowerShell (as Administrator):

```powershell
usbipd list                        # Find J-Link (1366:0105) BUSID
usbipd attach --wsl --busid <BUSID>
```

**Q: pyOCD segfault (SIGSEGV)**

In WSL2, `ctypes` loading `libjlinkarm.so` causes abnormal memory layout and crash. **Solution: Use the J-Link runner.**

```bash
west flash --runner jlink
```

**Q: `JLinkExe` reports `Cannot connect to J-Link`**

Close any J-Link related programs on the Windows side; or re-run `usbipd attach` after WSL restart.

**Q: Test if J-Link can connect to the chip**

```bash
echo -e "connect\nexit" | JLinkExe -device nRF54L15_M33 -if SWD -speed 4000 \
    -autoconnect 1 -NoGui 1 -CommandFile /dev/stdin 2>&1 | head -20
```

Success indicators: `Connecting to J-Link via USB...O.K.` / `Device "NRF54L15_M33" selected.`

### 7.3 Build Issues

**Q: `ERROR: board rak3162/nrf54l15/cpuapp not found`**

```bash
ls ~/rak_zephyr/zephyrproject/zephyr/boards/rakwireless/rak3162/
# If the files exist, check whether $ZEPHYR_BASE is set correctly
```

**Q: Kconfig cannot find `CONFIG_USP`**

The USP-Zephyr module is not being discovered by the build system:

```bash
# Confirm usp_zephyr exists
ls ~/rak_zephyr/zephyrproject/modules/lib/usp_zephyr/module.yml
# Confirm usp submodule is initialized
ls ~/rak_zephyr/zephyrproject/modules/lib/usp/smtc_rac_lib/
# If managed via manifest, run west update
west update usp_zephyr usp
```

**Q: Linker cannot find `floorf` and other math functions**

```ini
CONFIG_NEWLIB_LIBC=y
CONFIG_PICOLIBC_USE_MODULE=n
```

### 7.4 Runtime Issues

**Q: `smtc_rac_open_radio()` returns `RAC_INVALID_RADIO_ID`**

The USP driver is not enabled, usually because the overlay did not change `compatible`:

```bash
grep -r "sx1262" build/zephyr/zephyr.dts | head -5
# Should see compatible = "semtech,sx1262-new", NOT "semtech,sx1262"
```

**Q: Join fails (JOINFAIL)**

1. Verify keys match the LoRaWAN network server
2. Verify `user-lorawan-region` matches the actual region
3. Ensure a gateway for the corresponding frequency band is nearby and operational
4. Confirm the antenna is properly connected

**Q: No serial output**

```bash
ls /dev/ttyACM* /dev/ttyUSB*          # Confirm device exists (serial ports in WSL also need usbipd forwarding)
minicom -D /dev/ttyACM0 -b 115200     # If garbled output, check baud rate
```

---

## 8. Command Quick Reference

| Scenario | Command |
|----------|---------|
| Activate environment | `source ~/rak_zephyr/env.sh` |
| Build Hello World | `west build -b rak3162/nrf54l15/cpuapp samples/hello_world --pristine always` |
| Build MCUboot | `cd bootloader/mcuboot/boot/zephyr && west build -b rak3162/nrf54l15/cpuapp . --pristine always` |
| Flash (J-Link) | `west flash --runner jlink` |
| Build + Flash | `west build ... && west flash --runner jlink` |
| Build USP app | `west build -b rak3162/nrf54l15/cpuapp <app-path> --pristine always` |
| Serial monitor | `minicom -D /dev/ttyACM0 -b 115200` (or `/dev/ttyUSB0`) |
| Clean build artifacts | `rm -rf build` |
| Check J-Link connection | `lsusb \| grep -i segger` |
| USB forward to WSL | `usbipd attach --wsl --busid <BUSID>` (PowerShell) |
| Test J-Link chip connection | `echo -e "connect\nexit" \| JLinkExe -device nRF54L15_M33 -if SWD -speed 4000 -autoconnect 1 -NoGui 1 -CommandFile /dev/stdin` |
| Update USP modules | `cd ~/rak_zephyr/zephyrproject && west update usp_zephyr usp` |
| Check USP version | `git -C modules/lib/usp_zephyr describe --tags --always` |
| Verify toolchain | `cmake -P $ZEPHYR_BASE/cmake/verify-toolchain.cmake` |
| Reload udev | `sudo udevadm control --reload-rules && sudo udevadm trigger` |
