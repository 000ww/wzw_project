#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/input/mt.h>

#define USB_HALL_BUFF_SIZE (32)

struct usb_hall_data {
	int usb_hall_gpio;
	int usb_hall_irq;
	u32 usb_hall_gpio_flags;
	struct workqueue_struct *usb_hall_workqueue;
	struct work_struct usb_hall_work;
};

static ssize_t state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_hall_data *dat = dev_get_drvdata(dev);
	int value = 0;
	value = gpio_get_value(dat->usb_hall_gpio);
	return snprintf(buf, USB_HALL_BUFF_SIZE,"%d\n", value);
}

static DEVICE_ATTR(state, S_IWUSR | S_IRUGO, state_show, NULL);

static struct device_attribute *state_attrs[] = {
	&dev_attr_state,
};

/*
 *config workqueue func
 *
 */
static void usb_hall_work_func(struct work_struct *work)
{
	int io_state = 0;
	struct usb_hall_data *dat = container_of(work, struct usb_hall_data, usb_hall_work);
	io_state = gpio_get_value(dat->usb_hall_gpio);
	if(io_state){
		pr_err("usb_hall>>>%s: usb_hall_gpio gpio%d change to %d\n", __func__, dat->usb_hall_gpio, io_state);
	}else{
		pr_err("usb_hall>>>%s: usb_hall_gpio gpio%d change to %d\n", __func__, dat->usb_hall_gpio, io_state);
	}
	enable_irq(dat->usb_hall_irq);
}

static irqreturn_t usb_hall_irq_handler(int irq, void *dev_id)
{
	struct usb_hall_data *dat = dev_id;
	bool rc;
	disable_irq_nosync(dat->usb_hall_irq);
	mdelay(400);
	rc = queue_work(dat->usb_hall_workqueue, &dat->usb_hall_work);
	if (!rc){
		pr_err("usb_hall>>>%s: queue_work fail!!!\n", __func__);
	}
	return IRQ_HANDLED;
}

static int usb_hall_request_irq(struct usb_hall_data *dat)
{
	int ret = 0;
	ret = request_threaded_irq(dat->usb_hall_irq, NULL, usb_hall_irq_handler, dat->usb_hall_gpio_flags, "usb_hall", dat);
	if(ret){
		pr_err("usb_hall>>>%s: request_threaded_irq usb_hall fail\n", __func__);
	}else{
		irq_set_irq_wake(dat->usb_hall_irq, 1);
		disable_irq_nosync(dat->usb_hall_irq);
	}
	return ret;
}

static int usb_hall_parse_and_config_io(struct usb_hall_data *dat, struct device_node *node)
{
	int usb_hall_gpio;
	u32 usb_hall_gpio_flags;

	//const char *usb_hall_irq_det = "meig,hall-irq-gpio";
/* get gpio node in dts*/
	usb_hall_gpio = of_get_named_gpio_flags(node, "meig,hall-irq-gpio", 0, &usb_hall_gpio_flags);

	dat->usb_hall_gpio = usb_hall_gpio;
	dat->usb_hall_gpio_flags = usb_hall_gpio_flags;
	//init output gpio configurations
	if(!gpio_is_valid(dat->usb_hall_gpio)){
		pr_err("usb_hall>>>%s: fail @gpio_is_valid, usb_hall_gpio=%d\n", __func__, dat->usb_hall_gpio);
	}else{
		if(gpio_request(dat->usb_hall_gpio, "meig,hall-irq-gpio") != 0){
			gpio_free(dat->usb_hall_gpio);
			pr_err("usb_hall>>>%s: fail @gpio_request, usb_hall_gpio=%d\n", __func__, dat->usb_hall_gpio);
		}else{
			if(gpio_direction_input(dat->usb_hall_gpio)){
				pr_err("usb_hall>>>%s: fail @gpio_direction_input, usb_hall_gpio=%d\n", __func__, dat->usb_hall_gpio);
			}else{
				dat->usb_hall_irq = gpio_to_irq(dat->usb_hall_gpio);
			}
		}
	}
	return 0;
}

static int usb_hall_probe(struct platform_device *pdev)
{
	struct usb_hall_data *usb_hall_dat = NULL;
	struct device_node *np = pdev->dev.of_node;
	int rc = 0, i=0;
	if (!np) {
		pr_err("[meig]usb hall: have no node in dtsi\n");
		return -ENODEV;
	}
	usb_hall_dat = devm_kzalloc(&pdev->dev, sizeof(struct usb_hall_data), GFP_KERNEL);
	if (!usb_hall_dat) {
		pr_err("usb_hall>>>>%s: malloc usb_hall fail\n", __func__);
		return -ENOMEM;
	}
/*parse and config io*/
	if (usb_hall_parse_and_config_io(usb_hall_dat, np) <0)
		goto err;
	dev_set_drvdata(&pdev->dev, usb_hall_dat);

/*create sysfs nodes*/
	for (i = 0; i < ARRAY_SIZE(state_attrs); i++) {
		rc = device_create_file(&pdev->dev, state_attrs[i]);
		if (rc) {
			pr_err("usb_hall>>>>%s: create sysfs files fail\n", __func__);
			goto err;
		}
	}

	usb_hall_dat->usb_hall_workqueue = create_singlethread_workqueue("usb_hall_workqueue");
	if (!usb_hall_dat->usb_hall_workqueue) 
	{
		pr_err("usb_hall>>>%s: create_singlethread_workqueue fail!!!\n",__func__);
		goto err;
	}
	
	INIT_WORK(&usb_hall_dat->usb_hall_work, usb_hall_work_func);
	
	if(usb_hall_request_irq(usb_hall_dat))
		pr_err("usb_hall>>>%s: call usb_hall_request_irq failed\n", __func__);
	
	enable_irq(usb_hall_dat->usb_hall_irq);

	pr_err("usb_hall>>>%s: probe succeed\n",__func__);
	return 0;
err:
	if (usb_hall_dat)
		devm_kfree(&pdev->dev, usb_hall_dat);
	return -1;
}

static int usb_hall_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct of_device_id of_usb_hall_match[] = {
	{.compatible = "meig,usb-hall"},
	{},
};

static struct platform_driver usb_hall_driver = {
	.probe      = usb_hall_probe,
	.remove     = usb_hall_remove,
	.driver     = {
		.name   = "usb-hall",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(of_usb_hall_match),
	},
};

static int __init usb_hall_init(void)
{
	return platform_driver_register(&usb_hall_driver);
}

static void __exit usb_hall_exit(void)
{
	platform_driver_unregister(&usb_hall_driver);
}

late_initcall(usb_hall_init);
module_exit(usb_hall_exit);
MODULE_DESCRIPTION("Meig usb hall driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:meig-usb-hall-driver");