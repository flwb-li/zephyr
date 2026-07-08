.. zephyr:code-sample:: wisblock-pwm-la
   :name: WisBlock PWM logic analyzer test
   :relevant-api: pwm_interface

   Generate PWM on WisBlock IO1 for logic analyzer capture.

Overview
********

Outputs a **200 Hz, 50 % duty** PWM waveform on **IO1** (WisBlock pin 29 / P2.00).
Before PWM starts, the sample toggles IO1 as GPIO so you can verify probe placement.
**LED1** emits one sync pulse at the start of each cycle.

Hardware
********

- RAK3362 + RAK19007
- LA: Ch0 = LED1 (pin 14), Ch1 = IO1 (WisBlock pin 29 → **MCU P1.14**)

.. note:: IO1 is **P1.14**, not P2.00. P2.00 is WisBlock IO2 (pin 30).

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/rakwireless/wisblock_pwm_la
   :board: rak3362/nrf54l15/cpuapp
   :shield: rakwireless_rak19007
   :goals: build flash
   :compact:
