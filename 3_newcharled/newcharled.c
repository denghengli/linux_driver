#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/errno.h> 
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/mach/map.h>

#define NEWCHARLED_MAJOR 200
#define NEWCHARLED_NAME  "newcharled"

#define LEDOFF 0
#define LEDON  1

/*寄存器物理地址*/
#define CCM_CCGR1_BASE          (0X020C406C)
#define SW_MUX_GPIO1_IO03_BASE  (0X020E0068)
#define SW_PAD_GPIO1_IO03_BASE  (0X020E02F4)
#define GPIO1_DR_BASE           (0X0209C000)
#define GPIO1_GDIR_BASE         (0X0209C004)
/* 映射后的寄存器虚拟地址指针 */
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

struct newcharled_dev
{
    struct cdev cdev;   /*字符设备*/
    struct class *class;/*类*/
    struct device *device;/*设备*/

    dev_t devid;        /*设备号*/
    int major;          /*主设备号*/
    int minor;          /*次设备号*/
};

struct newcharled_dev   newcharled;

/*
 * @description		: LED打开/关闭
 * @param - sta 	: LEDON(0) 打开LED，LEDOFF(1) 关闭LED
 * @return 			: 无
 */
void newcharled_switch(u8 sta)
{
	u32 val = 0;
    
	if(sta == LEDON) 
    {
		val = readl(GPIO1_DR);
		val &= ~(1 << 3);	
		writel(val, GPIO1_DR);
	}
    else if(sta == LEDOFF) 
    {
		val = readl(GPIO1_DR);
		val|= (1 << 3);	
		writel(val, GPIO1_DR);
	}	
}

static int newcharled_open(struct inode *inode, struct file *file)
{
	u32 val = 0;

    file->private_data = &newcharled;

    /*初始化led，不能直接操作物理地址，需要地址重映射*/
    /*1、寄存器地址映射*/
    IMX6U_CCM_CCGR1   = ioremap(CCM_CCGR1_BASE, 4);
    SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
    SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
    GPIO1_DR          = ioremap(GPIO1_DR_BASE, 4);
    GPIO1_GDIR        = ioremap(GPIO1_GDIR_BASE, 4);

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

    return 0;
}

static int newcharled_release(struct inode *inode, struct file *file)
{
    /* 取消映射 */
	iounmap(IMX6U_CCM_CCGR1);
	iounmap(SW_MUX_GPIO1_IO03);
	iounmap(SW_PAD_GPIO1_IO03);
	iounmap(GPIO1_DR);
	iounmap(GPIO1_GDIR);

    return 0;
}

static ssize_t newcharled_read(struct file *file, char __user *buf, 
        size_t count, loff_t *offset)
{
    int ret = 0;

    return ret;
}

static ssize_t newcharled_write(struct file *file, const char __user *buf,
		size_t count, loff_t *offset)
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
        newcharled_switch(ledsta);
    else
        printk("led para error\r\n");
    
    return ret;
}

static const struct file_operations newcharled_fops = 
{
    .owner   = THIS_MODULE,
    .open    = newcharled_open,
    .release = newcharled_release,
    .read    = newcharled_read,
    .write   = newcharled_write,
};

static int __init newchar_led_init(void)
{
    int ret = 0;

    /*1、创建设备号*/
    if (newcharled.major){ /*指定了设备号*/
        newcharled.devid = MKDEV(newcharled.major, 0);
        ret = register_chrdev_region(newcharled.devid, 1, NEWCHARLED_NAME);
    }
    else{ /*没有指定设备号*/
        ret = alloc_chrdev_region(&newcharled.devid, 0, 1, NEWCHARLED_NAME);
        newcharled.major = MAJOR(newcharled.devid);
        newcharled.minor = MINOR(newcharled.devid);
    }

    if (ret < 0){
        printk("newcharled chrdev_region failed\r\n");
        return -1;
    }

    printk("newcharled major = %d, minor = %d\r\n", newcharled.major, newcharled.minor);
    
    /*2、初始化字符设备cdev*/
    newcharled.cdev.owner = THIS_MODULE;
    cdev_init(&newcharled.cdev, &newcharled_fops);

    /*3、向linux内核添加一个字符设备*/
    cdev_add(&newcharled.cdev, newcharled.devid, 1);

    /*4、自动创建设备节点 -- 创建类*/
    newcharled.class = class_create (THIS_MODULE, NEWCHARLED_NAME);
    if (IS_ERR(newcharled.class)){
        return PTR_ERR(newcharled.class);
    }
    /*-- 创建设备*/
    newcharled.device = device_create(newcharled.class, 
                                      NULL, 
                                      newcharled.devid, 
                                      NULL, 
                                      NEWCHARLED_NAME);
    if (IS_ERR(newcharled.device)){
        return PTR_ERR(newcharled.device);
    }

    return 0;
}

static void __exit newchar_led_exit(void)
{
    /*删除字符设备*/
    cdev_del(&newcharled.cdev);

    /*注销设备号*/
    unregister_chrdev_region(newcharled.devid, 1);

    /*注销设备*/
    device_destroy(newcharled.class, newcharled.devid);
    /*注销类*/
    class_destroy(newcharled.class);

    printk("newcharled_exit\r\n");
}

/*注册驱动和卸载驱动*/
module_init(newchar_led_init);
module_exit(newchar_led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("denghengli");