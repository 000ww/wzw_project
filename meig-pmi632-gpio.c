#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>

static int pmi632_gpio6 = 0;
extern struct class *heater;
static ssize_t pwm_gpio_enable_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
	char temp[10];
	sprintf(temp,"%s",buf);
	if(!strncmp(temp,"1",1)) {
		gpio_direction_output(pmi632_gpio6,1);
	}else{
		gpio_direction_output(pmi632_gpio6,0);
	}
	return count;
}

static ssize_t pwm_gpio_enable_show(struct class *class,struct class_attribute *attr, char *buf)
{
	int value;
	value = gpio_get_value(pmi632_gpio6);
	if (value < 0)
		pr_err("get pmi632-gpio6 value failed\n");
	return snprintf(buf, 50, "pmi632-gpio6 value = %d\n",value);
}

static const struct class_attribute class_attr_pwm_gpio_enable = {
	.attr = {
		.name = "pwm_gpio_enable",
		.mode = 0644,
	},
	.show = pwm_gpio_enable_show,
	.store = pwm_gpio_enable_store,
};

static int pmi632_gpio6_enable_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int rc=0;

	pr_info("%s start...\n",__func__);
	if (!np) {
		pr_err("enable_pmi632_gpio6: have no node in dtsi\n");
		return -ENODEV;
	}
	pmi632_gpio6 = of_get_named_gpio(np, "pwm,pmi632-gpio6-enable", 0);
	if(pmi632_gpio6 < 0) {
		pr_err("pwm get pmi632_gpio6 failed\n");
		return -1;
	}
	rc = gpio_request(pmi632_gpio6,"pwm-pmi632-gpio-enable-5v");
	if(rc < 0) {
		pr_err("pwm request pmi632_gpio6 failed\n");
		return rc;
	}
	rc = gpio_direction_output(pmi632_gpio6,0);
	if(rc < 0) {
		pr_err("pwm enable pmi632_gpio6 out low fail\n");
		return rc;
	}
	rc = class_create_file(heater, &class_attr_pwm_gpio_enable);
	if(rc < 0) {
		pr_err("%s:fail to register heater\n",__func__);
	}
	pr_info("%s end...\n",__func__);

	return rc;
}

static int pmi632_gpio6_enable_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id of_pmi632_gpio6_match[] = {
	{.compatible = "pmi632-gpio6-enable"},
	{},
};
static struct platform_driver pmi632_gpio6_enable_driver = {
	.driver = {
		.name  = "pmi632-gpio6-enable",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_pmi632_gpio6_match),
	},
	.probe  = pmi632_gpio6_enable_probe,
	.remove = pmi632_gpio6_enable_remove,
};

static int __init pmi632_gpio6_init(void)
{
	return platform_driver_register(&pmi632_gpio6_enable_driver);
}

static void __exit pmi632_gpio6_exit(void)
{
	platform_driver_unregister(&pmi632_gpio6_enable_driver);
}

late_initcall(pmi632_gpio6_init);
module_exit(pmi632_gpio6_exit);
MODULE_DESCRIPTION("enable pwm gpio6 5v");
MODULE_LICENSE("GPL");
