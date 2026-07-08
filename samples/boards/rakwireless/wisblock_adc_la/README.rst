.. zephyr:code-sample:: wisblock-adc-la
   :name: WisBlock ADC logic analyzer test
   :relevant-api: adc_interface

   Sample WisBlock ADC channels with LED strobe for logic analyzer timing.

Overview
********

Reads **AIN0** and **AIN1** eight times per cycle. **LED2** toggles on each
conversion so a logic analyzer can see sample timing. Use a multimeter on the
AIN pins for voltage. **LED1** emits four sync pulses at cycle start.

Hardware
********

- RAK3362 + RAK19007
- LA: Ch0 = LED1 (pin 14), Ch1 = LED2 (pin 15)
- DMM: AIN0 pin 21, AIN1 pin 22

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/rakwireless/wisblock_adc_la
   :board: rak3362/nrf54l15/cpuapp
   :shield: rakwireless_rak19007
   :goals: build flash
   :compact:
