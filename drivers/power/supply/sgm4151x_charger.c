// SPDX-License-Identifier: GPL-2.0
// SGM4151x driver
// Copyright (C) 2021 Texas Instruments Incorporated - http://www.sg-micro.com

#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>

#include <linux/acpi.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "sgm4151x_charger.h"
static struct power_supply_desc sgm4151x_power_supply_desc;

#include <ontim_dev_dgb.h>
static  char charge_ic_vendor_name[50]="SGM41513";
DEV_ATTR_DECLARE(charge_ic)
DEV_ATTR_DEFINE("vendor",charge_ic_vendor_name)
DEV_ATTR_DECLARE_END;
ONTIM_DEBUG_DECLARE_AND_INIT(charge_ic,charge_ic,8);


enum SGM4151x_OVP {
	SGM4151x_OVP_5500mV,
	SGM4151x_OVP_6500mV,
	SGM4151x_OVP_10500mV,
	SGM4151x_OVP_14000mV,		
};

static int __sgm4151x_read_byte(struct sgm4151x_device *sgm, u8 reg, u8 *data)
{
    s32 ret;

    ret = i2c_smbus_read_byte_data(sgm->client, reg);
    if (ret < 0) {
        pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
        return ret;
    }

    *data = (u8) ret;

    return 0;
}

static int __sgm4151x_write_byte(struct sgm4151x_device *sgm, int reg, u8 val)
{
    s32 ret;

    ret = i2c_smbus_write_byte_data(sgm->client, reg, val);
    if (ret < 0) {
        pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
               val, reg, ret);
        return ret;
    }
    return 0;
}

static int sgm4151x_read_reg(struct sgm4151x_device *sgm, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm4151x_read_byte(sgm, reg, data);
	mutex_unlock(&sgm->i2c_rw_lock);

	return ret;
}
static int sgm4151x_update_bits(struct sgm4151x_device *sgm, u8 reg,
					u8 mask, u8 val)
{
	int ret;
	u8 tmp;

	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm4151x_read_byte(sgm, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= val & mask;

	ret = __sgm4151x_write_byte(sgm, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&sgm->i2c_rw_lock);
	return ret;
}


static u8 sgm41513_iterm[]=
{
5 ,10,15,20,
30,40,50,60,
80,100,120,140,160,180,200,
240,
};

static int sgm4151x_set_prechrg_curr(struct sgm4151x_device *sgm, int pre_current)
{
	int reg_val;
	int i=0;

	if (pre_current > 240)
		pre_current = 240;

	while(pre_current > sgm41513_iterm[i])	i++;
	reg_val = i;
	pr_info("%s: %d\n", __func__, pre_current);
	reg_val = reg_val << 4;
	return sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_3,
				  SGM4151x_PRECHRG_CUR_MASK, reg_val);
}

static int sgm4151x_set_term_curr(struct sgm4151x_device *sgm, int term_current)
{
	u8 reg_val;
	int i=0;

	if (term_current > 240)
		term_current = 240;
	while(term_current > sgm41513_iterm[i])	i++;
	reg_val = i;
	pr_info("%s: %d\n", __func__, term_current);

	return sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_3,
				  		SGM4151x_TERMCHRG_CUR_MASK, reg_val);
}

static u32 sgm41513_ichg[]=
{
0,5,10,15,20,25,30,35,40,
50,60,70,80,90,100,110,
130,150,170,190,210,230,250,270,
300,330,360,390,420,450,480,510,540,
600,660,720,780,840,900,960,1020,1080,1140,1200,1260,1320,1380,1440,1500,
1620,1740,1860,1980,2100,2220,2340,2460,2580,2700,2820,2940,
3000,
};

static int sgm4151x_set_ichrg_curr(struct sgm4151x_device *sgm, int chrg_curr)
{
	unsigned int register_value;
	u8 i=0;
	int ret;

	chrg_curr = chrg_curr / 1000;

	if (chrg_curr > 3000)
		chrg_curr = 3000;

	pr_info("%s: %d\n", __func__, chrg_curr);

	while(chrg_curr >= sgm41513_ichg[i]) i++;

	register_value = i - 1;

	ret = sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_2,
				  SGM4151x_ICHRG_CUR_MASK, register_value);

	return ret;
}

