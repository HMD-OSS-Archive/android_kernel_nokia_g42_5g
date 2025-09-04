// SPDX-License-Identifier: GPL-2.0-only
/*
 * LEDs driver for Sunlight
 *
 * Copyright (C) 2020 wenchao <chao.wen@chino-e.com>
 *
 * Based on leds-ams-delta.c
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/backlight.h>
#include <linux/notifier.h>
#define DRVNAME "sunlight"

struct mutex sunlight_lock;
struct platform_device *pdev = NULL;
static void sunlight_led_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{
	mutex_lock(&sunlight_lock);
	mutex_unlock(&sunlight_lock);
}

static struct led_classdev sunlight_led = {
	.name = "sunlight_led",
	.brightness_set = sunlight_led_set,
	.flags = LED_CORE_SUSPENDRESUME,
};

static int sunlight_led_probe(struct platform_device *pdev)
{
	int ret ;
	mutex_init(&sunlight_lock);
	ret = devm_led_classdev_register(&pdev->dev, &sunlight_led);
	if(ret < 0){
		dev_err(&pdev->dev, "can't register LED %s\n", sunlight_led.name);
		mutex_destroy(&sunlight_lock);
	}
	return ret;
}

static struct platform_driver sunlight_led_driver = {
	.probe = sunlight_led_probe,
	.driver = {
		.name = DRVNAME,
	},
};


static int __init sunlight_led_init(void)
{
	int ret;
	ret = platform_driver_register(&sunlight_led_driver);
	if (ret < 0)
		goto out;

	pdev = platform_device_register_simple(DRVNAME, -1, NULL, 0);
	if (IS_ERR(pdev)) {
		ret = PTR_ERR(pdev);
		platform_driver_unregister(&sunlight_led_driver);
		goto out;
	}

out:
	return ret;
}

static void __exit sunlight_led_exit(void)
{
	platform_device_unregister(pdev);
	platform_driver_unregister(&sunlight_led_driver);
}

module_init(sunlight_led_init);
module_exit(sunlight_led_exit);

MODULE_AUTHOR("wenchao <chao.wen@chino-e.com>");
MODULE_DESCRIPTION("chino-e sunlight led driver");
MODULE_LICENSE("GPL");
