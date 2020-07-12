#include <linux/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <signal.h>


/* 定义按键值 */
#define KEY_VALUE		0X0F	/* 按键按下键值 */
#define KEY_INVAL		0X00	/* 无效的按键值 */

int fd = 0;

static void sigio_signal_func(int signum)
{
    int ret = 0;
    int keyvalue;

    ret = read(fd, &keyvalue, sizeof(keyvalue));
    if (ret < 0){ //读取失败

    } else {
        printf("receive sigio signal, key value = %#x\r\n", keyvalue);
    }
}

/*
*argc 应用程序参数个数
*argv 具体的参数内容,字符串形式
*/
int main(int argc, char *argv[]) 
{
    int ret = 0, flags = 0;
    char *filename = argv[1];

    if (argc != 2){
        printf("error usage!! Plese use : ./fasyncAPP  <filename> ");
        return -1;
    }

    /*open*/
    fd = open(filename, O_RDWR);
    if (fd < 0){
        printf("open %s failed!!\r\n", filename);
        return -1;
    }

    signal(SIGIO, sigio_signal_func);/*设置sigio的处理函数*/
    /*int fcntl (int __fd, int __cmd, ...);*/
    fcntl(fd, F_SETOWN, getpid());/*设置当前进程接收SIGIO信号*/
    flags = fcntl(fd, F_GETFL);/*获取当前文件标志*/
    fcntl(fd, F_SETFL, flags | FASYNC);/*设置当前文件标志，开启异步通知*/

    while(1)
    {
        sleep(2);
    }

    /*close*/
    ret = close(fd);
    if (ret < 0){
        printf("close %s failed!!", filename);
        return -1;
    }

    return 0;
}