/*REG04 VREG[7:3]*/
const unsigned int VBAT_CV_VTH[] = {
	3856000, 3888000, 3920000, 3952000,
	3984000, 4016000, 4048000, 4080000,
	4112000, 4144000, 4176000, 4208000,
	4240000, 4272000, 4304000, 4336000,
	4368000, 4400000, 4432000, 4464000,
	4496000, 4528000, 4560000, 4592000,
	4624000
};

static int sgm4151x_set_chrg_volt(struct sgm4151x_device *sgm, int chrg_volt)
{
	int ret;
	int reg_val;

	if (chrg_volt < SGM4151x_VREG_V_MIN_uV)
		chrg_volt = SGM4151x_VREG_V_MIN_uV;
	else if (chrg_volt > SGM4151x_VREG_V_MAX_uV)
		chrg_volt = SGM4151x_VREG_V_MAX_uV;

	pr_info("%s: %d\n", __func__, chrg_volt);
	reg_val = (chrg_volt-SGM4151x_VREG_V_MIN_uV) / SGM4151x_VREG_V_STEP_uV;
	//pr_info("sgm41513: set reg04_val = 0x%x\n", reg_val);
	reg_val = reg_val<<3;
	ret = sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_4,
				  SGM4151x_VREG_V_MASK, reg_val);

	return ret;
}

static int sgm4151x_set_input_curr_lim(struct sgm4151x_device *sgm, int iindpm)
{
	int ret;
	int reg_val;

	if (iindpm < SGM4151x_IINDPM_I_MIN_uA ||
			iindpm > SGM4151x_IINDPM_I_MAX_uA)
		return -EINVAL;
	pr_info("%s: %d\n", __func__, iindpm);
	reg_val = (iindpm-SGM4151x_IINDPM_I_MIN_uA) / SGM4151x_IINDPM_STEP_uA;
	//pr_info("sgm41513: set reg00_val = 0x%x\n", reg_val);
	return ret = sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_0,
				  SGM4151x_IINDPM_I_MASK, reg_val);
}

static int sgm4151x_set_watchdog_timer(struct sgm4151x_device *sgm, int time)
{
	int ret;
	u8 reg_val;

	if (time == 0)
		reg_val = SGM4151x_WDT_TIMER_DISABLE;
	else if (time == 40)
		reg_val = SGM4151x_WDT_TIMER_40S;
	else if (time == 80)
		reg_val = SGM4151x_WDT_TIMER_80S;
	else
		reg_val = SGM4151x_WDT_TIMER_160S;	

	ret = sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_5,
				SGM4151x_WDT_TIMER_MASK, reg_val);

	return ret;
}
static int sgm4151x_get_state(struct sgm4151x_device *sgm,
			     struct sgm4151x_state *state)
{
	//u8 chrg_stat;
	//u8 fault;
	//u8 chrg_param_0,chrg_param_1,chrg_param_2;
	int ret;
	u8 chrg_param_2;
#if 0
	ret = sgm4151x_read_reg(sgm, SGM4151x_CHRG_STAT, &chrg_stat);
	if (ret){
		ret = sgm4151x_read_reg(sgm, SGM4151x_CHRG_STAT, &chrg_stat);
		if (ret){
			pr_err("%s read SGM4151x_CHRG_STAT fail\n",__func__);
			return ret;
		}
	}

	state->chrg_type = chrg_stat & SGM4151x_VBUS_STAT_MASK;
	state->chrg_stat = chrg_stat & SGM4151x_CHG_STAT_MASK;
	state->online = !!(chrg_stat & SGM4151x_PG_STAT);
	state->therm_stat = !!(chrg_stat & SGM4151x_THERM_STAT);
	state->vsys_stat = !!(chrg_stat & SGM4151x_VSYS_STAT);

	pr_err("%s chrg_type = 0x%x; chrg_stat =0x%x; online = %d;\n",__func__,
				state->chrg_type,state->chrg_stat,state->online);

