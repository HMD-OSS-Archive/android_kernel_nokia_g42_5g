// SPDX-License-Identifier: GPL-2.0
// ETA6963 driver
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

#include "eta6963_charger.h"
static struct power_supply_desc eta6963_power_supply_desc;

#include <ontim_dev_dgb.h>
static  char charge_ic_vendor_name[50]="eta6963";
DEV_ATTR_DECLARE(charge_ic)
DEV_ATTR_DEFINE("vendor",charge_ic_vendor_name)
DEV_ATTR_DECLARE_END;
ONTIM_DEBUG_DECLARE_AND_INIT(charge_ic,charge_ic,8);


enum ETA6963_OVP {
	ETA6963_OVP_5500mV,
	ETA6963_OVP_6500mV,
	ETA6963_OVP_10500mV,
	ETA6963_OVP_14000mV,
};

static int __eta6963_read_byte(struct eta6963_device *eta, u8 reg, u8 *data)
{
    s32 ret;

    ret = i2c_smbus_read_byte_data(eta->client, reg);
    if (ret < 0) {
        pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
        return ret;
    }

    *data = (u8) ret;

    return 0;
}

static int __eta6963_write_byte(struct eta6963_device *eta, int reg, u8 val)
{
    s32 ret;

    ret = i2c_smbus_write_byte_data(eta->client, reg, val);
    if (ret < 0) {
        pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
               val, reg, ret);
        return ret;
    }
    return 0;
}

static int eta6963_read_reg(struct eta6963_device *eta, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&eta->i2c_rw_lock);
	ret = __eta6963_read_byte(eta, reg, data);
	mutex_unlock(&eta->i2c_rw_lock);

	return ret;
}

