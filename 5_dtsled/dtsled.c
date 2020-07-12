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
#include <linux/errno.h> 
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/mach/map.h>

#define LEDOFF 0
#define LEDON  1

#define DTSLED_NAME "dts_led"

/*映射后的寄存器虚拟地址指针*/
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

struct dtsled_dev
{
    struct cdev cdev;   /*字符设备*/
    struct class *class;/*类*/
    struct device *device;/*设备*/

    dev_t devid;        /*设备号*/
    int major;          /*主设备号*/
    int minor;          /*次设备号*/

    struct device_node *nd; /*设备节点*/
};

struct dtsled_dev dtsled;

/*
 * @description		: LED打开/关闭
 * @param - sta 	: LEDON(0) 打开LED，LEDOFF(1) 关闭LED
 * @return 			: 无
 */
void dtsled_switch(u8 sta)
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

static int dtsled_open(struct inode *inode, struct file *file)
{
    int ret = 0, val;
    struct property *dtsled_prop = NULL;
    const char *str = NULL;
    uint32_t i, regdata[14];

    file->private_data = &dtsled;

    /*1、找到节点 alphaled，路径是 /alphaled */
    dtsled.nd = of_find_node_by_path("/alphaled");
    if (dtsled.nd == NULL){
        printk("alphaled node can not found!\r\n");
        return -EINVAL;
    } else {
        printk("alphaled node has been found!\r\n");
    }

    /*2、查找 兼容性属性*/
    dtsled_prop = of_find_property(dtsled.nd,"compatible",NULL);
    if (dtsled_prop == NULL){
        printk("compatible property find failed\r\n");
        return -EINVAL;
    } else {
        printk("compatible = %s\r\n", (char *)dtsled_prop->value);
    }
    /*3、查找 status 属性*/
    ret = of_property_read_string(dtsled.nd, "status", &str);
    if (ret < 0){
        printk("status read failed!\r\n");
        return -EINVAL;
    } else {
        printk("status = %s\r\n",str);
    }
    /*4、获取reg属性内容*/
    ret = of_property_read_u32_array(dtsled.nd, "reg", regdata, 10);
    if (ret < 0){
        printk("reg property read failed!\r\n");
    } else {
        printk("reg data:\r\n");
        for (i=0; i<10; i++){
            printk("%#X ", regdata[i]);
        }
        printk("\r\n");
    }

    /*初始化led，不能直接操作物理地址，需要地址重映射*/
    /*1、寄存器地址映射*/
#if 0
    IMX6U_CCM_CCGR1   = ioremap(regdata[0], regdata[1]);
    SW_MUX_GPIO1_IO03 = ioremap(regdata[2], regdata[3]);
    SW_PAD_GPIO1_IO03 = ioremap(regdata[4], regdata[5]);
    GPIO1_DR          = ioremap(regdata[6], regdata[7]);
    GPIO1_GDIR        = ioremap(regdata[8], regdata[9]);
#else
    IMX6U_CCM_CCGR1   = of_iomap(dtsled.nd, 0);
    SW_MUX_GPIO1_IO03 = of_iomap(dtsled.nd, 1);
    SW_PAD_GPIO1_IO03 = of_iomap(dtsled.nd, 2);
    GPIO1_DR          = of_iomap(dtsled.nd, 3);
    GPIO1_GDIR        = of_iomap(dtsled.nd, 4);
#endif

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

static int dtsled_release(struct inode *inode, struct file *file)
{
    /* 取消映射 */
	iounmap(IMX6U_CCM_CCGR1);
	iounmap(SW_MUX_GPIO1_IO03);
	iounmap(SW_PAD_GPIO1_IO03);
	iounmap(GPIO1_DR);
	iounmap(GPIO1_GDIR);

    return 0;
}

static ssize_t dtsled_read(struct file *file, char __user *buf, 
        size_t count, loff_t *offset)
{
    int ret = 0;

    return ret;
}

static ssize_t dtsled_write(struct file *file, const char __user *buf,
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
        dtsled_switch(ledsta);
    else
        printk("led para error\r\n");
    
    return ret;
}

static const struct file_operations dtsled_fops = 
{
    .owner   = THIS_MODULE,
    .open    = dtsled_open,
    .release = dtsled_release,
    .read    = dtsled_read,
    .write   = dtsled_write,
};

static int __init dts_led_init(void)
{
    int ret = 0;

    /*1、创建设备号*/
    if (dtsled.major){  /*指定了设备号*/
        dtsled.devid = MKDEV(dtsled.major, 0);
        ret = register_chrdev_region(dtsled.devid, 1, DTSLED_NAME);
    }
    else{   /*没有指定设备号*/
        ret = alloc_chrdev_region(&dtsled.devid, 0, 1, DTSLED_NAME);
        dtsled.major = MAJOR(dtsled.devid);
        dtsled.minor = MINOR(dtsled.devid);
    }
    if (ret < 0){
        printk("dtsled chrdev_region failed \r\n");
        return -1;
    }
    printk("dtsled major = %d, minor = %d\r\n", dtsled.major, dtsled.minor);

    /*2、初始化字符设备cdev*/
    dtsled.cdev.owner = THIS_MODULE;
    cdev_init(&dtsled.cdev, &dtsled_fops);

    /*3、向linux内核添加一个字符设备*/
    cdev_add(&dtsled.cdev, dtsled.devid, 1);

    /*4、自动创建设备节点 -- 创建类*/
    dtsled.class = class_create(THIS_MODULE, DTSLED_NAME);
    if (IS_ERR(dtsled.class)){
        return PTR_ERR(dtsled.class);
    }
    /*创建设备*/
    dtsled.device = device_create(dtsled.class, NULL, dtsled.devid, NULL, DTSLED_NAME);
    if (IS_ERR(dtsled.device)){
        return PTR_ERR(dtsled.device);
    }

    return 0;
}

static void __exit dts_led_exit(void)
{
    /*删除字符设备*/
    cdev_del(&dtsled.cdev);

    /*注销设备号*/
    unregister_chrdev_region(dtsled.devid, 1);

    /*注销设备*/
    device_destroy(dtsled.class, dtsled.devid);
    /*注销类*/
    class_destroy(dtsled.class);

    printk("dtsled_exit\r\n");
}

/*注册和卸载驱动*/
module_init(dts_led_init);
module_exit(dts_led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("denghengli");