	ret = sgm4151x_read_reg(sgm, SGM4151x_CHRG_FAULT, &fault);
	if (ret){
		pr_err("%s read SGM4151x_CHRG_FAULT fail\n",__func__);
		return ret;
	}
	state->chrg_fault = fault;
	state->ntc_fault = fault & SGM4151x_TEMP_MASK;
	state->health = state->ntc_fault;
	ret = sgm4151x_read_reg(sgm, SGM4151x_CHRG_CTRL_0, &chrg_param_0);
	if (ret){
		pr_err("%s read SGM4151x_CHRG_CTRL_0 fail\n",__func__);
		return ret;
	}
	state->hiz_en = !!(chrg_param_0 & SGM4151x_HIZ_EN);

	ret = sgm4151x_read_reg(sgm, SGM4151x_CHRG_CTRL_5, &chrg_param_1);
	if (ret){
		pr_err("%s read SGM4151x_CHRG_CTRL_5 fail\n",__func__);
		return ret;
	}
	state->term_en = !!(chrg_param_1 & SGM4151x_TERM_EN);
#endif
	ret = sgm4151x_read_reg(sgm, SGM4151x_CHRG_CTRL_A, &chrg_param_2);
	if (ret){
		pr_err("%s read SGM4151x_CHRG_CTRL_a fail\n",__func__);
		return ret;
	}
	state->vbus_gd = !!(chrg_param_2 & SGM4151x_VBUS_GOOD);
	state->vindpm_stat = !!(chrg_param_2 & SGM4151x_VINDPM_STAT);
	state->iindpm_stat = !!(chrg_param_2 & SGM4151x_IINDPM_STAT);

	return 0;
}

int sgm4151x_enable_charger(struct sgm4151x_device *sgm, u8 chg_en)
{
    int ret;
	u8 reg_val;

    //dev_notice(sgm->dev, "%s:%d", __func__, chg_en);
	ret = sgm4151x_read_reg(sgm, SGM4151x_CHRG_CTRL_1, &reg_val);
	if (ret){
		pr_err("%s read SGM4151x_CHRG_CTRL_1 fail\n",__func__);
		return ret;
	}
	//dev_notice(sgm->dev, "%s:reg00 = 0x%x-------000", __func__, reg_val);
	reg_val = reg_val & SGM4151x_CHRG_EN;
	//dev_notice(sgm->dev, "%s:reg00 = 0x%x-------111", __func__, reg_val);
	chg_en = chg_en << 4;
	if(reg_val != chg_en)
    	ret = sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_1, SGM4151x_CHRG_EN,
                     chg_en);
    return ret;
}

static int sgm4151x_set_vac_ovp(struct sgm4151x_device *sgm,enum SGM4151x_OVP volt)
{
	int reg_val;

	reg_val = volt<<6;

	return sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_6,
				  SGM4151x_VAC_OVP_MASK, reg_val);
}

static int sgm4151x_set_recharge_volt(struct sgm4151x_device *sgm, int recharge_volt)
{
	int reg_val;

	reg_val = (recharge_volt - SGM4151x_VRECHRG_OFFSET_mV) / SGM4151x_VRECHRG_STEP_mV;

	return sgm4151x_update_bits(sgm, SGM4151x_CHRG_CTRL_4,
				  SGM4151x_VRECHARGE, reg_val);
}

static int sgm4151x_get_prop_charge_status(struct sgm4151x_device *sgm)
{


    if (sgm->state.chrg_stat == SGM4151x_TERM_CHRG)
        return POWER_SUPPLY_STATUS_FULL;

    if (sgm->state.chrg_stat == SGM4151x_PRECHRG ||
        sgm->state.chrg_stat == SGM4151x_FAST_CHRG)
        return POWER_SUPPLY_STATUS_CHARGING;
    else if (sgm->state.chrg_stat == SGM4151x_NOT_CHRGING)
        return POWER_SUPPLY_STATUS_NOT_CHARGING;
    else if (!sgm->state.vbus_gd)
        return POWER_SUPPLY_STATUS_DISCHARGING;
    else
        return POWER_SUPPLY_STATUS_UNKNOWN;

}

