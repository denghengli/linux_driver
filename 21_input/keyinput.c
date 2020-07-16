#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/semaphore.h>
#include <linux/timer.h> 
#include <linux/of_irq.h> 
#include <linux/irq.h> 
#include <asm/mach/map.h> 
#include <asm/uaccess.h> 
#include <asm/io.h>


/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: key.c
作者	  	: 左忠凯
版本	   	: V1.0
描述	   	: Linux按键输入驱动实验
其他	   	: 无
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2019/7/18 左忠凯创建
***************************************************************/
#define KEY_NAME		"keyinput"	/* 名字 		*/
#define KEY_NUM			1

/*key结构体*/
struct irq_key
{
	int gpio;		/*gpio编号*/
	int irqnum;		/*中断号*/
	int value;		/*按键值*/
	char name[10];	/*名字*/
	irqreturn_t (*handler)(int, void *); /*中断处理函数*/
};

/* key设备结构体 */
struct keyinput_dev{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	/* 类 		*/
	struct device *device;	/* 设备 	 */
	int major;				/* 主设备号	  */
	int minor;				/* 次设备号   */
	struct device_node	*nd; /* 设备节点 */

	struct timer_list timer;	/*消抖定时器*/
	struct irq_key irqkey[KEY_NUM]; /* 按键,可能会存在多个按键*/

	/*input结构体*/
	struct input_dev *inputdev;
};

struct keyinput_dev keyinputdev;		/* key设备 */

/*key中断处理函数*/
static irqreturn_t key_irq_func(int irq, void *dev_id)
{
	struct keyinput_dev *dev = (struct keyinput_dev*)dev_id;

	/*启动定时器进行消枓*/
	dev->timer.data = (unsigned long)dev_id;
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10));

	return IRQ_HANDLED;
}


/*定时器回调函数*/
static void timer_func(unsigned long arg)
{
    struct keyinput_dev *dev = (struct keyinput_dev*)arg;
	int value = 0;

	value = gpio_get_value(dev->irqkey[0].gpio);
	if (value == 0){	/*按下*/
		input_report_key(dev->inputdev, dev->irqkey[0].value, 1);/*1，按下*/
		input_sync(dev->inputdev);
		printk("key press!\r\n");
	} else if (value == 1) {	/*弹起*/
		input_report_key(dev->inputdev, dev->irqkey[0].value, 0);
		input_sync(dev->inputdev);
		printk("key release!\r\n");
	}
}

/*
 * @description	: 初始化按键IO，open函数打开驱动的时候
 * 				  初始化按键所使用的GPIO引脚。
 * @param 		: 无
 * @return 		: 无
 */
static int keyio_init(struct keyinput_dev *dev)
{
	int ret = 0;
	uint8_t i = 0;

	/*1、按键初始化*/
	dev->nd = of_find_node_by_path("/key");
	if (dev->nd== NULL) {
		return -EINVAL;
	}
	/*提取GPIO*/
	for (i = 0; i < KEY_NUM; i++)
	{
		dev->irqkey[i].gpio = of_get_named_gpio(dev->nd ,"key-gpio", i);
		if (dev->irqkey[i].gpio < 0) {
			printk("can't get key0\r\n");
			return -EINVAL;
		}
		printk("key%d_gpio=%d\r\n", i, dev->irqkey[i].gpio);
	}
	
	for ( i = 0; i < KEY_NUM; i++)
	{
		memset(dev->irqkey[i].name, 0, sizeof(dev->irqkey[i].name));
		sprintf(dev->irqkey[i].name, "key%d", i);
		gpio_request(dev->irqkey[i].gpio, dev->irqkey[i].name);	/* 请求IO */
		gpio_direction_input(dev->irqkey[i].gpio);	/* 设置为输入 */

		dev->irqkey[i].irqnum = gpio_to_irq(dev->irqkey[i].gpio);	/*获取中断号,作为GPIO的时候*/
		//dev->irqkey[i].irqnum = irq_of_parse_and_map(dev->nd, i) ;	/*获取中断号,通用方法*/
		printk("key%d_irq=%d\r\n", i, dev->irqkey[i].irqnum);
	}

	/*2、初始化按键使用的中断,tasklet_init为初始化中断的下半部，可以不需要，这里为实验*/
	for ( i = 0; i < KEY_NUM; i++)
	{
		dev->irqkey[i].value = KEY_0; //作为事件码
		dev->irqkey[i].handler = key_irq_func;
		ret = request_irq(dev->irqkey[i].irqnum, 
						  dev->irqkey[i].handler, 
                		  IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, 
                		  dev->irqkey[i].name, 
                		  dev);
		if (ret){
			printk("irq %d request failed!\r\n", dev->irqkey[i].irqnum);
			goto fail_irq;
		}
	}

	/*3、初始化定时器,没有开启*/
	init_timer(&dev->timer);
	dev->timer.function = timer_func;

fail_irq:
	for ( i = 0; i < KEY_NUM; i++)
	{
		gpio_free(dev->irqkey[i].gpio);	/* 释放IO */
	}
	
	return ret;
}


/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init keyinput_init(void)
{
	int ret = 0;

	/* 初始化按键IO */
	ret = keyio_init(&keyinputdev);				
	if (ret < 0) {
		return ret;
	}

	/*申请input_dev*/
	keyinputdev.inputdev = input_allocate_device();
	keyinputdev.inputdev->name = KEY_NAME;

	/*初始化 input_dev，设置产生那些事件*/
	__set_bit(EV_KEY, keyinputdev.inputdev->evbit); /*按键事件 */
	__set_bit(EV_REP, keyinputdev.inputdev->evbit); /* 重复事件 */

	/* 初始化input_dev，设置产生哪些按键 */
	__set_bit(KEY_0, keyinputdev.inputdev->keybit);

#if 0
	keyinputdev.inputdev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP); 
	keyinputdev.inputdev->keybit[BIT_WORD(KEY_0)] |= BIT_MASK(KEY_0);
#endif

#if 0
	keyinputdev.inputdev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP); 
	input_set_capability(keyinputdev.inputdev, EV_KEY, KEY_0);
#endif

	/*注册输入设备*/
	ret = input_register_device(keyinputdev.inputdev);
	if (ret) {
		printk("register input device failed! \r\n");
	}

	return ret;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit keyinput_exit(void)
{
	int i = 0;

	/*释放中断 和 GPIO*/
	for ( i = 0; i < KEY_NUM; i++)
	{
		free_irq(keyinputdev.irqkey[i].irqnum, &keyinputdev);
		gpio_free(keyinputdev.irqkey[i].gpio);	
	}

	/*删除定时器*/
	del_timer(&keyinputdev.timer);

	/* 释放 input_dev */
	input_unregister_device(keyinputdev.inputdev);
	input_free_device(keyinputdev.inputdev);
}

module_init(keyinput_init);
module_exit(keyinput_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("denghengli");
