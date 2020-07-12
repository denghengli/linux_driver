#include <linux/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 定义按键值 */
#define KEY_VALUE		0X0F	/* 按键按下键值 */
#define KEY_INVAL		0X00	/* 无效的按键值 */

/*
*argc 应用程序参数个数
*argv 具体的参数内容,字符串形式
*命令调用: ./led  <filename> <1:2> 1读2写
*./led  /dev/led  1 
*./led  /dev/led  2
*/
int main(int argc, char *argv[]) 
{
    int fd = 0;
    int ret = 0, cnt = 0;
    char *filename = argv[1];
    int keyvalue;

    if (argc != 2){
        printf("error usage!! Plese use : ./keyAPP  <filename> ");
        return -1;
    }

    /*open*/
    fd = open(filename, O_RDWR);
    if (fd < 0){
        printf("open %s failed!!\r\n", filename);
        return -1;
    }

    while(1)
    {
        ret = read(fd, &keyvalue, sizeof(keyvalue));
        if (ret >= 0) {
            printf("KEY0 Press, value = %#x \r\n", keyvalue);
        }
    }

    /*close*/
    ret = close(fd);
    if (ret < 0){
        printf("close %s failed!!", filename);
        return -1;
    }

    return 0;
}