static int sgm4151x_property_is_writeable(struct power_supply *psy,
					 enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_STATUS:
		return true;
	default:
		return false;
	}
}
static int sgm4151x_charger_set_property(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	struct sgm4151x_device *sgm = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		sgm4151x_set_input_curr_lim(sgm, val->intval);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		sgm4151x_enable_charger(sgm, val->intval);
		if(sgm->chg_en != val->intval)
			sgm->chg_en = val->intval;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		sgm4151x_set_ichrg_curr(sgm, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		sgm4151x_set_chrg_volt(sgm, val->intval);
		break;

	default:
		rc = -EINVAL;
	}

	return rc;
}



static int sgm4151x_charger_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sgm4151x_device *sgm = power_supply_get_drvdata(psy);
	struct sgm4151x_state state;
	int ret = 0;

	mutex_lock(&sgm->lock);
	//ret = sgm4151x_get_state(sgm, &state);
	state = sgm->state;
	mutex_unlock(&sgm->lock);
	if (ret)
		return ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = sgm4151x_get_prop_charge_status(sgm);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		switch (state.chrg_stat) {		
		case SGM4151x_PRECHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case SGM4151x_FAST_CHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			break;		
		case SGM4151x_TERM_CHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			break;
		case SGM4151x_NOT_CHRGING:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			break;
		default:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		}
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = SGM4151x_MANUFACTURER;
		break;

	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = "sgm4151x";
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = state.online;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = state.vbus_gd;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = sgm4151x_power_supply_desc.type;
		break;
	
	case POWER_SUPPLY_PROP_HEALTH:
		if (state.chrg_fault & 0xF8)
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;

		switch (state.health) {
		case SGM4151x_TEMP_HOT:
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
			break;
		case SGM4151x_TEMP_WARM:
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
			break;
		case SGM4151x_TEMP_COOL:
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
			break;
		case SGM4151x_TEMP_COLD:
			val->intval = POWER_SUPPLY_HEALTH_COLD;
			break;
		}
		break;

	default:
		return -EINVAL;
	}

	return ret;
}
/*
static void sgm4151x_dump_register(struct sgm4151x_device * sgm)
{
	int i = 0;
	u8 reg = 0;

	for(i=0; i<=SGM4151x_CHRG_CTRL_F; i++) {
		sgm4151x_read_reg(sgm, i, &reg);
		pr_info("%s REG_%x    0x%X\n", __func__, i, reg);
	}
}
*/
static void charger_monitor_work_func(struct work_struct *work)
{
	int ret = 0;
	u8 regval = 0;
	struct sgm4151x_device * sgm = NULL;
	struct delayed_work *charge_monitor_work = NULL;
	struct sgm4151x_state state;

	charge_monitor_work = container_of(work, struct delayed_work, work);
	if(charge_monitor_work == NULL) {
		pr_err("Cann't get charge_monitor_work\n");
		return ;
	}
	sgm = container_of(charge_monitor_work, struct sgm4151x_device, charge_monitor_work);
	if(sgm == NULL) {
		pr_err("Cann't get sgm \n");
		return ;
	}

	ret = sgm4151x_get_state(sgm, &state);
	mutex_lock(&sgm->lock);
	sgm->state = state;
	mutex_unlock(&sgm->lock);

	if (!sgm->state.vbus_gd) {
		pr_info("%s not present vbus_gd \n",__func__);
		goto OUT;
	}

	if(sgm->chg_en){
		//sgm4151x_dump_register(sgm);
		pr_info("sgm4151x enable\n");
	}
	else{
		sgm4151x_read_reg(sgm, SGM4151x_CHRG_CTRL_1, &regval);
		pr_info("%s sgm4151x reg01 = 0x%02x \n", __func__, regval);
	}

OUT:
	schedule_delayed_work(&sgm->charge_monitor_work, 10*HZ);
}


static enum power_supply_property sgm4151x_power_supply_props[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_PRESENT,
};



static char *sgm4151x_charger_supplied_to[] = {
	"battery",	
};

