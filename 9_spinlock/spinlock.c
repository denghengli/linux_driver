#include <linux/types.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/errno.h> 
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/atomic.h>
#include <asm/mach/map.h>

#define LEDOFF 0
#define LEDON  1

#define GPIOLED_NAME "gpioled"

struct gpioled_dev
{
    struct cdev cdev;   /*字符设备*/
    struct class *class;/*类*/
    struct device *device;/*设备*/

    dev_t devid;        /*设备号*/
    int major;          /*主设备号*/
    int minor;          /*次设备号*/

    struct device_node *nd; /*设备节点*/
    int led_gpio; /*led所使用的GPIO编号*/

    int dev_status;/*0表示设备可以使用，大于0表示设备不可使用*/
    spinlock_t lock;
};

struct gpioled_dev gpioled;


static int gpioled_open(struct inode *inode, struct file *file)
{
    int ret = 0;

    file->private_data = &gpioled;
    
    spin_lock(&gpioled.lock);
    if (gpioled.dev_status){
        spin_unlock(&gpioled.lock);
        return -EBUSY;
    }

    gpioled.dev_status++;
    spin_unlock(&gpioled.lock);

    /*1、找到节点 gpioled，路径是 /gpioled */
    gpioled.nd = of_find_node_by_path("/gpioled");
    if (gpioled.nd == NULL){
        printk("gpioled node can not found!\r\n");
        return -EINVAL;
    } else {
        printk("gpioled node has been found!\r\n");
    }
    
    /*2、获取设备树中的gpio属性，得到LED所使用的LED编号*/
    gpioled.led_gpio = of_get_named_gpio(gpioled.nd, "led-gpio", 0);
    if (gpioled.led_gpio < 0){
        printk("can't  get led-gpio \r\n");
        return -EINVAL;
    }
    printk("led-gpio num = %d \r\n", gpioled.led_gpio);

    /*3、设置GPIO_IO03为输出，并且输出高电平，默认关闭LED灯*/
    ret = gpio_direction_output(gpioled.led_gpio, 1);
    if (ret < 0){
        printk("can't set gpio \r\n");
    }

    return ret;
}

static int gpioled_release(struct inode *inode, struct file *file)
{
    struct gpioled_dev *dev = file->private_data;

    spin_lock(&dev->lock);
    if (dev->dev_status){
        dev->dev_status--;
    }
    spin_unlock(&dev->lock);

    return 0;
}

static ssize_t gpioled_read(struct file *file, char __user *buf, 
        size_t count, loff_t *offset)
{
    int ret = 0;

    return ret;
}

static ssize_t gpioled_write(struct file *file, const char __user *buf,
		size_t count, loff_t *offset)
{
    int ret = 0;
    uint8_t data[1], ledsta;
    struct gpioled_dev *dev = file->private_data;

    ret = copy_from_user(data, buf, count);
    if (ret < 0){
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }

    ledsta = data[0];
    if (ledsta == LEDOFF){
        gpio_set_value(dev->led_gpio, 1);
    } 
    else if (ledsta == LEDON){
        gpio_set_value(dev->led_gpio, 0);
    }
    else {
        printk("led para error\r\n");
    }
    
    return ret;
}

static const struct file_operations gpioled_fops = 
{
    .owner   = THIS_MODULE,
    .open    = gpioled_open,
    .release = gpioled_release,
    .read    = gpioled_read,
    .write   = gpioled_write,
};

static int __init gpio_led_init(void)
{
    int ret = 0;

    /*1、创建设备号*/
    if (gpioled.major){  /*指定了设备号*/
        gpioled.devid = MKDEV(gpioled.major, 0);
        ret = register_chrdev_region(gpioled.devid, 1, GPIOLED_NAME);
    }
    else{   /*没有指定设备号*/
        ret = alloc_chrdev_region(&gpioled.devid, 0, 1, GPIOLED_NAME);
        gpioled.major = MAJOR(gpioled.devid);
        gpioled.minor = MINOR(gpioled.devid);
    }
    if (ret < 0){
        printk("gpioled chrdev_region failed \r\n");
        return -1;
    }
    printk("gpioled major = %d, minor = %d\r\n", gpioled.major, gpioled.minor);

    /*2、初始化字符设备cdev*/
    gpioled.cdev.owner = THIS_MODULE;
    cdev_init(&gpioled.cdev, &gpioled_fops);

    /*3、向linux内核添加一个字符设备*/
    cdev_add(&gpioled.cdev, gpioled.devid, 1);

    /*4、自动创建设备节点 -- 创建类*/
    gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
    if (IS_ERR(gpioled.class)){
        return PTR_ERR(gpioled.class);
    }
    /*创建设备*/
    gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, GPIOLED_NAME);
    if (IS_ERR(gpioled.device)){
        return PTR_ERR(gpioled.device);
    }

    /*初始化自旋锁*/
    spin_lock_init(&gpioled.lock);

    return 0;
}

static void __exit gpio_led_exit(void)
{
    /*删除字符设备*/
    cdev_del(&gpioled.cdev);

    /*注销设备号*/
    unregister_chrdev_region(gpioled.devid, 1);

    /*注销设备*/
    device_destroy(gpioled.class, gpioled.devid);
    /*注销类*/
    class_destroy(gpioled.class);

    printk("gpioled_exit\r\n");
}

/*注册和卸载驱动*/
module_init(gpio_led_init);
module_exit(gpio_led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("denghengli");






