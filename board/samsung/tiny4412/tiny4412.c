/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd. All rights reserved.
 * Sanghee Kim <sh0130.kim@samsung.com>
 * Piotr Wilczek <p.wilczek@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <lcd.h>
#include <asm/gpio.h>
#include <asm/arch/pinmux.h>
#include <asm/arch/power.h>
#include <asm/arch/mipi_dsim.h>
#include <power/pmic.h>
#include <power/max77686_pmic.h>
#include <power/battery.h>
#include <power/max77693_pmic.h>
#include <power/max77693_muic.h>
#include <power/max77693_fg.h>
#include <libtizen.h>
#include <errno.h>
#include <usb.h>
#include <usb/dwc2_udc.h>
#include <usb_mass_storage.h>

DECLARE_GLOBAL_DATA_PTR;

static unsigned int board_rev = -1;

static inline u32 get_model_rev(void);

static void check_hw_revision(void)
{
	int modelrev = 0;
	char str[12];
	int i;

	/*
	 * GPM1[1:0]: MODEL_REV[1:0]
	 * Don't set as pull-none for these N/C pin.
	 * TRM say that it may cause unexcepted state and leakage current.
	 * and pull-none is only for output function.
	 */
	for (i = 0; i < 2; i++) {
		int pin = i + EXYNOS4X12_GPIO_M10;

		sprintf(str, "model_rev%d", i);
		gpio_request(pin, str);
		gpio_cfg_pin(pin, S5P_GPIO_INPUT);
	}

	/* GPM1[5:2]: HW_REV[3:0] */
	for (i = 0; i < 4; i++) {
		int pin = i + EXYNOS4X12_GPIO_M12;

		sprintf(str, "hw_rev%d", i);
		gpio_request(pin, str);
		gpio_cfg_pin(pin, S5P_GPIO_INPUT);
		gpio_set_pull(pin, S5P_GPIO_PULL_NONE);
	}

	/* GPM1[1:0]: MODEL_REV[1:0] */
	for (i = 0; i < 2; i++)
		modelrev |= (gpio_get_value(EXYNOS4X12_GPIO_M10 + i) << i);

	/* board_rev[15:8] = model */
	board_rev = modelrev << 8;
}

u32 get_board_rev(void)
{
	volatile uint32_t *p = (volatile uint32_t *) 0x110002C4;
	return ((*p) >> 5)&0xff;
}

static inline u32 get_model_rev(void)
{
	return get_board_rev();
}

static void board_external_gpio_init(void)
{
	/*
	 * some pins which in alive block are connected with external pull-up
	 * but it's default setting is pull-down.
	 * if that pin set as input then that floated
	 */

	gpio_set_pull(EXYNOS4X12_GPIO_X02, S5P_GPIO_PULL_NONE);	/* PS_ALS_INT */
	gpio_set_pull(EXYNOS4X12_GPIO_X04, S5P_GPIO_PULL_NONE);	/* TSP_nINT */
	gpio_set_pull(EXYNOS4X12_GPIO_X07, S5P_GPIO_PULL_NONE);	/* AP_PMIC_IRQ*/
	gpio_set_pull(EXYNOS4X12_GPIO_X15, S5P_GPIO_PULL_NONE);	/* IF_PMIC_IRQ*/
	gpio_set_pull(EXYNOS4X12_GPIO_X20, S5P_GPIO_PULL_NONE);	/* VOL_UP */
	gpio_set_pull(EXYNOS4X12_GPIO_X21, S5P_GPIO_PULL_NONE);	/* VOL_DOWN */
	gpio_set_pull(EXYNOS4X12_GPIO_X23, S5P_GPIO_PULL_NONE);	/* FUEL_ALERT */
	gpio_set_pull(EXYNOS4X12_GPIO_X24, S5P_GPIO_PULL_NONE);	/* ADC_INT */
	gpio_set_pull(EXYNOS4X12_GPIO_X27, S5P_GPIO_PULL_NONE);	/* nPOWER */
	gpio_set_pull(EXYNOS4X12_GPIO_X30, S5P_GPIO_PULL_NONE);	/* WPC_INT */
	gpio_set_pull(EXYNOS4X12_GPIO_X35, S5P_GPIO_PULL_NONE);	/* OK_KEY */
	gpio_set_pull(EXYNOS4X12_GPIO_X37, S5P_GPIO_PULL_NONE);	/* HDMI_HPD */
}

