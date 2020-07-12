#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#define LED_MAJOR 200
#define LED_NAME  "led"

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


/*
 * @description		: LED打开/关闭
 * @param - sta 	: LEDON(0) 打开LED，LEDOFF(1) 关闭LED
 * @return 			: 无
 */
void led_switch(u8 sta)
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

static int led_open(struct inode *inode, struct file *file)
{
	u32 val = 0;

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

static int led_release(struct inode *inode, struct file *file)
{
    /* 取消映射 */
	iounmap(IMX6U_CCM_CCGR1);
	iounmap(SW_MUX_GPIO1_IO03);
	iounmap(SW_PAD_GPIO1_IO03);
	iounmap(GPIO1_DR);
	iounmap(GPIO1_GDIR);

    return 0;
}

static ssize_t led_read(struct file *file, char __user *buf, 
        size_t count, loff_t *offset)
{
    int ret = 0;

    return ret;
}

static ssize_t led_write(struct file *file, const char __user *buf,
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
        led_switch(ledsta);
    else
        printk("led para error\r\n");
    
    return ret;
}

static const struct file_operations led_fops = 
{
    .owner   = THIS_MODULE,
    .open    = led_open,
    .release = led_release,
    .read    = led_read,
    .write   = led_write,
};

static int __init led_init(void)
{
    int ret = 0;

    /*注册字符设备*/
    ret = register_chrdev(LED_MAJOR, LED_NAME, &led_fops);
    if (ret < 0){
        printk("led register failed\r\n");
    }

    printk("led_init\r\n");
    return 0;
}

static void __exit led_exit(void)
{
    unregister_chrdev(LED_MAJOR, LED_NAME);

    printk("led_exit\r\n");
}

/*注册驱动和卸载驱动*/
module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("denghengli");