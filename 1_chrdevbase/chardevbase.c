#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/ide.h>

#define CHARDEVBASE_MAJOR 200 /*主设备号*/
#define CHARDEVBASE_NAME  "chardevbase"

static char readbuf[100] ="kernel to user data";
static char writebuf[100];

static int chardevbase_open(struct inode *inode, struct file *file)
{
    //printk("chardevbase_open\r\n");
    return 0;
}

static int chardevbase_release(struct inode *inode, struct file *file)
{
    //printk("chardevbase_release\r\n");
    return 0;
}

static ssize_t chardevbase_read(struct file *file, char __user *buf, 
        size_t count, loff_t *offset)
{
    int ret = 0;

    ret = copy_to_user(buf, readbuf, count);
    if (ret < 0){
        printk("kernel chardevbase read fail\r\n");
    }
    return ret;
}

static ssize_t chardevbase_write(struct file *file, const char __user *buf,
		size_t count, loff_t *offset)
{
    int ret = 0;

    ret = copy_from_user(writebuf, buf, count);
    if (ret < 0){
        printk("kernel chardevbase write fail\r\n");
    }
    else{
        printk("kernel receive data is : %s \r\n", writebuf);
    }

    return ret;
}

/*
 字符设备操作集合
*/
static struct file_operations chardevbase_fops = {
    .owner = THIS_MODULE,
    .open = chardevbase_open,
    .release = chardevbase_release,
    .read = chardevbase_read,
    .write = chardevbase_write,
};



/*
    模块输入与输出
 */
static int __init chardevbase_init(void)
{
    int ret = 0;
    printk("chardevbase_init\r\n");

    /*注册字符设备*/
    ret = register_chrdev(CHARDEVBASE_MAJOR, CHARDEVBASE_NAME, &chardevbase_fops);
    if (ret < 0){
        printk("chardevbase init failed!\r\n");
    }
    return 0;
}

static void __exit chardevbase_exit(void)
{
    printk("chardevbase_exit\r\n");

    /*卸载字符设备*/
    unregister_chrdev(CHARDEVBASE_MAJOR, CHARDEVBASE_NAME);
}

module_init(chardevbase_init); /*入口*/
module_exit(chardevbase_exit); /*出口*/

MODULE_AUTHOR("denghengli");
MODULE_LICENSE("GPL");