/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 RAKwireless Technology Limited
 *
 * WisBlock ADC logic-analyzer test.
 * LED2 toggles on each conversion for LA timing; voltage on AIN pins is analog.
 */

#include <inttypes.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define SAMPLE_COUNT     8U
#define SAMPLE_GAP_MS    50U
#define CYCLE_IDLE_MS    2000U

#if !DT_NODE_EXISTS(DT_ALIAS(led0)) || !DT_NODE_EXISTS(DT_ALIAS(led1))
#error "RAK19007 shield must provide led0 and led1 aliases"
#endif

#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
	!DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "Overlay must define zephyr,user io-channels"
#endif

#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
	ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

static const struct gpio_dt_spec sync_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec strobe_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels, DT_SPEC_AND_COMMA)
};

static void sync_pulse(void)
{
	printk("   [SYNC] LED1 quad pulse — ADC test starting\n");
	for (int i = 0; i < 4; i++) {
		(void)gpio_pin_set_dt(&sync_led, 1);
		k_msleep(100);
		(void)gpio_pin_set_dt(&sync_led, 0);
		k_msleep(100);
	}
	k_msleep(200);
}

static int adc_setup_all(void)
{
	int ret;

	for (size_t i = 0; i < ARRAY_SIZE(adc_channels); i++) {
		if (!adc_is_ready_dt(&adc_channels[i])) {
			printk("   [ADC] channel %zu not ready\n", i);
			return -ENODEV;
		}

		ret = adc_channel_setup_dt(&adc_channels[i]);
		if (ret < 0) {
			printk("   [ADC] channel %zu setup failed: %d\n", i, ret);
			return ret;
		}
	}

	return 0;
}

static void run_adc(void)
{
	uint32_t sample_buf;
	struct adc_sequence sequence = {
		.buffer = &sample_buf,
		.buffer_size = sizeof(sample_buf),
	};
	int ret;

	printk("   [ADC] %u sample sets x %zu channel(s)\n",
	       SAMPLE_COUNT, ARRAY_SIZE(adc_channels));
	printk("   [ADC] Probes: AIN0 pin 21, AIN1 pin 22 (analog)\n");
	printk("   [ADC] LA on LED2 pin 15 — toggles each conversion\n");

	for (unsigned int set = 0; set < SAMPLE_COUNT; set++) {
		printk("   [ADC] sample set %u/%u\n", set + 1, SAMPLE_COUNT);

		for (size_t i = 0; i < ARRAY_SIZE(adc_channels); i++) {
			int32_t val_mv;

			sample_buf = 0;
			(void)adc_sequence_init_dt(&adc_channels[i], &sequence);

			(void)gpio_pin_set_dt(&strobe_led, 1);
			ret = adc_read_dt(&adc_channels[i], &sequence);
			(void)gpio_pin_set_dt(&strobe_led, 0);

			if (ret < 0) {
				printk("      ch %zu: read error %d\n", i, ret);
				continue;
			}

			val_mv = (int32_t)sample_buf;
			ret = adc_raw_to_millivolts_dt(&adc_channels[i], &val_mv);
			if (ret < 0) {
				printk("      ch %zu: raw=%" PRId32 "\n", i, (int32_t)sample_buf);
			} else {
				printk("      ch %zu: %" PRId32 " mV\n", i, val_mv);
			}
		}

		k_msleep(SAMPLE_GAP_MS);
	}
}

int main(void)
{
	unsigned int cycle = 0;
	int ret;

	printk("WisBlock ADC LA test\n");
	printk("Probe: LED2 pin 15 (strobe), AIN0/AIN1 pins 21/22 (DMM)\n");

	if (!gpio_is_ready_dt(&sync_led) || !gpio_is_ready_dt(&strobe_led)) {
		printk("LED GPIO not ready\n");
		return 0;
	}

	ret = gpio_pin_configure_dt(&sync_led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&strobe_led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return 0;
	}

	while (1) {
		printk("\n=== ADC cycle %u ===\n", cycle++);

		ret = adc_setup_all();
		if (ret < 0) {
			k_msleep(CYCLE_IDLE_MS);
			continue;
		}

		sync_pulse();
		run_adc();
		printk("=== cycle idle %u ms ===\n", CYCLE_IDLE_MS);
		k_msleep(CYCLE_IDLE_MS);
	}

	return 0;
}
