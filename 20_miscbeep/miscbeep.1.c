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
#include <linux/miscdevice.h>

#define BEEP_MINOR 144
#define BEEP_NAME  "miscbeep"

#define BEEPOFF 0
#define BEEPON  1

struct miscbeep_driver
{
    struct cdev cdev;   /*字符设备*/
    struct class *class;/*类*/
    struct device *device;/*设备*/

    dev_t devid;        /*设备号*/
    struct device_node *nd; /*设备节点*/
    int beep_gpio; /*led所使用的GPIO编号*/
};

struct miscbeep_driver   miscbeep;

static int miscbeep_open(struct inode *inode, struct file *file)
{
    file->private_data = &miscbeep;

    return 0;
}

static ssize_t miscbeep_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    int ret = 0;
    uint8_t data[1], ledsta;
    struct miscbeep_driver *drv = file->private_data;

    ret = copy_from_user(data, buf, count);
    if (ret < 0){
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }

    ledsta = data[0];
    if (ledsta == BEEPOFF){
        gpio_set_value(drv->beep_gpio, 1);
    } 
    else if (ledsta == BEEPON){
        gpio_set_value(drv->beep_gpio, 0);
    }
    else {
        printk("led para error\r\n");
    }
    
    return ret;
}

/*设备操作集合*/
static const struct file_operations miscbeep_fops = 
{
    .owner   = THIS_MODULE,
    .open    = miscbeep_open,
    .write   = miscbeep_write,
};


/*MISC设备结构体*/
static struct miscdevice beep_miscdev = {
    .minor = BEEP_MINOR,    /*子设备*/
    .name = BEEP_NAME,      /*设备名称*/
    .fops = &miscbeep_fops, /*设备操作集合*/
};

static int beep_probe(struct platform_device *dev)
{
    int ret;

    printk("beep driver probe \r\n");

#if 0
    /*1、找到节点 beep，路径是 /beep */
    miscbeep.nd = of_find_node_by_path("/beep");
    if (miscbeep.nd == NULL){
        printk("beep node can not found!\r\n");
        return -EINVAL;
    } else {
        printk("beep node has been found!\r\n");
    }
#endif

    /*1、从platform_device获取设备树节点.
     驱动与设备匹配成功以后，设备信息就会从设备树节点转为platform_device结构体*/
    miscbeep.nd = dev->dev.of_node;
    if (miscbeep.nd == NULL){
        printk("beep node not find ! \r\n");
        return -EINVAL;
    }

    /*2、获取设备树中的gpio属性，得到LED所使用的gpio编号*/
    miscbeep.beep_gpio = of_get_named_gpio(miscbeep.nd, "beep-gpio", 0);
    if (miscbeep.beep_gpio < 0){
        printk("can't  get beep-gpio \r\n");
        return -EINVAL;
    }
    printk("beep-gpio num = %d \r\n", miscbeep.beep_gpio);

    /*3、设置GPIO_IO03为输出，并且输出高电平，默认关闭LED灯*/
    ret = gpio_direction_output(miscbeep.beep_gpio, 1);
    if (ret < 0){
        printk("can't set gpio \r\n");
        return -EINVAL;
    }

    /*一般情况下，还需要注册对应的字符设备，但是这里我们使用 MISC 设备，所以就不再需要自己注册
     *字符设备驱动类，只需要注册misc设备驱动即可
     */
    ret = misc_register(&beep_miscdev);
    if (ret < 0){
        printk("misc device register failed! \r\n");
        return -EFAULT;
    }

    return 0;
}

static int beep_remove(struct platform_device *dev)
{
    printk("beep driver remove \r\n");

    misc_deregister(&beep_miscdev);//注销misc设备驱动

    return 0;
}

/*设备树匹配表*/
static struct of_device_id beep_of_match[] = {
    {.compatible = "alientek-beep"},//兼容性属性可以有多个
    {/*sentinel*/},//结尾必须这样
};

static struct platform_driver miscbeep_driver = 
{
    .probe		= beep_probe,
    .remove		= beep_remove,
    .driver		= {
        .name	= "imx6ull-beep", //驱动名字，无设备树下 用于与设备名字匹配，匹配成功之后 probe 就会执行
        .of_match_table = beep_of_match,//匹配表，有设备树下，用于与设备树中的节点进行匹配，匹配成功之后 probe 就会执行
    },
};

/*驱动模块加载*/
static int __init miscbeep_driver_init(void)
{
    return platform_driver_register(&miscbeep_driver);/*注册platform驱动*/
}

/*驱动模块卸载*/
static void __exit miscbeep_driver_exit(void)
{
    platform_driver_unregister(&miscbeep_driver);
}

module_init(miscbeep_driver_init);
module_exit(miscbeep_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("denghengli");
