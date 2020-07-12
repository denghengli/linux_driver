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
#include <linux/timer.h>

#define LEDOFF 0
#define LEDON  1
#define CMD_OPEN  (_IO(0xEF, 0X01))
#define CMD_CLOSE (_IO(0xEF, 0X02))
#define CMD_SET   (_IO(0xEF, 0X03))

#define TIMER_NAME "timer"

struct timer_dev
{
    struct cdev cdev;   /*字符设备*/
    struct class *class;/*类*/
    struct device *device;/*设备*/

    dev_t devid;        /*设备号*/
    int major;          /*主设备号*/
    int minor;          /*次设备号*/

    struct device_node *nd; /*设备节点*/
    int led_gpio; /*led所使用的GPIO编号*/

    int period;
    struct timer_list timer;
};

struct timer_dev timerdev;

static int led_init(void)
{
    int ret = 0;

    /*1、找到节点 gpioled，路径是 /gpioled */
    timerdev.nd = of_find_node_by_path("/gpioled");
    if (timerdev.nd == NULL){
        printk("timer node can not found!\r\n");
        return -EINVAL;
    } else {
        printk("timer node has been found!\r\n");
    }
    
    /*2、获取设备树中的gpio属性，得到LED所使用的LED编号*/
    timerdev.led_gpio = of_get_named_gpio(timerdev.nd, "led-gpio", 0);
    if (timerdev.led_gpio < 0){
        printk("can't  get led-gpio \r\n");
        return -EINVAL;
    }
    printk("led-gpio num = %d \r\n", timerdev.led_gpio);

    /*3、设置GPIO_IO03为输出，并且输出高电平，默认关闭LED灯*/
    gpio_request(timerdev.led_gpio, "led");
    ret = gpio_direction_output(timerdev.led_gpio, 1);
    if (ret < 0){
        printk("can't set gpio \r\n");
        return -EINVAL;
    }

    return ret;
}

static int timer_open(struct inode *inode, struct file *file)
{
    int ret = 0;

    file->private_data = &timerdev;

    timerdev.period = 1000; //1000ms
    ret = led_init();

    return ret;
}

static int timer_release(struct inode *inode, struct file *file)
{
    //struct timer_dev *dev = file->private_data;

    return 0;
}

static ssize_t timer_read(struct file *file, char __user *buf, 
        size_t count, loff_t *offset)
{
    int ret = 0;

    return ret;
}

static ssize_t timer_write(struct file *file, const char __user *buf,
		size_t count, loff_t *offset)
{
    int ret = 0;
    uint8_t data[1], ledsta;
    struct timer_dev *dev = file->private_data;

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

static long timer_unlocked_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{
    struct timer_dev *dev = (struct timer_dev *)file->private_data;
    int value = 0;
    int ret = 0;

    switch (cmd)
    {
    case CMD_CLOSE:
        del_timer_sync(&dev->timer);
        break;

    case CMD_OPEN:
        mod_timer(&dev->timer, jiffies + msecs_to_jiffies(dev->period));
        break;

    case CMD_SET:
        //ret = copy_from_user(&value, (int *)arg, sizeof(int));
        dev->period = arg;
        mod_timer(&dev->timer, jiffies + msecs_to_jiffies(arg));
        break;

    default:
        break;
    }
}

static const struct file_operations timer_fops = 
{
    .owner   = THIS_MODULE,
    .open    = timer_open,
    .release = timer_release,
    .read    = timer_read,
    .write   = timer_write,
    .unlocked_ioctl = timer_unlocked_ioctl,
};

/*定时器回调函数*/
void timer_fun(unsigned long arg)
{
    struct timer_dev *dev = (struct timer_dev*)arg;
    static char sta = 1;
    //int period;

    /*设置led输出*/
    sta = !sta;
    gpio_set_value(dev->led_gpio, sta);

    /*重启定时器*/
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(timerdev.period));
}

static int __init timer_init(void)
{
    int ret = 0;

    /*1、创建设备号*/
    if (timerdev.major){  /*指定了设备号*/
        timerdev.devid = MKDEV(timerdev.major, 0);
        ret = register_chrdev_region(timerdev.devid, 1, TIMER_NAME);
    } else {   /*没有指定设备号*/
        ret = alloc_chrdev_region(&timerdev.devid, 0, 1, TIMER_NAME);
        timerdev.major = MAJOR(timerdev.devid);
        timerdev.minor = MINOR(timerdev.devid);
    }
    if (ret < 0){
        printk("timer chrdev_region failed \r\n");
        return -1;
    }
    printk("timer major = %d, minor = %d\r\n", timerdev.major, timerdev.minor);

    /*2、初始化字符设备cdev*/
    timerdev.cdev.owner = THIS_MODULE;
    cdev_init(&timerdev.cdev, &timer_fops);

    /*3、向linux内核添加一个字符设备*/
    cdev_add(&timerdev.cdev, timerdev.devid, 1);

    /*4、自动创建设备节点 -- 创建类*/
    timerdev.class = class_create(THIS_MODULE, TIMER_NAME);
    if (IS_ERR(timerdev.class)){
        return PTR_ERR(timerdev.class);
    }
    /*创建设备*/
    timerdev.device = device_create(timerdev.class, NULL, timerdev.devid, NULL, TIMER_NAME);
    if (IS_ERR(timerdev.device)){
        return PTR_ERR(timerdev.device);
    }

    /*5、初始化timer*/
    init_timer(&timerdev.timer);
    timerdev.timer.function = timer_fun;
    timerdev.timer.data = (unsigned long)&timerdev;

    return 0;
}

static void __exit timer_exit(void)
{
    gpio_set_value(timerdev.led_gpio, 1);/*卸载前关闭led*/
    del_timer_sync(&timerdev.timer);/*删除timer*/

    /*删除字符设备*/
    cdev_del(&timerdev.cdev);

    /*注销设备号*/
    unregister_chrdev_region(timerdev.devid, 1);

    /*注销设备*/
    device_destroy(timerdev.class, timerdev.devid);
    /*注销类*/
    class_destroy(timerdev.class);

    printk("timer_exit\r\n");
}

/*注册和卸载驱动*/
module_init(timer_init);
module_exit(timer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("denghengli");






