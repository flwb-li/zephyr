.. zephyr:code-sample:: wisblock-i2c-la
   :name: WisBlock I2C logic analyzer test
   :relevant-api: i2c_interface

   Generate I2C1 traffic for logic analyzer capture.

Overview
********

Sends **16 write bursts** to address **0x55** on **I2C1** (400 kHz).
NACK without a slave is normal; the START/address/data waveform is still visible.
**LED1** emits two sync pulses at the start of each cycle.

Hardware
********

- RAK3362 + RAK19007
- LA: Ch0 = LED1 (pin 14), Ch1 = SDA (pin 19), Ch2 = SCL (pin 20)

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/rakwireless/wisblock_i2c_la
   :board: rak3362/nrf54l15/cpuapp
   :shield: rakwireless_rak19007
   :goals: build flash
   :compact:
