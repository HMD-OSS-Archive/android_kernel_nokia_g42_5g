#include <linux/device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

unsigned int platform_board_id;
unsigned int platform_sku_id;

static const char * const boardid_gpios[] = {
	"gpio,boardid0",
	"gpio,boardid1",
};

static const char * const skuid_gpios[] = {
	"gpio,skuid0",
	"gpio,skuid1",
	"gpio,skuid2",
};

struct gpio_data {
	int value;
	int nfcvalue;
	int prjvalue;
	struct boardid *boardid_gpio;
	struct boardid *skuid_gpio;
};

struct boardid {
	unsigned int gpio;
	char gpio_name[32];
};

static int parse_dt_get_gpios(struct device *dev, const char *const names[],
				struct boardid **gpios, int size)
{
	int i = 0;
	struct device_node *of_node = dev->of_node;
	struct boardid *tmp_gpios = NULL;

	tmp_gpios = devm_kzalloc(dev, sizeof(struct boardid) * size, GFP_KERNEL);
	if (!tmp_gpios)
		return -ENOMEM;

	for (i = 0; i < size; i++) {
		tmp_gpios[i].gpio = of_get_named_gpio(of_node, names[i], 0);
		if (tmp_gpios[i].gpio < 0) {
			devm_kfree(dev, tmp_gpios);
			dev_err(dev, " dts get gpio error\n");
			return -EINVAL;
		}

		strlcpy(tmp_gpios[i].gpio_name, names[i], sizeof(tmp_gpios[i].gpio_name));
		printk(KERN_ERR "gpios[%d].gpio = %d, gpios[%d].gpio_name = %s",
						i, tmp_gpios[i].gpio,
						i, tmp_gpios[i].gpio_name);
	}

	*gpios = tmp_gpios;
	return 0;
}


static int nlsx_parse_dt(struct device *dev, struct gpio_data *data)
{
	int ret = 0;

	ret = parse_dt_get_gpios(dev, boardid_gpios,
			&(data->boardid_gpio), ARRAY_SIZE(boardid_gpios));
	if (ret < 0) {
		dev_err(dev, "fail to get boardid_gpios\n");
		return ret;
	}

	ret = parse_dt_get_gpios(dev, skuid_gpios,
			&(data->skuid_gpio), ARRAY_SIZE(skuid_gpios));
	return ret;
}

static int gpio_boardid_suspend(struct device *dev)
{
	return 0;
}

static int gpio_boardid_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops gpio_boardid_pm_ops = {
	.suspend = gpio_boardid_suspend,
	.resume = gpio_boardid_resume,
};

static int boardid_get_gpio_value(struct device *dev, struct boardid *gpios, int size, int *value)
{
	int i = 0;
	int err = 0;
	int tmp = 0;

	for (i = 0; i < size; i++) {
		if (gpio_is_valid(gpios[i].gpio)) {
			err = gpio_request(gpios[i].gpio, gpios[i].gpio_name);
			if (err) {
				dev_err(dev, "request %d gpio failed, err = %d\n", gpios[i].gpio, err);
				goto err_gpio_request;
			}

			err = gpio_direction_input(gpios[i].gpio);
			if (err) {
				dev_err(dev, "gpio %d set input failed\n", gpios[i].gpio);
				goto err_gpio_request;
			}

			tmp |= ((gpio_get_value(gpios[i].gpio)) ? (1 << i) : 0);
		}
	}

	*value = tmp;
	return 0;

err_gpio_request:
	for (; i >= 0; i--) {
		if (gpio_is_valid(gpios[i].gpio)) {
			gpio_free(gpios[i].gpio);
		}
	}
	return err;
}

static void boardid_free_gpio(struct boardid *gpios, int size)
{
	int i = 0;

	for (i = 0; i < size; i++) {
		if (gpio_is_valid(gpios[i].gpio)) {
			gpio_free(gpios[i].gpio);
		}
	}
}

static int gpio_boardid_probe(struct platform_device *pdev)
{
	struct gpio_data *data;
	int err = 0;
	int id_value = 0;

	printk(KERN_ERR "%s start\n", __func__);
	data = devm_kzalloc(&pdev->dev, sizeof(struct gpio_data), GFP_KERNEL);
	if (data == NULL) {
		err = -ENOMEM;
		dev_err(&pdev->dev, "failed to allocate memory %d\n", err);
		goto alloc_data_failed;
	}

	dev_set_drvdata(&pdev->dev, data);
	if (pdev->dev.of_node) {
		err = nlsx_parse_dt(&pdev->dev, data);
		if (err < 0) {
			dev_err(&pdev->dev, "Failed to parse device tree\n");
			goto parse_dt_failed;
		}
	} else if (pdev->dev.platform_data != NULL) {
		memcpy(data, pdev->dev.platform_data, sizeof(*data));
	} else {
		dev_err(&pdev->dev, "No valid platform data.\n");
		err = -ENODEV;
		goto invalid_pdata;
	}

	err = boardid_get_gpio_value(&pdev->dev, data->boardid_gpio,
					ARRAY_SIZE(boardid_gpios), &id_value);
	if (err < 0)
		goto boardid_get_err;
	platform_board_id = id_value;
	printk(KERN_ERR "deng %s ok boardid= %04d, value = %04d\n",
					__func__, platform_board_id, id_value);

	err = boardid_get_gpio_value(&pdev->dev, data->skuid_gpio,
					ARRAY_SIZE(skuid_gpios), &id_value);
	if (err < 0)
		goto prjid_get_err;
	platform_sku_id = id_value;
	printk(KERN_ERR "yrn %s ok sku_id= %04d, value = %04d\n",
					__func__, platform_sku_id, id_value);

prjid_get_err:
	boardid_free_gpio(data->boardid_gpio, ARRAY_SIZE(boardid_gpios));
boardid_get_err:
invalid_pdata:
parse_dt_failed:
	devm_kfree(&pdev->dev, data);
alloc_data_failed:
	return err;
}

static int gpio_boardid_remove(struct platform_device *pdev)
{
	struct gpio_data *data = dev_get_drvdata(&pdev->dev);

	boardid_free_gpio(data->skuid_gpio, ARRAY_SIZE(skuid_gpios));
	boardid_free_gpio(data->boardid_gpio, ARRAY_SIZE(boardid_gpios));
	devm_kfree(&pdev->dev, data);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct of_device_id platform_match_table[] = {
	{ .compatible = "gpio,boardid",},
	{ },
};

static struct platform_driver gpio_boardid_driver = {
	.driver = {
		.name           = "gpio_boardid",
		.of_match_table	= platform_match_table,
		.pm             = &gpio_boardid_pm_ops,
		.owner	        = THIS_MODULE,
	},
	.probe      = gpio_boardid_probe,
	.remove     = gpio_boardid_remove,
};

static int __init gpio_boardid_init(void)
{
	    return platform_driver_register(&gpio_boardid_driver);
}
module_init(gpio_boardid_init);

static void __exit gpio_boardid_exit(void)
{
	    return platform_driver_unregister(&gpio_boardid_driver);
}
module_exit(gpio_boardid_exit);

MODULE_DESCRIPTION("gpio boardid driver");
MODULE_LICENSE("GPL");