static struct power_supply_desc sgm4151x_power_supply_desc = {
	.name = "sgm4151x-charger",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = sgm4151x_power_supply_props,
	.num_properties = ARRAY_SIZE(sgm4151x_power_supply_props),
	.get_property = sgm4151x_charger_get_property,
	.set_property = sgm4151x_charger_set_property,
	.property_is_writeable = sgm4151x_property_is_writeable,
};

static int sgm4151x_power_supply_init(struct sgm4151x_device *sgm,
							struct device *dev)
{
	struct power_supply_config psy_cfg = { .drv_data = sgm,
						.of_node = dev->of_node, };

	psy_cfg.supplied_to = sgm4151x_charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(sgm4151x_charger_supplied_to);

	sgm->charger = devm_power_supply_register(sgm->dev,
						 &sgm4151x_power_supply_desc,
						 &psy_cfg);
	if (IS_ERR(sgm->charger))
		return -EINVAL;
	return 0;
}

static int sgm4151x_hw_init(struct sgm4151x_device *sgm)
{
	int ret = 0;

	ret = sgm4151x_set_watchdog_timer(sgm, 0);
	if (ret)
		goto err_out;

	ret = sgm4151x_enable_charger(sgm, 0);
	if (ret)
		goto err_out;
	sgm->chg_en = 0;

	ret = sgm4151x_set_ichrg_curr(sgm, SGM4151x_ICHRG_I_DEF_uA);
	if (ret)
		goto err_out;

	ret = sgm4151x_set_prechrg_curr(sgm, 180);
	if (ret)
		goto err_out;

	ret = sgm4151x_set_chrg_volt(sgm, SGM4151x_VREG_V_DEF_uV);
	if (ret)
		goto err_out;

	ret = sgm4151x_set_term_curr(sgm, 180);
	if (ret)
		goto err_out;

	ret = sgm4151x_set_input_curr_lim(sgm, SGM4151x_IINDPM_DEF_uA);
	if (ret)
		goto err_out;

	ret = sgm4151x_set_vac_ovp(sgm, SGM4151x_OVP_14000mV);
	if (ret)
		goto err_out;

	ret = sgm4151x_set_recharge_volt(sgm, 100);//100mv or 200mv
	if (ret)
		goto err_out;

	return 0;

err_out:
	return ret;
}

static int sgm4151x_parse_dt(struct sgm4151x_device *sgm)
{
	int ret;
	int chg_en_gpio = 0;

	chg_en_gpio = of_get_named_gpio(sgm->dev->of_node, "sgm,chg-en-gpio", 0);
	if (!gpio_is_valid(chg_en_gpio))
	{
		dev_err(sgm->dev, "%s: %d gpio get failed\n", __func__, chg_en_gpio);
		return -EINVAL;
	}
	ret = gpio_request(chg_en_gpio, "sgm chg en pin");
	if (ret) {
		dev_err(sgm->dev, "%s: %d gpio request failed\n", __func__, chg_en_gpio);
		return ret;
	}
	gpio_direction_output(chg_en_gpio,0);//default enable charge

	return 0;
}

static int sgm4151x_suspend_notifier(struct notifier_block *nb,
                unsigned long event,
                void *dummy)
{
	struct sgm4151x_device *sgm = container_of(nb, struct sgm4151x_device, pm_nb);

	pr_info("sgm4151x event = %d \n", event);

	switch (event) {
		case PM_SUSPEND_PREPARE:
			pr_info("sgm4151x PM_SUSPEND \n");
			cancel_delayed_work_sync(&sgm->charge_monitor_work);
			return NOTIFY_OK;

		case PM_POST_SUSPEND:
			pr_info("sgm4151x PM_RESUME \n");
			schedule_delayed_work(&sgm->charge_monitor_work, 0);
			return NOTIFY_OK;

		default:
			return NOTIFY_DONE;
	}
}

static int sgm4151x_hw_chipid_detect(struct sgm4151x_device *sgm)
{
	int ret = 0;
	u8 val = 0;
	ret = sgm4151x_read_reg(sgm,SGM4151x_CHRG_CTRL_B,&val);
	if (ret < 0)
	{
		pr_err("[%s] read SGM4151x_CHRG_CTRL_b fail\n", __func__);
		return ret;
	}

	pr_info("[%s] Reg[0x0B]=0x%x\n", __func__,val);

	return val;
}

