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
#include <linux/semaphore.h>
#include <linux/timer.h> 
#include <linux/of_irq.h> 
#include <linux/irq.h> 
#include <asm/mach/map.h> 
#include <asm/uaccess.h> 
#include <asm/io.h>
#include <linux/wait.h> 
#include <linux/ide.h> 
#include <linux/poll.h> 

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
#define KEY_CNT			1		/* 设备号个数 	*/
#define KEY_NAME		"keyirq"	/* 名字 		*/

/* 定义按键值 */
#define KEY_VALUE		0X0F	/* 按键按下键值 */
#define KEY_INVAL		0X00	/* 无效的按键值 */
#define KEY_NUM			1

/*key结构体*/
struct irq_key
{
	int gpio;		/*gpio编号*/
	int irqnum;		/*中断号*/
	int value;		/*按键值*/
	char name[10];	/*名字*/
	irqreturn_t (*handler)(int, void *); /*中断处理函数*/

	/*tasklet*/
	struct tasklet_struct tasklet; 
	void (*tasklet_func)(unsigned long);

	/*work*/
//	struct work_struct work;
//	void (*work_func)(struct work_struct *work);
};

/* key设备结构体 */
struct key_dev{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	/* 类 		*/
	struct device *device;	/* 设备 	 */
	int major;				/* 主设备号	  */
	int minor;				/* 次设备号   */
	struct device_node	*nd; /* 设备节点 */

	atomic_t press_value;	/*按下时的键值*/
	atomic_t reles_value;	/*弹起时的键值*/
	struct timer_list timer;	/*消抖定时器*/
	struct irq_key irqkey[KEY_NUM]; /* 按键,可能会存在多个按键*/

	/*work*/
	struct work_struct work;
	void (*work_func)(struct work_struct *work);

	/*等待队列*/
	wait_queue_head_t r_wait;

};

struct key_dev keydev;		/* key设备 */

/*key中断处理函数*/
static irqreturn_t key_irq_func(int irq, void *dev_id)
{
	struct key_dev *dev = (struct key_dev*)dev_id;

	/*启动定时器进行消枓*/
	dev->timer.data = (unsigned long)dev_id;
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10));

	/*调度tasklet*/
	//tasklet_schedule(&dev->irqkey[0].tasklet);
	/*调度work*/
	schedule_work(&dev->work);

	return IRQ_HANDLED;
}
/*中断下半部初始化*/
static void key_tasklet_func(unsigned long data)
{
	struct key_dev *dev = (struct key_dev*)data;

	/*启动定时器进行消枓*/
	dev->timer.data = (unsigned long)data;
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10));
}
static void work_func(struct work_struct *work)
{
	struct key_dev *dev = container_of(work, struct key_dev, work);

	/*启动定时器进行消枓*/
	dev->timer.data = (unsigned long)dev;
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10));
}

/*定时器回调函数*/
static void timer_func(unsigned long arg)
{
    struct key_dev *dev = (struct key_dev*)arg;
	int value = 0;

	value = gpio_get_value(dev->irqkey[0].gpio);
	if (value == 0){	/*按下*/
		atomic_set(&dev->press_value, dev->irqkey[0].value);
		printk("key press!\r\n");
	} else if (value == 1) {	/*未按下*/
		atomic_set(&dev->press_value, 0x80 | dev->irqkey[0].value);
		atomic_set(&dev->reles_value, 1);

		wake_up(&dev->r_wait);/*唤醒等待队列中的线程*/
		//wake_up_interruptible(&dev->r_wait);
	}
}

/*
 * @description	: 初始化按键IO，open函数打开驱动的时候
 * 				  初始化按键所使用的GPIO引脚。
 * @param 		: 无
 * @return 		: 无
 */
static int keyio_init(struct key_dev *dev)
{
	int ret = 0;
	uint8_t i = 0;

	/*1、按键初始化*/
	dev->nd = of_find_node_by_path("/key");
	if (dev->nd== NULL) {
		return -EINVAL;
	}

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
		dev->irqkey[i].value = KEY_VALUE;
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
		/*tasklet*/
		dev->irqkey[i].tasklet_func = key_tasklet_func;
		tasklet_init(&dev->irqkey[i].tasklet, dev->irqkey[i].tasklet_func, (unsigned long)dev);
		/*work*/
		dev->work_func = work_func;
		INIT_WORK(&dev->work, dev->work_func);
	}

	/*3、初始化定时器,没有开启*/
	init_timer(&dev->timer);
	dev->timer.function = timer_func;

	/*初始化原子变量*/
	atomic_set(&dev->press_value, KEY_INVAL);/* 初始化原子变量 */
	atomic_set(&dev->reles_value, KEY_INVAL);/* 初始化原子变量 */

	/*初始化等待队列*/
	init_waitqueue_head(&dev->r_wait);

fail_irq:
	for ( i = 0; i < KEY_NUM; i++)
	{
		gpio_free(dev->irqkey[i].gpio);	/* 释放IO */
	}
	
	return ret;
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int key_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	filp->private_data = &keydev; 	/* 设置私有数据 */

	ret = keyio_init(&keydev);				/* 初始化按键IO */
	if (ret < 0) {
		return ret;
	}

	return 0;
}

/*
 * @description		: 从设备读取数据 
 * @param - filp 	: 要打开的设备文件(文件描述符)
 * @param - buf 	: 返回给用户空间的数据缓冲区
 * @param - cnt 	: 要读取的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 0表示成功，如果为负值，表示读取失败
 */
