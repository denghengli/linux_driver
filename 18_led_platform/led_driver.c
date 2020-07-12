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
#define LED_NAME  "platformled"

#define LEDOFF 0
#define LEDON  1

/* 映射后的寄存器虚拟地址指针 */
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

struct led_driver
{
    struct cdev cdev;   /*字符设备*/
    struct class *class;/*类*/
    struct device *device;/*设备*/

    dev_t devid;        /*设备号*/
    int major;          /*主设备号*/
    int minor;          /*次设备号*/
};

struct led_driver   leddriver;

/*
 * @description		: LED打开/关闭
 * @param - sta 	: LEDON(0) 打开LED，LEDOFF(1) 关闭LED
 * @return 			: 无
 */
void leddriver_switch(u8 sta)
{
	u32 val = 0;
    
	if(sta == LEDON) {
		val = readl(GPIO1_DR);
		val &= ~(1 << 3);	
		writel(val, GPIO1_DR);
	}
    else if(sta == LEDOFF) {
		val = readl(GPIO1_DR);
		val|= (1 << 3);	
		writel(val, GPIO1_DR);
	}	
}

static int leddriver_open(struct inode *inode, struct file *file)
{
    file->private_data = &leddriver;

    return 0;
}

static ssize_t leddriver_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    int ret = 0;
    u8 data[1], ledsta;

    ret = copy_from_user(data, buf, count);
    if (ret < 0){
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }

    ledsta = data[0];
    if (ledsta == LEDOFF || ledsta == LEDON)
        leddriver_switch(ledsta);
    else
        printk("led para error\r\n");
    
    return ret;
}

static const struct file_operations leddriver_fops = 
{
    .owner   = THIS_MODULE,
    .open    = leddriver_open,
    .write   = leddriver_write,
};

static void led_init(void)
{
    int val = 0;

    /* 2、使能GPIO1时钟 */
	val = readl(IMX6U_CCM_CCGR1);
	val &= ~(3 << 26);	/* 清楚以前的设置 */
	val |= (3 << 26);	/* 设置新值 */
	writel(val, IMX6U_CCM_CCGR1);

	/* 3、设置GPIO1_IO03的复用功能，将其复用为
	 *    GPIO1_IO03，最后设置IO属性。
	 */
	writel(5, SW_MUX_GPIO1_IO03);
	
	/*寄存器SW_PAD_GPIO1_IO03设置IO属性
	 *bit 16:0 HYS关闭
	 *bit [15:14]: 00 默认下拉
     *bit [13]: 0 kepper功能
     *bit [12]: 1 pull/keeper使能
     *bit [11]: 0 关闭开路输出
     *bit [7:6]: 10 速度100Mhz
     *bit [5:3]: 110 R0/6驱动能力
     *bit [0]: 0 低转换率
	 */
	writel(0x10B0, SW_PAD_GPIO1_IO03);

	/* 4、设置GPIO1_IO03为输出功能 */
	val = readl(GPIO1_GDIR);
	val &= ~(1 << 3);	/* 清除以前的设置 */
	val |= (1 << 3);	/* 设置为输出 */
	writel(val, GPIO1_GDIR);

	/* 5、默认关闭LED */
	val = readl(GPIO1_DR);
	val |= (1 << 3);	
	writel(val, GPIO1_DR);
}

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
    int i = 0;
    struct resource *led_resources[5];

    printk("led driver probe \r\n");

    /*从设备中获取资源*/
    for ( i = 0; i < 5; i++)
    {
        led_resources[i] = platform_get_resource(dev, IORESOURCE_MEM, i);
        if (led_resources[i] == NULL){
            return -EINVAL;
        }
    }

    /*内存映射*/
    IMX6U_CCM_CCGR1   = ioremap(led_resources[0]->start, resource_size(led_resources[0]));
    SW_MUX_GPIO1_IO03 = ioremap(led_resources[1]->start, resource_size(led_resources[1]));
    SW_PAD_GPIO1_IO03 = ioremap(led_resources[2]->start, resource_size(led_resources[2]));
    GPIO1_DR          = ioremap(led_resources[3]->start, resource_size(led_resources[3]));
    GPIO1_GDIR        = ioremap(led_resources[4]->start, resource_size(led_resources[4]));

    /*初始化led*/
    led_init();

    /*创建字符设备,会在 /dev目下创建 leddriver 的字符设备,这样应用程序才能使用leddriver这个设备*/
    return create_chrdev();
}

static int led_remove(struct platform_device *dev)
{
    printk("led driver remove \r\n");

    /* 取消映射 */
	iounmap(IMX6U_CCM_CCGR1);
	iounmap(SW_MUX_GPIO1_IO03);
	iounmap(SW_PAD_GPIO1_IO03);
	iounmap(GPIO1_DR);
	iounmap(GPIO1_GDIR);

    cdev_del(&leddriver.cdev);/*删除字符设备*/
    unregister_chrdev_region(leddriver.devid, 1);/*注销设备号*/
    device_destroy(leddriver.class, leddriver.devid);/*注销设备*/
    class_destroy(leddriver.class); /*注销类*/

    return 0;
}



static struct platform_driver led_driver = 
{
    .probe		= led_probe,
    .remove		= led_remove,
    .driver		= {
        .name	= "imx6ull-led", //驱动名字，用于与设备名字匹配，匹配成功之后 probe 就会执行
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