static int sgm4151x_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct sgm4151x_device *sgm;
	int ret;
	int chip_id = 0;

	pr_info("[%s] start\n", __func__);

	if(CHECK_THIS_DEV_DEBUG_AREADY_EXIT()==0)
	{
		return -EIO;
	}

	sgm = devm_kzalloc(dev, sizeof(*sgm), GFP_KERNEL);
	if (!sgm){
		pr_err("[%s] sgm is null!\n", __func__);
		return -ENOMEM;
	}

	sgm->client = client;
	sgm->dev = dev;

	mutex_init(&sgm->lock);
	mutex_init(&sgm->i2c_rw_lock);

	i2c_set_clientdata(client, sgm);

	chip_id = sgm4151x_hw_chipid_detect(sgm);
	if (chip_id != 0)
	{
		dev_err(dev, "sgm41513 i2c error!\n");
		return -ENOMEM;
	}

	// Customer customization
	ret = sgm4151x_parse_dt(sgm);
	if (ret) {
		dev_err(dev, "Failed to read device tree properties%d\n", ret);
		return ret;
	}

	INIT_DELAYED_WORK(&sgm->charge_monitor_work, charger_monitor_work_func);

	sgm->pm_nb.notifier_call = sgm4151x_suspend_notifier;
	register_pm_notifier(&sgm->pm_nb);

	ret = sgm4151x_power_supply_init(sgm, dev);
	if (ret) {
		dev_err(dev, "Failed to register power supply\n");
		goto error_out1;
	}

	ret = sgm4151x_hw_init(sgm);
	if (ret) {
		dev_err(dev, "Cannot initialize the chip.\n");
		goto error_out;
	}
	schedule_delayed_work(&sgm->charge_monitor_work, 1000);

	REGISTER_AND_INIT_ONTIM_DEBUG_FOR_THIS_DEV();

	pr_info("[%s] end\n", __func__);

	return ret;

error_out:
	power_supply_unregister(sgm->charger);

error_out1:
	mutex_destroy(&sgm->lock);
	mutex_destroy(&sgm->i2c_rw_lock);
	pr_info("[%s] error\n", __func__);

	return ret;
}

static int sgm4151x_charger_remove(struct i2c_client *client)
{
    struct sgm4151x_device *sgm = i2c_get_clientdata(client);

    cancel_delayed_work_sync(&sgm->charge_monitor_work);
    power_supply_unregister(sgm->charger);
	mutex_destroy(&sgm->lock);
	mutex_destroy(&sgm->i2c_rw_lock);
    return 0;
}
/*
static void sgm4151x_charger_shutdown(struct i2c_client *client)
{
    int ret = 0;
	
	struct sgm4151x_device *sgm = i2c_get_clientdata(client);
    ret = sgm4151x_enable_charger(sgm, 0);
    if (ret) {
        pr_err("Failed to disable charger, ret = %d\n", ret);
    }
    pr_info("sgm4151x_charger_shutdown\n");
}
*/
static const struct i2c_device_id sgm4151x_i2c_ids[] = {
	{ "sgm4151x", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, sgm4151x_i2c_ids);

static const struct of_device_id sgm4151x_of_match[] = {
	{ .compatible = "sgm,sgm4151x", },
	{ },
};
MODULE_DEVICE_TABLE(of, sgm4151x_of_match);

static struct i2c_driver sgm4151x_driver = {
	.driver = {
		.name = "sgm4151x-charger",
		.of_match_table = sgm4151x_of_match,
	},
	.probe = sgm4151x_probe,
	.remove = sgm4151x_charger_remove,
	//.shutdown = sgm4151x_charger_shutdown,
	.id_table = sgm4151x_i2c_ids,
};
module_i2c_driver(sgm4151x_driver);

MODULE_AUTHOR(" qhq <allen_qin@sg-micro.com>");
MODULE_DESCRIPTION("sgm4151x charger driver");
MODULE_LICENSE("GPL v2");
