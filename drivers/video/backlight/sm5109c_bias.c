// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * sm5109c_bias.c - driver for SM5109C
 *
 * Author: tianrunlong<tianrunlong@chino-e.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

struct sm5109c_data {
	int enp_gpio;
	int enn_gpio;
	int poscntl_value;
	int negcntl_value;
	int on_delay;
	int off_delay;
};
static struct i2c_client *client_g = NULL;

int sm5109c_on()
{
	char write_data[2] = {0};
	struct sm5109c_data *data = i2c_get_clientdata(client_g);

	pr_err("LCM: to be sm5109c_on\n");
	if (data->enp_gpio < 0) {
		pr_err("LCM:can not set enp_gpio output high\n");
		return -1;
	}
	gpio_direction_output(data->enp_gpio, 1);

	if (data->poscntl_value > 0) {
		write_data[0] = 0x0;
		write_data[1] = data->poscntl_value;
		i2c_master_send(client_g, write_data, 2);
	}

	if (data->on_delay > 0)
		mdelay(data->on_delay);

	if (data->enn_gpio < 0) {
		pr_err("LCM:can not set enn_gpio output high\n");
		return -1;
	}
	gpio_direction_output(data->enn_gpio, 1);

	if (data->negcntl_value > 0) {
		write_data[0] = 0x1;
		write_data[1] = data->negcntl_value;
		i2c_master_send(client_g, write_data, 2);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sm5109c_on);

int sm5109c_off()
{
	struct sm5109c_data *data = i2c_get_clientdata(client_g);

	pr_err("LCM: to be sm5109c_off\n");
	if (data->enn_gpio < 0) {
		pr_err("LCM:can not set enn_gpio output low\n");
		return -1;
	}
	gpio_direction_output(data->enn_gpio, 0);

	if (data->off_delay > 0)
		mdelay(data->off_delay);

	if (data->enp_gpio < 0) {
		pr_err("LCM:can not set enp_gpio output low\n");
		return -1;
	}
	gpio_direction_output(data->enp_gpio, 0);

	return 0;
}
EXPORT_SYMBOL_GPL(sm5109c_off);

static int sm5109c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	static struct sm5109c_data *data = NULL;
	struct device_node *np = client->dev.of_node;

	pr_err("LCM: enter sm5109c_probe\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	data = devm_kzalloc(&client->dev, sizeof(struct sm5109c_data),
			GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	i2c_set_clientdata(client, data);
	client_g = client;

	if (np) {
		data->enp_gpio = of_get_named_gpio(np, "enp_gpio", 0);
		if (gpio_is_valid(data->enp_gpio))
			gpio_request(data->enp_gpio, "enp_gpio");
		else {
			pr_err("LCM:enp_gpio is invalid\n");
			data->enp_gpio = -1;
		}
		data->enn_gpio = of_get_named_gpio(np, "enn_gpio", 0);
		if (gpio_is_valid(data->enn_gpio))
			gpio_request(data->enn_gpio, "enn_gpio");
		else {
			pr_err("LCM:enn_gpio is invalid\n");
			data->enn_gpio = -1;
		}

		if (of_property_read_u32(np, "poscntl_value",
				&data->poscntl_value)) {
			pr_err("LCM:poscntl_value is NULL\n");
			data->poscntl_value = -1;
		}
		if (of_property_read_u32(np, "negcntl_value",
				&data->negcntl_value)) {
			pr_err("LCM:negcntl_value is NULL\n");
			data->negcntl_value = -1;
		}
		if (of_property_read_u32(np, "on_delay",
				&data->on_delay)) {
			pr_err("LCM:on_delay is NULL\n");
			data->on_delay = -1;
		}
		if (of_property_read_u32(np, "off_delay",
				&data->off_delay)) {
			pr_err("LCM:off_delay is NULL\n");
			data->off_delay = -1;
		}
	}
	pr_err("LCM: exit sm5109c_probe\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id of_sm5109c_match[] = {
	{ .compatible = "smi,sm5109c", },
	{ }
};
MODULE_DEVICE_TABLE(of, of_sm5109c_match);
#endif

static struct i2c_driver sm5109c_driver = {
	.driver = {
		.name = "sm5109c",
		.of_match_table = of_match_ptr(of_sm5109c_match),
	},
	.probe = sm5109c_probe,
};

module_i2c_driver(sm5109c_driver);

MODULE_AUTHOR("ontim lcm team");
MODULE_DESCRIPTION("SM5109C driver");
MODULE_LICENSE("GPL");
