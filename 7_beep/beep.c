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
#include <asm/mach/map.h>

#define BEEPOFF 0
#define BEEPON  1

#define BEEP_NAME "beep"

struct beep_dev
{
    struct cdev cdev;   /*字符设备*/
    struct class *class;/*类*/
    struct device *device;/*设备*/

    dev_t devid;        /*设备号*/
    int major;          /*主设备号*/
    int minor;          /*次设备号*/

    struct device_node *nd; /*设备节点*/

    int beep_gpio; /*led所使用的GPIO编号*/
};

struct beep_dev beep;


static int beep_open(struct inode *inode, struct file *file)
{
    int ret = 0;

    file->private_data = &beep;

    /*1、找到节点 beep，路径是 /beep */
    beep.nd = of_find_node_by_path("/beep");
    if (beep.nd == NULL){
        printk("beep node can not found!\r\n");
        return -EINVAL;
    } else {
        printk("beep node has been found!\r\n");
    }
    
    /*2、获取设备树中的gpio属性，得到beep所使用的gpio编号*/
    beep.beep_gpio = of_get_named_gpio(beep.nd, "beep-gpio", 0);
    if (beep.beep_gpio < 0){
        printk("can't  get beep-gpio \r\n");
        return -EINVAL;
    }
    printk("beep-gpio num = %d \r\n", beep.beep_gpio);

    /*3、设置GPIO_IO03为输出，并且输出高电平，默认关闭gpio灯*/
    ret = gpio_direction_output(beep.beep_gpio, 1);
    if (ret < 0){
        printk("can't set gpio \r\n");
    }

    return ret;
}

static int beep_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t beep_read(struct file *file, char __user *buf, 
        size_t count, loff_t *offset)
{
    int ret = 0;

    return ret;
}

static ssize_t beep_write(struct file *file, const char __user *buf,
		size_t count, loff_t *offset)
{
    int ret = 0;
    uint8_t data[1], ledsta;
    struct beep_dev *dev = file->private_data;

    ret = copy_from_user(data, buf, count);
    if (ret < 0){
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }

    ledsta = data[0];
    if (ledsta == BEEPOFF){
        gpio_set_value(dev->beep_gpio, 1);
    } 
    else if (ledsta == BEEPON){
        gpio_set_value(dev->beep_gpio, 0);
    }
    else {
        printk("led para error\r\n");
    }
    
    return ret;
}

static const struct file_operations beep_fops = 
{
    .owner   = THIS_MODULE,
    .open    = beep_open,
    .release = beep_release,
    .read    = beep_read,
    .write   = beep_write,
};

static int __init beep_init(void)
{
    int ret = 0;

    /*1、创建设备号*/
    if (beep.major){  /*指定了设备号*/
        beep.devid = MKDEV(beep.major, 0);
        ret = register_chrdev_region(beep.devid, 1, BEEP_NAME);
    }
    else{   /*没有指定设备号*/
        ret = alloc_chrdev_region(&beep.devid, 0, 1, BEEP_NAME);
        beep.major = MAJOR(beep.devid);
        beep.minor = MINOR(beep.devid);
    }
    if (ret < 0){
        printk("beep chrdev_region failed \r\n");
        return -1;
    }
    printk("beep major = %d, minor = %d\r\n", beep.major, beep.minor);

    /*2、初始化字符设备cdev*/
    beep.cdev.owner = THIS_MODULE;
    cdev_init(&beep.cdev, &beep_fops);

    /*3、向linux内核添加一个字符设备*/
    cdev_add(&beep.cdev, beep.devid, 1);

    /*4、自动创建设备节点 -- 创建类*/
    beep.class = class_create(THIS_MODULE, BEEP_NAME);
    if (IS_ERR(beep.class)){
        return PTR_ERR(beep.class);
    }
    /*创建设备*/
    beep.device = device_create(beep.class, NULL, beep.devid, NULL, BEEP_NAME);
    if (IS_ERR(beep.device)){
        return PTR_ERR(beep.device);
    }

    return 0;
}

static void __exit beep_exit(void)
{
    /*删除字符设备*/
    cdev_del(&beep.cdev);

    /*注销设备号*/
    unregister_chrdev_region(beep.devid, 1);

    /*注销设备*/
    device_destroy(beep.class, beep.devid);
    /*注销类*/
    class_destroy(beep.class);

    printk("beep_exit\r\n");
}

/*注册和卸载驱动*/
module_init(beep_init);
module_exit(beep_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("denghengli");






