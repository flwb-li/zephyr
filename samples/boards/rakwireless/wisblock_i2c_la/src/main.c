/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 RAKwireless Technology Limited
 *
 * WisBlock I2C1 logic-analyzer test.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define BURST_COUNT      16U
#define BURST_GAP_MS     20U
#define CYCLE_IDLE_MS    2000U
#define I2C_PROBE_ADDR   0x55U

#if !DT_NODE_EXISTS(DT_ALIAS(led0))
#error "RAK19007 shield must provide led0 alias"
#endif

#if !DT_NODE_HAS_STATUS(DT_NODELABEL(wisblock_i2c1), okay)
#error "Overlay must enable wisblock_i2c1"
#endif

static const struct gpio_dt_spec sync_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct device *const i2c_dev = DEVICE_DT_GET(DT_NODELABEL(wisblock_i2c1));

static const uint8_t pattern[] = { 0x55, 0xAA, 0x12, 0x34, 0x78, 0x9A, 0xBC, 0xDE };

static void sync_pulse(void)
{
	printk("   [SYNC] LED1 double pulse — I2C test starting\n");
	for (int i = 0; i < 2; i++) {
		(void)gpio_pin_set_dt(&sync_led, 1);
		k_msleep(100);
		(void)gpio_pin_set_dt(&sync_led, 0);
		k_msleep(100);
	}
	k_msleep(200);
}

static void run_i2c(void)
{
	int ret;

	printk("   [I2C] %u writes to 0x%02x @ 400 kHz\n", BURST_COUNT, I2C_PROBE_ADDR);
	printk("   [I2C] Probes: SDA pin 19 (P0.02), SCL pin 20 (P0.03)\n");
	printk("   [I2C] NACK without slave is expected; capture START/ADDR/data\n");

	for (unsigned int i = 0; i < BURST_COUNT; i++) {
		ret = i2c_write(i2c_dev, pattern, sizeof(pattern), I2C_PROBE_ADDR);
		if (ret < 0 && ret != -EIO) {
			printk("   [I2C] burst %u error: %d\n", i + 1, ret);
		} else {
			printk("   [I2C] burst %u/%u (%zu bytes)\n",
			       i + 1, BURST_COUNT, sizeof(pattern));
		}
		k_msleep(BURST_GAP_MS);
	}
}

int main(void)
{
	unsigned int cycle = 0;
	int ret;

	printk("WisBlock I2C LA test (I2C1)\n");
	printk("Probe: SDA pin 19, SCL pin 20, sync = LED1 pin 14\n");

	if (!gpio_is_ready_dt(&sync_led)) {
		printk("LED1 not ready\n");
		return 0;
	}

	ret = gpio_pin_configure_dt(&sync_led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return 0;
	}

	if (!device_is_ready(i2c_dev)) {
		printk("I2C device not ready\n");
		return 0;
	}

	while (1) {
		printk("\n=== I2C cycle %u ===\n", cycle++);
		sync_pulse();
		run_i2c();
		printk("=== cycle idle %u ms ===\n", CYCLE_IDLE_MS);
		k_msleep(CYCLE_IDLE_MS);
	}

	return 0;
}
