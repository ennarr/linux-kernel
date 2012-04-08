/*
 * arch/arm/mach-kirkwood/rd88f6281-setup.c
 *
 * Marvell RD-88F6281 Reference Board Setup
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/mtd/partitions.h>
#include <linux/ata_platform.h>
#include <linux/mv643xx_eth.h>
#include <linux/ethtool.h>
#include <net/dsa.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/gpio_keys.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/kirkwood.h>
#include "common.h"
#include "mpp.h"

static struct mtd_partition rd88f6281_nand_parts[] = {
	{
		.name = "uImage",
		.offset = 0x100000,
		.size = 0x300000
	}, {
		.name = "uInitrd",
		.offset = MTDPART_OFS_NXTBLK,
		.size = 0x300000,
	},
};

static struct mv643xx_eth_platform_data rd88f6281_ge00_data = {
	.phy_addr       = MV643XX_ETH_PHY_NONE,
	.speed          = SPEED_1000,
	.duplex         = DUPLEX_FULL,
};

static struct mv643xx_eth_platform_data rd88f6281_ge01_data = {
	.phy_addr	= MV643XX_ETH_PHY_ADDR(11),
};

static struct mv_sata_platform_data rd88f6281_sata_data = {
	.n_ports	= 2,
};

/*****************************************************************************
 * LEDs attached to GPIO
 ****************************************************************************/

static struct gpio_led rd88f6281_led_pins[] = {
	{
		.name			= "power_led",
		.gpio			= 16,
		.default_trigger	= "default-on",
	},
	{
		.name			= "rebuild_led",
		.gpio			= 36,
		.default_trigger	= "none",
	},
	{
		.name			= "health_led",
		.gpio			= 37,
		.default_trigger	= "none",
	},
	{
		.name			= "backup_led",
		.gpio			= 15,
		.default_trigger	= "none",
	},

};

#define ORION_BLINK_HALF_PERIOD 100 /* ms */

int rd88f6281_gpio_blink_set(unsigned gpio, int state,
	unsigned long *delay_on, unsigned long *delay_off)
{
	orion_gpio_set_blink(gpio, state);
	return 0;
}


static struct gpio_led_platform_data rd88f6281_led_data = {
	.leds		= rd88f6281_led_pins,
	.num_leds	= ARRAY_SIZE(rd88f6281_led_pins),
	.gpio_blink_set = (void*)rd88f6281_gpio_blink_set,
};

static struct platform_device rd88f6281_leds = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &rd88f6281_led_data,
	}
};

/****************************************************************************
 * GPIO Attached Keys
 ****************************************************************************/

#define RD88F6281_GPIO_KEY_RESET	12
#define RD88F6281_GPIO_KEY_POWER	14
#define RD88F6281_GPIO_KEY_OTB		35

#define RD88F6281_SW_RESET	0x00
#define RD88F6281_SW_POWER	0x01
#define RD88F6281_SW_OTB	0x02

static struct gpio_keys_button rd88f6281_buttons[] = {
	{
		.type		= EV_SW,
		.code	   = RD88F6281_SW_RESET,
		.gpio	   = RD88F6281_GPIO_KEY_RESET,
		.desc	   = "Reset Button",
		.active_low     = 1,
		.debounce_interval = 100,
	}, 
	{
		.type		= EV_SW,
		.code	   = RD88F6281_SW_POWER,
		.gpio	   = RD88F6281_GPIO_KEY_POWER,
		.desc	   = "Power Button",
		.active_low     = 1,
		.debounce_interval = 100,
	},
	{
		.type		= EV_SW,
		.code	   = RD88F6281_SW_OTB,
		.gpio	   = RD88F6281_GPIO_KEY_OTB,
		.desc	   = "OTB Button",
		.active_low     = 1,
		.debounce_interval = 100,
	},

};

static struct gpio_keys_platform_data rd88f6281_button_data = {
	.buttons	= rd88f6281_buttons,
	.nbuttons       = ARRAY_SIZE(rd88f6281_buttons),
};

static struct platform_device rd88f6281_button_device = {
	.name	   = "gpio-keys",
	.id	     = -1,
	.num_resources  = 0,
	.dev	    = {
		.platform_data  = &rd88f6281_button_data,
	},
};
static unsigned int rd88f6281_mpp_config[] __initdata = {
	MPP12_GPIO,                             /* Reset Button */
	MPP14_GPIO,                             /* Power Button */
	MPP15_GPIO,                             /* Backup LED (blue) */
	MPP16_GPIO,                             /* Power LED (white) */
	MPP35_GPIO,                             /* OTB Button */
	MPP36_GPIO,                             /* Rebuild LED (white) */
	MPP37_GPIO,                             /* Health LED (red) */
	MPP38_GPIO,                             /* SATA LED brightness control 1 */
	MPP39_GPIO,                             /* SATA LED brightness control 2 */
	MPP40_GPIO,                             /* Backup LED brightness control 1 */
	MPP41_GPIO,                             /* Backup LED brightness control 2 */
	MPP42_GPIO,                             /* Power LED brightness control 1 */
	MPP43_GPIO,                             /* Power LED brightness control 2 */
	MPP44_GPIO,                             /* Health LED brightness control 1 */
	MPP45_GPIO,                             /* Health LED brightness control 2 */
	MPP46_GPIO,                             /* Rebuild LED brightness control 1 */
	MPP47_GPIO,                             /* Rebuild LED brightness control 2 */
	0
};

static struct i2c_board_info __initdata rd88f6281_i2c = {
	I2C_BOARD_INFO("lm63", 0x4c),
};

static void __init rd88f6281_init(void)
{
	/*
	 * Basic setup. Needs to be called early.
	 */
	kirkwood_init();
	kirkwood_mpp_conf(rd88f6281_mpp_config);

	kirkwood_nand_init(ARRAY_AND_SIZE(rd88f6281_nand_parts), 25);
	kirkwood_ehci_init();

	kirkwood_ge00_init(&rd88f6281_ge00_data);
	kirkwood_ge01_init(&rd88f6281_ge01_data);
	kirkwood_sata_init(&rd88f6281_sata_data);
	platform_device_register(&rd88f6281_leds);
	platform_device_register(&rd88f6281_button_device);
	kirkwood_uart0_init();
	kirkwood_i2c_init();
	i2c_register_board_info(0, &rd88f6281_i2c, 1);
}

MACHINE_START(RD88F6281, "Marvell RD-88F6281 Reference Board")
	/* Maintainer: Saeed Bishara <saeed@marvell.com> */
	.boot_params	= 0x00000100,
	.init_machine	= rd88f6281_init,
	.map_io		= kirkwood_map_io,
	.init_early	= kirkwood_init_early,
	.init_irq	= kirkwood_init_irq,
	.timer		= &kirkwood_timer,
MACHINE_END