static ssize_t key_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	int ret = 0;
	int press_value, reles_value;
	struct key_dev *dev = filp->private_data;

	reles_value = atomic_read(&dev->reles_value);
	press_value = atomic_read(&dev->press_value);

	if (filp->f_flags & O_NONBLOCK){//非阻塞访问方式
		if (reles_value == 0){
			return -EAGAIN;
		}
	} else { //阻塞访问方式
#if 0
		wait_event_interruptible(dev->r_wait, reles_value);
#else
		/*定义并初始化一个等待队列项*/
		DECLARE_WAITQUEUE(wait, current);
		if (reles_value == 0){
			add_wait_queue(&dev->r_wait, &wait);//将队列项加入等待队列
			__set_current_state(TASK_INTERRUPTIBLE);/* 设置任务状态，设置为可被打断的状态,可以接受信号 */
			schedule(); /* 进行一次任务切换 */

			/*等待被唤醒，可能被信号和可用唤醒*/
			if(signal_pending(current)) { /* 判断是否为信号引起的唤醒 */
				__set_current_state(TASK_RUNNING); /*设置为运行状态 */
				remove_wait_queue(&dev->r_wait, &wait); /*将等待队列移除 */
				return -ERESTARTSYS;
			}
			/*可用设备唤醒*/
			__set_current_state(TASK_RUNNING); /*设置为运行状态 */
			remove_wait_queue(&dev->r_wait, &wait); /*将等待队列移除 */
		}
#endif
	}

	if (reles_value)
	{
		if (press_value & 0x80) /*有效的一次按下弹起过程*/
		{	
			press_value &= ~0x80;
			ret = copy_to_user(buf, &press_value, sizeof(press_value));
		} else {
			return -EINVAL;
		}
		atomic_set(&dev->reles_value, 0);
		atomic_set(&dev->press_value, 0);
	} else {
		return -EINVAL;
	}

	return ret;
}

/*
 * @description		: 向设备写数据 
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t key_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int key_release(struct inode *inode, struct file *filp)
{
	int i = 0;
	struct key_dev *dev = filp->private_data;

	/*释放中断 和 GPIO*/
	for ( i = 0; i < KEY_NUM; i++)
	{
		free_irq(dev->irqkey[i].irqnum, dev);
		gpio_free(dev->irqkey[i].gpio);	
	}

	/*删除定时器*/
	del_timer(&dev->timer);

	return 0;
}

static unsigned int key_poll(struct file *filp, struct poll_table_struct *wait)
{
	int mask = 0;
	struct key_dev *dev = filp->private_data;

	/*把等待队列头添加到等poll_table中，不会引起阻塞*/
	poll_wait(filp, &dev->r_wait, wait);

	/*是否可读*/
	if (atomic_read(&dev->reles_value)){
		mask = POLLIN | POLLRDNORM; //表示数据可读
	}

	return mask;
}

/* 设备操作函数 */
static struct file_operations key_fops = {
	.owner = THIS_MODULE,
	.open = key_open,
	.read = key_read,
	.write = key_write,
	.release = 	key_release,
	.poll = key_poll,
};

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init mykey_init(void)
{
	int ret = 0;

	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if (keydev.major) {		/*  定义了设备号 */
		keydev.devid = MKDEV(keydev.major, 0);
		register_chrdev_region(keydev.devid, KEY_CNT, KEY_NAME);
	} else {						/* 没有定义设备号 */
		alloc_chrdev_region(&keydev.devid, 0, KEY_CNT, KEY_NAME);	/* 申请设备号 */
		keydev.major = MAJOR(keydev.devid);	/* 获取分配号的主设备号 */
		keydev.minor = MINOR(keydev.devid);	/* 获取分配号的次设备号 */
	}
	
	/* 2、初始化cdev */
	keydev.cdev.owner = THIS_MODULE;
	cdev_init(&keydev.cdev, &key_fops);
	
	/* 3、添加一个cdev */
	cdev_add(&keydev.cdev, keydev.devid, KEY_CNT);

	/* 4、创建类 */
	keydev.class = class_create(THIS_MODULE, KEY_NAME);
	if (IS_ERR(keydev.class)) {
		return PTR_ERR(keydev.class);
	}

	/* 5、创建设备 */
	keydev.device = device_create(keydev.class, NULL, keydev.devid, NULL, KEY_NAME);
	if (IS_ERR(keydev.device)) {
		return PTR_ERR(keydev.device);
	}
	
#if 0
	ret = keyio_init(&keydev);				/* 初始化按键IO */
	if (ret < 0) {
		return ret;
	}
#endif

	return ret;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit mykey_exit(void)
{
#if 0
	int i = 0;
	/*释放中断 和 gpio*/
	for ( i = 0; i < KEY_NUM; i++)
	{
		free_irq(keydev.irqkey[i].irqnum, &keydev);
		gpio_free(keydev.irqkey[i].gpio);
	}
	/*删除定时器*/
	del_timer(&keydev.timer);
#endif

	/* 注销字符设备驱动 */
	cdev_del(&keydev.cdev);/*  删除cdev */
	unregister_chrdev_region(keydev.devid, KEY_CNT); /* 注销设备号 */

	device_destroy(keydev.class, keydev.devid);
	class_destroy(keydev.class);
}

module_init(mykey_init);
module_exit(mykey_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("denghengli");
