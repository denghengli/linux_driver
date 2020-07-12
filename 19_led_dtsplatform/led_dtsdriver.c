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
#include <linux/fcntl.h>
#include <linux/platform_device.h>

#define LED_MAJOR 200
#define LED_NAME  "dtsplatformled"

#define LEDOFF 0
#define LEDON  1

struct led_driver
{
    struct cdev cdev;   /*字符设备*/
    struct class *class;/*类*/
    struct device *device;/*设备*/

    dev_t devid;        /*设备号*/
    int major;          /*主设备号*/
    int minor;          /*次设备号*/
    struct device_node *nd; /*设备节点*/
    int led_gpio; /*led所使用的GPIO编号*/
};

struct led_driver   leddriver;

static int leddriver_open(struct inode *inode, struct file *file)
{
    file->private_data = &leddriver;

    return 0;
}

static ssize_t leddriver_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    int ret = 0;
    uint8_t data[1], ledsta;
    struct led_driver *drv = file->private_data;

    ret = copy_from_user(data, buf, count);
    if (ret < 0){
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }

    ledsta = data[0];
    if (ledsta == LEDOFF){
        gpio_set_value(drv->led_gpio, 1);
    } 
    else if (ledsta == LEDON){
        gpio_set_value(drv->led_gpio, 0);
    }
    else {
        printk("led para error\r\n");
    }
    
    return ret;
}

static const struct file_operations leddriver_fops = 
{
    .owner   = THIS_MODULE,
    .open    = leddriver_open,
    .write   = leddriver_write,
};

static int create_chrdev(void)
{
    int ret = 0;

   /*1、创建设备号*/
    if (leddriver.major){ /*指定了设备号*/
        leddriver.devid = MKDEV(leddriver.major, 0);
        ret = register_chrdev_region(leddriver.devid, 1, LED_NAME);
    }
    else{ /*没有指定设备号*/
        ret = alloc_chrdev_region(&leddriver.devid, 0, 1, LED_NAME);
        leddriver.major = MAJOR(leddriver.devid);
        leddriver.minor = MINOR(leddriver.devid);
    }

    if (ret < 0){
        printk("leddriver chrdev_region failed\r\n");
        return -1;
    }

    printk("leddriver major = %d, minor = %d\r\n", leddriver.major, leddriver.minor);
    
    /*2、初始化字符设备cdev*/
    leddriver.cdev.owner = THIS_MODULE;
    cdev_init(&leddriver.cdev, &leddriver_fops);

    /*3、向linux内核添加一个字符设备*/
    cdev_add(&leddriver.cdev, leddriver.devid, 1);

    /*4、自动创建设备节点 -- 创建类*/
    leddriver.class = class_create (THIS_MODULE, LED_NAME);
    if (IS_ERR(leddriver.class)){
        return PTR_ERR(leddriver.class);
    }
    /*-- 创建设备*/
    leddriver.device = device_create(leddriver.class, NULL, leddriver.devid, NULL, LED_NAME);
    if (IS_ERR(leddriver.device)){
        return PTR_ERR(leddriver.device);
    }

    return ret;
}

static int led_probe(struct platform_device *dev)
{
    int ret;

    printk("led driver probe \r\n");

    /*1、找到节点 gpioled，路径是 /gpioled */
    leddriver.nd = of_find_node_by_path("/gpioled");
    if (leddriver.nd == NULL){
        printk("gpioled node can not found!\r\n");
        return -EINVAL;
    } else {
        printk("gpioled node has been found!\r\n");
    }
    
    /*2、获取设备树中的gpio属性，得到LED所使用的gpio编号*/
    leddriver.led_gpio = of_get_named_gpio(leddriver.nd, "led-gpio", 0);
    if (leddriver.led_gpio < 0){
        printk("can't  get led-gpio \r\n");
        return -EINVAL;
    }
    printk("led-gpio num = %d \r\n", leddriver.led_gpio);

    /*3、设置GPIO_IO03为输出，并且输出高电平，默认关闭LED灯*/
    ret = gpio_direction_output(leddriver.led_gpio, 1);
    if (ret < 0){
        printk("can't set gpio \r\n");
    }

    /*创建字符设备,会在 /dev目下创建 leddriver 的字符设备,这样应用程序才能使用leddriver这个设备*/
    return create_chrdev();
}

static int led_remove(struct platform_device *dev)
{
    printk("led driver remove \r\n");

    cdev_del(&leddriver.cdev);/*删除字符设备*/
    unregister_chrdev_region(leddriver.devid, 1);/*注销设备号*/
    device_destroy(leddriver.class, leddriver.devid);/*注销设备*/
    class_destroy(leddriver.class); /*注销类*/

    return 0;
}

/*设备树匹配表*/
static struct of_device_id led_of_match[] = {
    {.compatible = "alientek-gpioled"},//兼容性属性可以有多个
    {/*sentinel*/},//结尾必须这样
};

static struct platform_driver led_driver = 
{
    .probe		= led_probe,
    .remove		= led_remove,
    .driver		= {
        .name	= "imx6ull-led", //驱动名字，无设备树下 用于与设备名字匹配，匹配成功之后 probe 就会执行
        .of_match_table = led_of_match,//匹配表，有设备树下，用于与设备树中的节点进行匹配，匹配成功之后 probe 就会执行
    },
};

/*驱动模块加载*/
static int __init led_driver_init(void)
{
    return platform_driver_register(&led_driver);/*注册platform驱动*/
}

/*驱动模块卸载*/
static void __exit led_driver_exit(void)
{
    platform_driver_unregister(&led_driver);
}

module_init(led_driver_init);
module_exit(led_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("denghengli");
