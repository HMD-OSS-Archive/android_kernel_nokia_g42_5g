/* SPDX-License-Identifier: GPL-2.0-only */
// eta6963 Charger Driver
// Copyright (C) 2021 Texas Instruments Incorporated - http://www.sg-micro.com

#ifndef _ETA6963_CHARGER_H
#define _ETA6963_CHARGER_H

#include <linux/i2c.h>

#define ETA6963_MANUFACTURER	"Texas Instruments"


/*define register*/
#define ETA6963_CHRG_CTRL_0	0x00
#define ETA6963_CHRG_CTRL_1	0x01
#define ETA6963_CHRG_CTRL_2	0x02
#define ETA6963_CHRG_CTRL_3	0x03
#define ETA6963_CHRG_CTRL_4	0x04
#define ETA6963_CHRG_CTRL_5	0x05
#define ETA6963_CHRG_CTRL_6	0x06
#define ETA6963_CHRG_CTRL_7	0x07
#define ETA6963_CHRG_STAT	    0x08
#define ETA6963_CHRG_FAULT	    0x09
#define ETA6963_CHRG_CTRL_A	0x0a
#define ETA6963_CHRG_CTRL_B	0x0b

#define ETA6963_VREG_FT_MASK	GENMASK(7, 6)


/* charge status flags  */
#define ETA6963_CHRG_EN		BIT(4)
#define ETA6963_HIZ_EN		    BIT(7)
#define ETA6963_TERM_EN		BIT(7)
#define ETA6963_VAC_OVP_MASK	GENMASK(7, 6)
#define ETA6963_VBUS_GOOD      BIT(7)


/* WDT TIMER SET  */
#define ETA6963_WDT_TIMER_MASK        GENMASK(5, 4)
#define ETA6963_WDT_TIMER_DISABLE     0
#define ETA6963_WDT_TIMER_40S         BIT(4)
#define ETA6963_WDT_TIMER_80S         BIT(5)
#define ETA6963_WDT_TIMER_160S        (BIT(4)| BIT(5))

#define ETA6963_WDT_RST_MASK          BIT(6)


/* recharge voltage  */
#define ETA6963_VRECHARGE              BIT(0)
#define ETA6963_VRECHRG_STEP_mV		100
#define ETA6963_VRECHRG_OFFSET_mV		100

/* charge status  */
#define ETA6963_VSYS_STAT		BIT(0)
#define ETA6963_THERM_STAT		BIT(1)
#define ETA6963_PG_STAT		BIT(2)
#define ETA6963_CHG_STAT_MASK	GENMASK(4, 3)
#define ETA6963_PRECHRG		BIT(3)
#define ETA6963_FAST_CHRG	    BIT(4)
#define ETA6963_TERM_CHRG	    (BIT(3)| BIT(4))

/* charge type  */
#define ETA6963_VBUS_STAT_MASK	GENMASK(7, 5)
#define ETA6963_NOT_CHRGING	0

/* TEMP Status  */
#define ETA6963_TEMP_MASK	    GENMASK(2, 0)
#define ETA6963_TEMP_NORMAL	BIT(0)
#define ETA6963_TEMP_WARM	    BIT(1)
#define ETA6963_TEMP_COOL	    (BIT(0) | BIT(1))
#define ETA6963_TEMP_COLD	    (BIT(0) | BIT(3))
#define ETA6963_TEMP_HOT	    (BIT(2) | BIT(3))

/* precharge current  */
#define ETA6963_PRECHRG_CUR_MASK		GENMASK(7, 4)


/* termination current  */
#define ETA6963_TERMCHRG_CUR_MASK		GENMASK(3, 0)


/* charge current  */
#define ETA6963_ICHRG_CUR_MASK		GENMASK(5, 0)
#define ETA6963_ICHRG_I_DEF_uA			1920000

/* charge voltage  */
#define ETA6963_VREG_V_MASK		GENMASK(7, 3)

#define ETA6963_VREG_V_MAX_uV	    4616000
#define ETA6963_VREG_V_MIN_uV	    3848000
#define ETA6963_VREG_V_DEF_uV	    4400000

/* iindpm current  */
#define ETA6963_IINDPM_I_MASK		GENMASK(4, 0)
#define ETA6963_IINDPM_I_MIN_uA	100000
#define ETA6963_IINDPM_I_MAX_uA	3200000
#define ETA6963_IINDPM_STEP_uA	    100000
#define ETA6963_IINDPM_DEF_uA	    1600000

#define ETA6963_IINDPM_STAT        BIT(5)

/* vindpm voltage  */
#define ETA6963_VINDPM_V_MIN_uV    3900000
#define ETA6963_VINDPM_V_MAX_uV    5400000
#define ETA6963_VINDPM_STEP_uV     100000
#define ETA6963_VINDPM_DEF_uV	    4500000

#define ETA6963_VINDPM_STAT        BIT(6)

struct eta6963_state {
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

struct eta6963_device {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *charger;	
	struct mutex lock;
	struct mutex i2c_rw_lock;
	struct regmap *regmap;
	int device_id;
	struct eta6963_state state;
	struct delayed_work charge_monitor_work;
	struct notifier_block pm_nb;
	u8 chg_en;
};

#endif /* _ETA6963_CHARGER_H */
