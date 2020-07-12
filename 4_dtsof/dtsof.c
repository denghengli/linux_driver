#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/errno.h> 
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/mach/map.h>

#if 0
	backlight {
		compatible = "pwm-backlight";
		pwms = <&pwm1 0 5000000>;
		brightness-levels = <0 4 8 16 32 64 128 255>;
		default-brightness-level = <7>;
		status = "okay";
	};
#endif

static int __init dtsof_init(void)
{
    int ret = 0, elem_size = 0;
    struct device_node *bl_nd = NULL;
    struct property *bl_property = NULL;
    const char *str = NULL;
    uint32_t def_value, i;
    uint32_t *brival = NULL;

    /*1、找到节点 backlight，路径是 /backlight */
    bl_nd = of_find_node_by_path("/backlight");
    if (bl_nd == NULL){
        ret = -EINVAL;
        goto fail_findnd;
    }

    /*2、查找 兼容性属性*/
    bl_property = of_find_property(bl_nd,"compatible",NULL);
    if (bl_property == NULL){
        ret = -EINVAL;
        goto fail_compatible;
    }else{
        printk("compatible = %s\r\n", (char *)bl_property->value);
    }
    /*读取字符串属性*/
    ret = of_property_read_string(bl_nd,"status",&str);
    if (ret < 0){
        goto fail_rstr;
    }
    else{
        printk("status = %s\r\n", str);
    }
    /*读取数值型属性的元素*/
    ret = of_property_read_u32(bl_nd,"default-brightness-level",&def_value);
    if (ret < 0){
        goto fail_ru32;
    }
    else{
        printk("default-brightness-level = %d\r\n", def_value);
    }
    /*获取数组型属性的元素*/
    elem_size = of_property_count_elems_of_size(bl_nd,"brightness-levels",sizeof(uint32_t));
    if (elem_size < 0){
        ret = -EINVAL;
        goto fail_relem;
    }
    else{
        printk("brightness-levels elem size = %d\r\n", elem_size);
        brival = kmalloc(elem_size * sizeof(uint32_t), GFP_KERNEL);//申请内存
        if (!brival){
            ret = -EINVAL;
            goto nomem;
        }
        /*读取数组*/
        ret = of_property_read_u32_array(bl_nd,"brightness-levels",brival,elem_size);
        if (ret < 0){
            goto fail_ru32arry;
        }
        else{
            for (i = 0; i < elem_size; i++){
                printk("brightness-levels[%d] = %d\r\n", i, *(brival + i));
            }
        }
    }

    return 0;

fail_ru32arry:
    kfree(brival);
nomem:
fail_relem:
fail_ru32:
fail_rstr:
fail_compatible:
fail_findnd:
    return ret;
}


static void __exit dtsof_exit(void)
{

}

/*注册驱动和卸载驱动*/
module_init(dtsof_init);
module_exit(dtsof_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("denghengli");