/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 RAKwireless Technology Limited
 *
 * WisBlock PWM logic-analyzer test on IO1.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#if defined(CONFIG_BOARD_RAK3362_NRF54L15_CPUAPP) || \
	defined(CONFIG_BOARD_RAK3362_NRF54L15_CPUAPP_NS)
#include <board.h>
#endif

#define PWM_RUN_MS       5000U
#define GPIO_TOGGLE_CNT  5U
#define CYCLE_IDLE_MS    2000U

#define PWM_PERIOD_NS    PWM_USEC(5000)
#define PWM_PULSE_NS     (PWM_PERIOD_NS / 2U)

#if !DT_NODE_EXISTS(DT_ALIAS(led0)) || !DT_NODE_EXISTS(DT_ALIAS(pwm_led0)) || \
	!DT_NODE_EXISTS(DT_ALIAS(test_io1))
#error "Overlay must define led0, pwm-led0, and test-io1 aliases"
#endif

static const struct gpio_dt_spec sync_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec io1_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(test_io1), gpios);
static const struct pwm_dt_spec pwm_io1 = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led0));

static void sync_pulse(void)
{
	printk("   [SYNC] LED1 pulse — PWM test starting\n");
	(void)gpio_pin_set_dt(&sync_led, 1);
	k_msleep(100);
	(void)gpio_pin_set_dt(&sync_led, 0);
	k_msleep(200);
}

static int gpio_io1_self_test(void)
{
	int ret;

	if (!gpio_is_ready_dt(&io1_gpio)) {
		printk("   [GPIO] IO1 not ready\n");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&io1_gpio, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return ret;
	}

	printk("   [GPIO] Toggling IO1 %u times (WisBlock pin 29 / MCU P1.14)\n", GPIO_TOGGLE_CNT);
	printk("   [GPIO] If LA sees no edges here, check probe placement\n");

	for (unsigned int i = 0; i < GPIO_TOGGLE_CNT; i++) {
		(void)gpio_pin_set_dt(&io1_gpio, 1);
		k_msleep(100);
		(void)gpio_pin_set_dt(&io1_gpio, 0);
		k_msleep(100);
		printk("   [GPIO] Toggle %u/%u\n", i + 1, GPIO_TOGGLE_CNT);
	}

	return 0;
}

static void run_pwm(void)
{
	int ret;
	int64_t end;

#if defined(CONFIG_BOARD_RAK3362_NRF54L15_CPUAPP) || \
	defined(CONFIG_BOARD_RAK3362_NRF54L15_CPUAPP_NS)
	ret = rak3362_wisblock_pwm_refresh();
	if (ret < 0) {
		printk("   [PWM] pinmux refresh failed: %d\n", ret);
		return;
	}
#endif

	printk("   [PWM] dev=%s ch=%u period=%lu ns pulse=%lu ns\n",
	       pwm_io1.dev->name, pwm_io1.channel,
	       (unsigned long)PWM_PERIOD_NS, (unsigned long)PWM_PULSE_NS);
	printk("   [PWM] Expect 200 Hz square wave on IO1 (P1.14) for %u ms\n", PWM_RUN_MS);

	ret = pwm_set_dt(&pwm_io1, PWM_PERIOD_NS, PWM_PULSE_NS);
	if (ret < 0) {
		printk("   [PWM] pwm_set_dt failed: %d\n", ret);
		return;
	}

	end = k_uptime_get() + PWM_RUN_MS;
	while (k_uptime_get() < end) {
		k_msleep(500);
	}

	(void)pwm_set_pulse_dt(&pwm_io1, 0);
	printk("   [PWM] Stopped\n");
}

int main(void)
{
	unsigned int cycle = 0;
	int ret;

	printk("WisBlock PWM LA test (IO1)\n");
	printk("Probe: IO1 = WisBlock pin 29 -> MCU P1.14 (not P2.00)\n");
	printk("       IO2 = WisBlock pin 30 -> MCU P2.00\n");

	if (!gpio_is_ready_dt(&sync_led)) {
		printk("LED1 not ready\n");
		return 0;
	}

	ret = gpio_pin_configure_dt(&sync_led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		printk("LED1 config failed: %d\n", ret);
		return 0;
	}

	if (!pwm_is_ready_dt(&pwm_io1)) {
		printk("PWM not ready (dev %s)\n", pwm_io1.dev->name);
		return 0;
	}

	while (1) {
		printk("\n=== PWM cycle %u ===\n", cycle++);
		sync_pulse();
		(void)gpio_io1_self_test();
		k_msleep(300);
		run_pwm();
		printk("=== cycle idle %u ms ===\n", CYCLE_IDLE_MS);
		k_msleep(CYCLE_IDLE_MS);
	}

	return 0;
}