int exynos_early_init_f(void)
{
	//board_external_gpio_init();

	return 0;
}

int exynos_init(void)
{
	struct exynos4_power *pwr =
		(struct exynos4_power *)samsung_get_base_power();

	check_hw_revision();
	printf("HW Revision:\t0x%04x\n", board_rev);

	/*
	 * First bootloader on the TRATS2 platform uses
	 * INFORM4 and INFORM5 registers for recovery
	 *
	 * To indicate correct boot chain - those two
	 * registers must be cleared out
	 */
	writel(0, &pwr->inform4);
	writel(0, &pwr->inform5);

	return 0;
}

int exynos_power_init(void)
{
	return 0;
}

#ifdef CONFIG_USB_GADGET
#define S5P_OTG_PHYPWR	 		(EXYNOS4X12_USBPHY_BASE + 0x000) /* R/W OTG PHY Power Control Register */
#define S5P_OTG_PHYCLK 			(EXYNOS4X12_USBPHY_BASE + 0x004) /* R/W OTG PHY Clock Control Register */
#define S5P_OTG_GOTGCTL 		(EXYNOS4X12_USBOTG_BASE + 0x000) /* R/W OTG Control and Status Register */
#define B_SESSION_VALID			(1<<19)
#define A_SESSION_VALID			(1<<18)

static int s5pc210_phy_control(int on)
{
	return 0;
}

struct dwc2_plat_otg_data s5pc210_otg_data = {
	.phy_control	= s5pc210_phy_control,
	.regs_phy	= EXYNOS4X12_USBPHY_BASE,
	.regs_otg	= EXYNOS4X12_USBOTG_BASE,
	.usb_phy_ctrl	= EXYNOS4X12_USBPHY_CONTROL,
	.usb_flags	= PHY0_SLEEP,
};

int board_usb_init(int index, enum usb_init_type init)
{
	debug("USB_udc_probe\n");
	return dwc2_udc_probe(&s5pc210_otg_data);
}

int g_dnl_board_usb_cable_connected(void)
{
	u32 tmp;
	tmp = readl(S5P_OTG_GOTGCTL);
	return tmp & (B_SESSION_VALID|A_SESSION_VALID);
}


#endif

/*
 * LCD
 */

#ifdef CONFIG_LCD
int mipi_power(void)
{
#ifndef CONFIG_DM_I2C /* TODO(maintainer): Convert to driver model */
	struct pmic *p = pmic_get("MAX77686_PMIC");

	/* LDO8 VMIPI_1.0V_AP */
	max77686_set_ldo_mode(p, 8, OPMODE_ON);
	/* LDO10 VMIPI_1.8V_AP */
	max77686_set_ldo_mode(p, 10, OPMODE_ON);
#endif

	return 0;
}

void exynos_lcd_power_on(void)
{
#ifndef CONFIG_DM_I2C /* TODO(maintainer): Convert to driver model */
	struct pmic *p = pmic_get("MAX77686_PMIC");

	/* LCD_2.2V_EN: GPC0[1] */
	gpio_request(EXYNOS4X12_GPIO_C01, "lcd_2v2_en");
	gpio_set_pull(EXYNOS4X12_GPIO_C01, S5P_GPIO_PULL_UP);
	gpio_direction_output(EXYNOS4X12_GPIO_C01, 1);

	/* LDO25 VCC_3.1V_LCD */
	pmic_probe(p);
	max77686_set_ldo_voltage(p, 25, 3100000);
	max77686_set_ldo_mode(p, 25, OPMODE_LPM);
#endif
}

void exynos_reset_lcd(void)
{
	/* reset lcd */
	gpio_request(EXYNOS4X12_GPIO_F21, "lcd_reset");
	gpio_direction_output(EXYNOS4X12_GPIO_F21, 0);
	udelay(10);
	gpio_set_value(EXYNOS4X12_GPIO_F21, 1);
}

void exynos_lcd_misc_init(vidinfo_t *vid)
{
#ifdef CONFIG_TIZEN
	get_tizen_logo_info(vid);
#endif
#ifdef CONFIG_S6E8AX0
	s6e8ax0_init();
#endif
}
#endif /* LCD */
