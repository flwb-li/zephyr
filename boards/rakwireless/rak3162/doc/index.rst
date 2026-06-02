.. zephyr:board:: rak3162

Overview
********

RAK3162 is a WisDuo LPWAN module based on the Nordic nRF54L15 SoC (Arm Cortex-M33),
integrating a Semtech SX1262 LoRa transceiver. It is part of the RAK WisBlock
ecosystem and can be used with a WisBlock Base board for rapid prototyping.
It provides Bluetooth Low Energy (BLE 5.4), LoRa, and multiple peripheral
interfaces (UART, I2C, SPI, ADC) in a compact form factor for IoT applications.

Hardware
********

The easiest way to use a RAK3162 is the WisBlock Modular system.
A WisBlock Base board (such as RAK19007) provides the power supply
and programming/debug interface for the RAK3162 module.

- Nordic nRF54L15 Arm Cortex-M33 processor
- 1.4 MB flash, 188 KB SRAM
- Semtech SX1262 low power LoRa transceiver with TCXO
- 2 user LEDs (LED0 = P2.09, LED1 = P2.10)
- UART x2, I2C, SPI x2, ADC, NFC
- Debug console (UART0): TX = P1.06, RX = P1.07, 115200 8N1
- SWD debug interface

For more information about the RAK3162 stamp module:

- `RAK3162 Product Page`_

Supported Features
==================

.. zephyr:board-supported-hw::

Connections and IOs
===================

LEDs
----

* LED0 = P2.09
* LED1 = P2.10

Serial Ports
------------

* UART0 (debug console): TX = P1.06, RX = P1.07
* UART1: TX = P2.08, RX = P2.07

I2C
---

* SDA = P0.02, SCL = P0.03

SPI
---

* LoRa SPI (spi22): SCK = P1.11, MOSI = P1.10, MISO = P1.09, CS = P1.12
* External SPI (spi00): SCK = P2.01, MOSI = P2.02, MISO = P2.04, CS = P2.05

LoRa SX1262
-----------

* RESET = P0.04
* BUSY = P1.13
* DIO1 = P0.01
* ANT_SW = P0.00 (RTC66006 RF switch control)

Programming and Debugging
*************************

.. zephyr:board-supported-runners::

The RAK3162 module can be programmed and debugged via SWD using an external
debug probe (J-Link, pyOCD, or OpenOCD).

Flashing an application
=======================

RAK3162 uses MCUboot as the bootloader. The flash layout reserves the first
64 KB for MCUboot. If the module is new (factory state), you must flash
MCUboot before flashing any application firmware.

Connect the module to your host computer via a debug probe and build and flash
an application. The sample application :zephyr:code-sample:`hello_world` is used
for this example:

.. zephyr-app-commands::
   :zephyr-app: samples/hello_world
   :board: rak3162/nrf54l15/cpuapp
   :goals: build flash

Run a serial terminal to connect to the debug UART (UART0): 115200, 8N1.

.. code-block:: console

   Hello World! rak3162/nrf54l15/cpuapp

Debugging
=========

You can debug an application using the supported runners.
Here is an example for the :zephyr:code-sample:`hello_world` application:

.. zephyr-app-commands::
   :zephyr-app: samples/hello_world
   :board: rak3162/nrf54l15/cpuapp
   :goals: debug

References
**********

.. target-notes::

.. _RAK3162 Product Page:
   https://docs.rakwireless.com/Product-Categories/WisDuo/RAK3162/Overview/

.. _WisBlock RAK19007 Website:
   https://docs.rakwireless.com/Product-Categories/WisBlock/RAK19007/Overview/
