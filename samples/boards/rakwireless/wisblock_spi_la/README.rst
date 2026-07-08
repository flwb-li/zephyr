.. zephyr:code-sample:: wisblock-spi-la
   :name: WisBlock SPI logic analyzer test
   :relevant-api: spi_interface

   Generate SPI master traffic for logic analyzer capture.

Overview
********

Sends **16 bursts** of a fixed 16-byte pattern at **2 MHz**, SPI mode 0.
**CS is bit-banged** on WisBlock pin 25 (MCU P2.05); a triple CS pulse runs once at boot for probe check.
**LED1** emits three sync pulses at the start of each cycle.

Hardware
********

- RAK3362 + RAK19007
- LA: Ch0 = LED1, Ch1 = CS (pin 25 / P2.05), Ch2 = CLK (pin 26 / P2.01), Ch3 = MOSI (pin 28 / P2.02)
- SPI runs at **2 MHz** (nRF54L15 SPIM prescaler-friendly rate)

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/rakwireless/wisblock_spi_la
   :board: rak3362/nrf54l15/cpuapp
   :shield: rakwireless_rak19007
   :goals: build flash
   :compact:
