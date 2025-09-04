/* SPDX-License-Identifier: GPL-2.0-only */
// sgm4151x Charger Driver
// Copyright (C) 2021 Texas Instruments Incorporated - http://www.sg-micro.com

#ifndef _SGM4151x_CHARGER_H
#define _SGM4151x_CHARGER_H

#include <linux/i2c.h>

#define SGM4151x_MANUFACTURER	"Texas Instruments"

/*define register*/
#define SGM4151x_CHRG_CTRL_0	0x00
#define SGM4151x_CHRG_CTRL_1	0x01
#define SGM4151x_CHRG_CTRL_2	0x02
#define SGM4151x_CHRG_CTRL_3	0x03
#define SGM4151x_CHRG_CTRL_4	0x04
#define SGM4151x_CHRG_CTRL_5	0x05
#define SGM4151x_CHRG_CTRL_6	0x06
#define SGM4151x_CHRG_CTRL_7	0x07
#define SGM4151x_CHRG_STAT	    0x08
#define SGM4151x_CHRG_FAULT	    0x09
#define SGM4151x_CHRG_CTRL_A	0x0a
#define SGM4151x_CHRG_CTRL_B	0x0b
#define SGM4151x_CHRG_CTRL_F	0x0f


/* charge status flags  */
#define SGM4151x_CHRG_EN		BIT(4)
#define SGM4151x_HIZ_EN		    BIT(7)
#define SGM4151x_TERM_EN		BIT(7)
#define SGM4151x_VAC_OVP_MASK	GENMASK(7, 6)
#define SGM4151x_VBUS_GOOD      BIT(7)

/* WDT TIMER SET  */
#define SGM4151x_WDT_TIMER_MASK        GENMASK(5, 4)
#define SGM4151x_WDT_TIMER_DISABLE     0
#define SGM4151x_WDT_TIMER_40S         BIT(4)
#define SGM4151x_WDT_TIMER_80S         BIT(5)
#define SGM4151x_WDT_TIMER_160S        (BIT(4)| BIT(5))
#define SGM4151x_WDT_RST_MASK          BIT(6)

/* recharge voltage  */
#define SGM4151x_VRECHARGE              BIT(0)
#define SGM4151x_VRECHRG_STEP_mV		100
#define SGM4151x_VRECHRG_OFFSET_mV		100

/* charge status  */
#define SGM4151x_VSYS_STAT		BIT(0)
#define SGM4151x_THERM_STAT		BIT(1)
#define SGM4151x_PG_STAT		BIT(2)
#define SGM4151x_CHG_STAT_MASK	GENMASK(4, 3)
#define SGM4151x_PRECHRG		BIT(3)
#define SGM4151x_FAST_CHRG	    BIT(4)
#define SGM4151x_TERM_CHRG	    (BIT(3)| BIT(4))

/* charge type  */
#define SGM4151x_VBUS_STAT_MASK	GENMASK(7, 5)
#define SGM4151x_NOT_CHRGING	0

/* TEMP Status  */
#define SGM4151x_TEMP_MASK	    GENMASK(2, 0)
#define SGM4151x_TEMP_NORMAL	BIT(0)
#define SGM4151x_TEMP_WARM	    BIT(1)
#define SGM4151x_TEMP_COOL	    (BIT(0) | BIT(1))
#define SGM4151x_TEMP_COLD	    (BIT(0) | BIT(3))
#define SGM4151x_TEMP_HOT	    (BIT(2) | BIT(3))

/* precharge current  */
#define SGM4151x_PRECHRG_CUR_MASK		GENMASK(7, 4)

/* termination current  */
#define SGM4151x_TERMCHRG_CUR_MASK		GENMASK(3, 0)

/* charge current  */
#define SGM4151x_ICHRG_CUR_MASK		GENMASK(5, 0)
#define SGM4151x_ICHRG_I_DEF_uA			1980000

/* charge voltage  */
#define SGM4151x_VREG_V_MASK		GENMASK(7, 3)
#define SGM4151x_VREG_V_MAX_uV	    4624000
#define SGM4151x_VREG_V_MIN_uV	    3856000
#define SGM4151x_VREG_V_DEF_uV	    4400000

#define SGM4151x_VREG_V_STEP_uV	    32000

/* iindpm current  */
#define SGM4151x_IINDPM_I_MASK		GENMASK(4, 0)
#define SGM4151x_IINDPM_I_MIN_uA	100000
#define SGM4151x_IINDPM_I_MAX_uA	3200000
#define SGM4151x_IINDPM_STEP_uA	    100000
#define SGM4151x_IINDPM_DEF_uA	    1600000

#define SGM4151x_IINDPM_STAT        BIT(5)

/* vindpm voltage  */
#define SGM4151x_VINDPM_V_MIN_uV    3900000
#define SGM4151x_VINDPM_V_MAX_uV    5400000
#define SGM4151x_VINDPM_STEP_uV     100000
#define SGM4151x_VINDPM_DEF_uV	    4500000

#define SGM4151x_VINDPM_STAT        BIT(6)

struct sgm4151x_state {
	bool vsys_stat;
	bool therm_stat;
	bool online;	
	u8 chrg_stat;
	u8 vbus_status;

	bool chrg_en;
	bool hiz_en;
	bool term_en;
	bool vbus_gd;
	bool vindpm_stat;
	bool iindpm_stat;
	u8 chrg_type;
	u8 health;
	u8 chrg_fault;
	u8 ntc_fault;
};

struct sgm4151x_device {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *charger;	

	struct mutex lock;
	struct mutex i2c_rw_lock;
	struct regmap *regmap;
	int device_id;
	struct sgm4151x_state state;
	struct delayed_work charge_monitor_work;
	struct notifier_block pm_nb;
	u8 chg_en;
};

#endif /* _SGM4151x_CHARGER_H */