static int eta6963_update_bits(struct eta6963_device *eta, u8 reg,
					u8 mask, u8 val)
{
	int ret;
	u8 tmp;

	mutex_lock(&eta->i2c_rw_lock);
	ret = __eta6963_read_byte(eta, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= val & mask;

	ret = __eta6963_write_byte(eta, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&eta->i2c_rw_lock);
	return ret;
}

static int eta6963_set_prechrg_curr(struct eta6963_device *eta, int pre_current)
{
	int reg_val;

	reg_val = (pre_current - 60) / 60;
	reg_val = reg_val << 4;
	return eta6963_update_bits(eta, ETA6963_CHRG_CTRL_3,
								ETA6963_PRECHRG_CUR_MASK, reg_val);
}

static int eta6963_set_term_curr(struct eta6963_device *eta, int term_current)
{
	u8 reg_val;

	reg_val = (term_current - 60)/60;
	pr_info("eta6963: term_current = %d; reg03_val = 0x%x;\n", term_current, reg_val);

	return eta6963_update_bits(eta, ETA6963_CHRG_CTRL_3,
								ETA6963_TERMCHRG_CUR_MASK, reg_val);
}

static int eta6963_set_ichrg_curr(struct eta6963_device *eta, int chrg_curr)
{
	u8 ichg_reg_val;
	int ret;

	chrg_curr = chrg_curr / 1000;

	if (chrg_curr > 3000)
		chrg_curr = 3000;

	pr_info("eta6963: set chrg_curr = %d\n", chrg_curr);

	ichg_reg_val = chrg_curr/60;
	//pr_info("eta6963: reg02_val = 0x%x\n", ichg_reg_val);
	ret = eta6963_update_bits(eta, ETA6963_CHRG_CTRL_2, ETA6963_ICHRG_CUR_MASK, ichg_reg_val);
	return ret;
}

static int eta6963_set_chrg_volt(struct eta6963_device *eta, int chrg_volt)
{
	int ret;
	int reg_val;

	chrg_volt = chrg_volt/1000;

	pr_info("eta6963: set chrg_volt = %d\n", chrg_volt);
	reg_val = (chrg_volt-3848) / 32;
	//pr_info("eta6963: set reg04_val = 0x%x\n", reg_val);
	reg_val = reg_val<<3;
	ret = eta6963_update_bits(eta, ETA6963_CHRG_CTRL_4,
				  ETA6963_VREG_V_MASK, reg_val);

	return ret;
}

static int eta6963_set_input_curr_lim(struct eta6963_device *eta, int iindpm)
{
	int ret;
	int reg_val;

	iindpm = iindpm/1000;

	pr_info("eta6963: set input_curr_lim = %d\n", iindpm);
	reg_val = (iindpm-100) / 100;
	//pr_info("eta6963: set reg00_val = 0x%x\n", reg_val);
	return ret = eta6963_update_bits(eta, ETA6963_CHRG_CTRL_0,
				  ETA6963_IINDPM_I_MASK, reg_val);
}

static int eta6963_set_watchdog_timer(struct eta6963_device *eta, int time)
{
	int ret;
	u8 reg_val;

	if (time == 0)
		reg_val = ETA6963_WDT_TIMER_DISABLE;
	else if (time == 40)
		reg_val = ETA6963_WDT_TIMER_40S;
	else if (time == 80)
		reg_val = ETA6963_WDT_TIMER_80S;
	else
		reg_val = ETA6963_WDT_TIMER_160S;	

	ret = eta6963_update_bits(eta, ETA6963_CHRG_CTRL_5,
				ETA6963_WDT_TIMER_MASK, reg_val);

	return ret;
}

static int eta6963_get_state(struct eta6963_device *eta,
			     struct eta6963_state *state)
{
	u8 chrg_param_2;
	int ret;

	ret = eta6963_read_reg(eta, ETA6963_CHRG_CTRL_A, &chrg_param_2);
	if (ret){
		pr_err("%s read ETA6963_CHRG_CTRL_a fail\n",__func__);
		return ret;
	}
	state->vbus_gd = !!(chrg_param_2 & ETA6963_VBUS_GOOD);
	state->vindpm_stat = !!(chrg_param_2 & ETA6963_VINDPM_STAT);
	state->iindpm_stat = !!(chrg_param_2 & ETA6963_IINDPM_STAT);
	return 0;
}

int eta6963_enable_charger(struct eta6963_device *eta, u8 chg_en)
{
    int ret;
	u8 reg_val;

    dev_notice(eta->dev, "%s:%d", __func__, chg_en);
	ret = eta6963_read_reg(eta, ETA6963_CHRG_CTRL_1, &reg_val);
	if (ret){
		pr_err("%s read ETA6963_CHRG_CTRL_1 fail\n",__func__);
		return ret;
	}
	//dev_notice(eta->dev, "%s:reg00 = 0x%x-------000", __func__, reg_val);
	reg_val = reg_val & ETA6963_CHRG_EN;
	//dev_notice(eta->dev, "%s:reg00 = 0x%x-------111", __func__, reg_val);
	chg_en = chg_en << 4;
	if(reg_val == chg_en)
		pr_info("%s ETA6963 chg_en set already\n",__func__);
	else{
    	ret = eta6963_update_bits(eta, ETA6963_CHRG_CTRL_1, ETA6963_CHRG_EN,
                     chg_en);
	}
    return ret;
}

static int eta6963_set_vac_ovp(struct eta6963_device *eta,enum ETA6963_OVP volt)
{
	int reg_val;
	
	reg_val = volt<<6;

	return eta6963_update_bits(eta, ETA6963_CHRG_CTRL_6,
				  ETA6963_VAC_OVP_MASK, reg_val);
}

static int eta6963_set_recharge_volt(struct eta6963_device *eta, int recharge_volt)
{
	int reg_val;
	
	reg_val = (recharge_volt - ETA6963_VRECHRG_OFFSET_mV) / ETA6963_VRECHRG_STEP_mV;

	return eta6963_update_bits(eta, ETA6963_CHRG_CTRL_4,
				  ETA6963_VRECHARGE, reg_val);
}

static int eta6963_get_prop_charge_status(struct eta6963_device *eta)
{


    if (eta->state.chrg_stat == ETA6963_TERM_CHRG)
        return POWER_SUPPLY_STATUS_FULL;

    if (eta->state.chrg_stat == ETA6963_PRECHRG ||
        eta->state.chrg_stat == ETA6963_FAST_CHRG)
        return POWER_SUPPLY_STATUS_CHARGING;
    else if (eta->state.chrg_stat == ETA6963_NOT_CHRGING)
        return POWER_SUPPLY_STATUS_NOT_CHARGING;
    else if (!eta->state.vbus_gd)
        return POWER_SUPPLY_STATUS_DISCHARGING;
    else
        return POWER_SUPPLY_STATUS_UNKNOWN;

}

static int eta6963_property_is_writeable(struct power_supply *psy,
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
static int eta6963_charger_set_property(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	struct eta6963_device *eta = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		eta6963_set_input_curr_lim(eta, val->intval);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		eta6963_enable_charger(eta, val->intval);
		if(eta->chg_en != val->intval)
			eta->chg_en = val->intval;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		eta6963_set_ichrg_curr(eta, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		eta6963_set_chrg_volt(eta, val->intval);
		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}



static int eta6963_charger_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct eta6963_device *eta = power_supply_get_drvdata(psy);
	struct eta6963_state state;
	int ret = 0;

	mutex_lock(&eta->lock);
	//ret = eta6963_get_state(eta, &state);
	state = eta->state;
	mutex_unlock(&eta->lock);
	if (ret)
		return ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = eta6963_get_prop_charge_status(eta);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		switch (state.chrg_stat) {		
		case ETA6963_PRECHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case ETA6963_FAST_CHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			break;		
		case ETA6963_TERM_CHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			break;
		case ETA6963_NOT_CHRGING:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			break;
		default:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		}
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = ETA6963_MANUFACTURER;
		break;

	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = "eta6963";
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = state.online;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = state.vbus_gd;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = eta6963_power_supply_desc.type;
		break;
	
	case POWER_SUPPLY_PROP_HEALTH:
		if (state.chrg_fault & 0xF8)
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;

		switch (state.health) {
		case ETA6963_TEMP_HOT:
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
			break;
		case ETA6963_TEMP_WARM:
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
			break;
		case ETA6963_TEMP_COOL:
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
			break;
		case ETA6963_TEMP_COLD:
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
static void eta6963_dump_register(struct eta6963_device * eta)
{
	int i = 0;
	u8 reg = 0;

	for(i=0; i<=ETA6963_CHRG_CTRL_B; i++) {
		eta6963_read_reg(eta, i, &reg);
		pr_info("%s REG_%x    0x%X\n", __func__, i, reg);
	}
}
*/
static void charger_monitor_work_func2(struct work_struct *work)
{
	int ret = 0;
	u8 regval = 0;
	struct eta6963_device * eta = NULL;
	struct delayed_work *charge_monitor_work = NULL;
	struct eta6963_state state;

	charge_monitor_work = container_of(work, struct delayed_work, work);
	if(charge_monitor_work == NULL) {
		pr_err("Cann't get charge_monitor_work\n");
		return ;
	}
	eta = container_of(charge_monitor_work, struct eta6963_device, charge_monitor_work);
	if(eta == NULL) {
		pr_err("Cann't get eta \n");
		return ;
	}

	ret = eta6963_get_state(eta, &state);
	mutex_lock(&eta->lock);
	eta->state = state;
	mutex_unlock(&eta->lock);

	if (!eta->state.vbus_gd) {
		pr_info("%s not present vbus_gd \n",__func__);
		goto OUT;
	}

	if(eta->chg_en){
		//eta6963_dump_register(eta);
		pr_info("eta6963 enable\n");
	}
	else{
		eta6963_read_reg(eta, ETA6963_CHRG_CTRL_1, &regval);
		pr_info("%s eta6963 reg01 = 0x%02x \n", __func__, regval);
	}

OUT:
	schedule_delayed_work(&eta->charge_monitor_work, 10*HZ);
}


static enum power_supply_property eta6963_power_supply_props[] = {
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



static char *eta6963_charger_supplied_to[] = {
	"battery",	
};

static struct power_supply_desc eta6963_power_supply_desc = {
	.name = "eta6963-charger",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = eta6963_power_supply_props,
	.num_properties = ARRAY_SIZE(eta6963_power_supply_props),
	.get_property = eta6963_charger_get_property,
	.set_property = eta6963_charger_set_property,
	.property_is_writeable = eta6963_property_is_writeable,
};

static int eta6963_power_supply_init(struct eta6963_device *eta,
							struct device *dev)
{
	struct power_supply_config psy_cfg = { .drv_data = eta,
						.of_node = dev->of_node, };

	psy_cfg.supplied_to = eta6963_charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(eta6963_charger_supplied_to);

	eta->charger = devm_power_supply_register(eta->dev,
						 &eta6963_power_supply_desc,
						 &psy_cfg);
	if (IS_ERR(eta->charger))
		return -EINVAL;	
	return 0;
}

static int eta6963_hw_init(struct eta6963_device *eta)
{
	int ret = 0;

	ret = eta6963_set_watchdog_timer(eta, 0);
	if (ret)
		goto err_out;

	ret = eta6963_enable_charger(eta, 0);
	if (ret)
		goto err_out;
	eta->chg_en = 0;

	ret = eta6963_set_ichrg_curr(eta, ETA6963_ICHRG_I_DEF_uA);
	if (ret)
		goto err_out;

	ret = eta6963_set_prechrg_curr(eta, 180);
	if (ret)
		goto err_out;

	ret = eta6963_set_chrg_volt(eta, ETA6963_VREG_V_DEF_uV);
	if (ret)
		goto err_out;

	ret = eta6963_set_term_curr(eta, 180);
	if (ret)
		goto err_out;

	ret = eta6963_set_input_curr_lim(eta, 1920000);
	if (ret)
		goto err_out;

	ret = eta6963_set_vac_ovp(eta,ETA6963_OVP_14000mV);
	if (ret)
		goto err_out;	

	ret = eta6963_set_recharge_volt(eta, 100);//100mv or 200mv
	if (ret)
		goto err_out;

	return 0;

err_out:
	return ret;
}

static int eta6963_parse_dt(struct eta6963_device *eta)
{
	int ret;	
	int chg_en_gpio = 0;

	chg_en_gpio = of_get_named_gpio(eta->dev->of_node, "eta,chg-en-gpio", 0);
	if (!gpio_is_valid(chg_en_gpio))
	{
		dev_err(eta->dev, "%s: %d gpio get failed\n", __func__, chg_en_gpio);
		return -EINVAL;
	}
	ret = gpio_request(chg_en_gpio, "eta chg en pin");
	if (ret) {
		dev_err(eta->dev, "%s: %d gpio request failed\n", __func__, chg_en_gpio);
		return ret;
	}
	gpio_direction_output(chg_en_gpio,0);//default enable charge
	return 0;
}

static int eta6963_suspend_notifier(struct notifier_block *nb,
                unsigned long event,
                void *dummy)
{
	struct eta6963_device *eta = container_of(nb, struct eta6963_device, pm_nb);

	pr_info("eta6963 event = %d \n", event);

	switch (event) {
		case PM_SUSPEND_PREPARE:
			pr_info("eta6963 PM_SUSPEND \n");
			cancel_delayed_work_sync(&eta->charge_monitor_work);
			return NOTIFY_OK;

		case PM_POST_SUSPEND:
			pr_info("eta6963 PM_RESUME \n");
			schedule_delayed_work(&eta->charge_monitor_work, 0);
			return NOTIFY_OK;

		default:
			return NOTIFY_DONE;
	}
}

static int eta6963_hw_chipid_detect(struct eta6963_device *eta)
{
	int ret = 0;
	u8 val = 0;
	ret = eta6963_read_reg(eta,ETA6963_CHRG_CTRL_B,&val);
	if (ret < 0)
	{
		pr_err("[%s] read ETA6963_CHRG_CTRL_b fail\n", __func__);
		return ret;
	}		
	if (val != 0x3C)
	{
		pr_err("[%s] Reg[0x0B]=0x%x error\n", __func__,val);
		return -1;
	}
	
	return ret;
}

static int eta6963_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct eta6963_device *eta;
	int ret;
	int chip_id = 0;

	pr_info("[%s] start\n", __func__);

	if(CHECK_THIS_DEV_DEBUG_AREADY_EXIT()==0)
	{
		return -EIO;
	}

	eta = devm_kzalloc(dev, sizeof(*eta), GFP_KERNEL);
	if (!eta){
		pr_err("eta6963:[%s] eta is null!\n", __func__);
		return -ENOMEM;
	}

	eta->client = client;
	eta->dev = dev;

	mutex_init(&eta->lock);
	mutex_init(&eta->i2c_rw_lock);

	i2c_set_clientdata(client, eta);

	chip_id = eta6963_hw_chipid_detect(eta);
	if (chip_id < 0)	
	{
		dev_err(dev, "eta6963 i2c error\n");
		return -ENOMEM;
	}

	// Customer customization
	ret = eta6963_parse_dt(eta);
	if (ret) {
		dev_err(dev, "Failed to read device tree properties%d\n", ret);
		return ret;
	}

	INIT_DELAYED_WORK(&eta->charge_monitor_work, charger_monitor_work_func2);

	eta->pm_nb.notifier_call = eta6963_suspend_notifier;
	register_pm_notifier(&eta->pm_nb);

	ret = eta6963_power_supply_init(eta, dev);
	if (ret) {
		dev_err(dev, "Failed to register power supply\n");
		goto error_out1;
	}

	ret = eta6963_hw_init(eta);
	if (ret) {
		dev_err(dev, "Cannot initialize the chip.\n");
		goto error_out;
	}

	schedule_delayed_work(&eta->charge_monitor_work, 1000);

	REGISTER_AND_INIT_ONTIM_DEBUG_FOR_THIS_DEV();

	pr_info("[%s] end\n", __func__);

	return ret;

error_out:
	power_supply_unregister(eta->charger);

error_out1:
	mutex_destroy(&eta->lock);
	mutex_destroy(&eta->i2c_rw_lock);
	pr_info("[%s] error\n", __func__);

	return ret;

}

static int eta6963_charger_remove(struct i2c_client *client)
{
    struct eta6963_device *eta = i2c_get_clientdata(client);

    cancel_delayed_work_sync(&eta->charge_monitor_work);
    power_supply_unregister(eta->charger);
	mutex_destroy(&eta->lock);
	mutex_destroy(&eta->i2c_rw_lock);
    return 0;
}


static const struct i2c_device_id eta6963_i2c_ids[] = {
	{ "eta6963", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, eta6963_i2c_ids);

static const struct of_device_id eta6963_of_match[] = {
	{ .compatible = "eta,eta6963", },
	{ },
};
MODULE_DEVICE_TABLE(of, eta6963_of_match);

static struct i2c_driver eta6963_driver = {
	.driver = {
		.name = "eta6963-charger",
		.of_match_table = eta6963_of_match,
	},
	.probe = eta6963_probe,
	.remove = eta6963_charger_remove,
	.id_table = eta6963_i2c_ids,
};

module_i2c_driver(eta6963_driver);


MODULE_AUTHOR(" qhq <allen_qin@sg-micro.com>");
MODULE_DESCRIPTION("eta6963 charger driver");
MODULE_LICENSE("GPL v2");
