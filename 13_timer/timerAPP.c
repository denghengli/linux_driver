#include <linux/types.h>
#include <sys/types.h>
#include <sys/stat.h>
/*
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/ioctl.h>*/

#include "stdio.h" 
#include "unistd.h"
#include "sys/types.h" 
#include "sys/stat.h" 
#include "fcntl.h" 
#include "stdlib.h"
#include "string.h" 
#include "linux/ioctl.h"

#define CMD_OPEN  (_IO(0xEF, 0X01))
#define CMD_CLOSE (_IO(0xEF, 0X02))
#define CMD_SET   (_IO(0xEF, 0X03))

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
    unsigned char keyvalue, str[100];
    unsigned int cmd, arg;

    if (argc != 2){
        printf("error usage!! Plese use : ./timerAPP  <driverfilename> ");
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
        printf("Input CMD:");
        ret = scanf("%d", &cmd);
        if (ret != 1){
            //gets(str);
        }

        if (cmd == 1) cmd = CMD_OPEN;
        else if (cmd == 2) cmd = CMD_CLOSE;
        else if (cmd == 3) {
            cmd = CMD_SET;
            printf("Input timer period:");
            ret = scanf("%d", &arg);
            if (ret != 1){
                //gets(str);
            }
        }

        ioctl(fd, cmd, arg);
    }

    /*close*/
    ret = close(fd);
    if (ret < 0){
        printf("close %s failed!!", filename);
        return -1;
    }

    return 0;
}