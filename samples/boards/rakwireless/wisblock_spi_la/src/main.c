/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 RAKwireless Technology Limited
 *
 * WisBlock SPI logic-analyzer test.
 *
 * CS is driven entirely by the SPIM driver via cs-gpios (P2.05, active-low).
 * The application never touches the CS GPIO — matching the RAK BSP hw_test
 * pattern. The driver's cs_init (boot) configures the pin, and cs_set /
 * cs_clear (per-transfer) assert/deassert it.
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define BURST_COUNT      16U
#define BURST_GAP_MS     20U
#define CYCLE_IDLE_MS    2000U
#define SPI_FREQUENCY    2000000U

#define TEST_SPI_NODE  DT_NODELABEL(wisblock_spi)

#if !DT_NODE_EXISTS(DT_ALIAS(led0))
#error "RAK19007 shield must provide led0 alias"
#endif

#if !DT_NODE_HAS_STATUS(TEST_SPI_NODE, okay)
#error "Overlay must enable wisblock_spi"
#endif

#if !DT_NODE_HAS_PROP(TEST_SPI_NODE, cs_gpios)
#error "spi00 needs cs-gpios in DTS"
#endif

static const struct gpio_dt_spec sync_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct device *const spi_dev = DEVICE_DT_GET(TEST_SPI_NODE);

static const uint8_t pattern[] = {
	0xA5, 0xA5, 0x5A, 0x5A,
};

static uint8_t tx_data[sizeof(pattern)] __aligned(4);
static uint8_t rx_data[sizeof(pattern)] __aligned(4);

static void sync_pulse(void)
{
	printk("   [SYNC] LED1 triple pulse — SPI test starting\n");
	for (int i = 0; i < 3; i++) {
		(void)gpio_pin_set_dt(&sync_led, 1);
		k_msleep(100);
		(void)gpio_pin_set_dt(&sync_led, 0);
		k_msleep(100);
	}
	k_msleep(200);
}

static void run_spi(void)
{
	const struct spi_cs_control cs_ctrl = {
		.gpio = GPIO_DT_SPEC_GET(TEST_SPI_NODE, cs_gpios),
		.delay = 0U,
		.cs_is_gpio = true,
	};
	struct spi_config spi_cfg = {
		.frequency = SPI_FREQUENCY,
		.operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB |
			     SPI_WORD_SET(8) | SPI_LINES_SINGLE,
		.slave = 0U,
		.cs = cs_ctrl,
	};
	struct spi_buf tx_buf = { .buf = tx_data, .len = sizeof(tx_data) };
	struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1U };
	struct spi_buf rx_buf = { .buf = rx_data, .len = sizeof(rx_data) };
	struct spi_buf_set rx_set = { .buffers = &rx_buf, .count = 1U };
	int ret;

	memcpy(tx_data, pattern, sizeof(pattern));
	memset(rx_data, 0, sizeof(rx_data));

	printk("   [SPI] %u x %zu-byte bursts @ %u Hz, mode 0\n",
	       BURST_COUNT, sizeof(tx_data), SPI_FREQUENCY);
	printk("   [SPI] Probes: CS pin 25 (P2.05), CLK pin 26 (P2.01), MOSI pin 28 (P2.02)\n");

	for (unsigned int i = 0; i < BURST_COUNT; i++) {
		/*
		 * Use spi_transceive with a same-length dummy RX buffer.
		 * On nRF SPIM, TX-only (rx_length == 0) can produce
		 * incomplete clocking with EasyDMA; matching TX/RX lengths
		 * yields one SCK edge per bit and proper CS timing.
		 */
		ret = spi_transceive(spi_dev, &spi_cfg, &tx_set, &rx_set);
		if (ret < 0) {
			printk("   [SPI] burst %u error: %d\n", i + 1, ret);
		} else {
			printk("   [SPI] burst %u/%u\n", i + 1, BURST_COUNT);
		}
		k_msleep(BURST_GAP_MS);
	}
}

int main(void)
{
	unsigned int cycle = 0;
	int ret;

	printk("WisBlock SPI LA test\n");
	printk("Probe: CS/CLK/MOSI pins 25/26/28, sync = LED1 pin 14\n");

	if (!gpio_is_ready_dt(&sync_led)) {
		printk("LED1 not ready\n");
		return 0;
	}

	ret = gpio_pin_configure_dt(&sync_led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return 0;
	}

	if (!device_is_ready(spi_dev)) {
		printk("SPI device not ready\n");
		return 0;
	}

	while (1) {
		printk("\n=== SPI cycle %u ===\n", cycle++);
		sync_pulse();
		run_spi();
		printk("=== cycle idle %u ms ===\n", CYCLE_IDLE_MS);
		k_msleep(CYCLE_IDLE_MS);
	}

	return 0;
}
