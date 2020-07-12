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
    fd_set select_readfds;
    struct timeval timeout;
    struct pollfd poll_readfds[1];//要监视的文件描述符和要监视的事件，是一个数组；一个描述符为一个元素

    if (argc != 2){
        printf("error usage!! Plese use : ./keyAPP  <filename> ");
        return -1;
    }

    /*open*/
    fd = open(filename, O_RDWR | O_NONBLOCK);
    if (fd < 0){
        printf("open %s failed!!\r\n", filename);
        return -1;
    }

    while(1)
    {
#if 0
        FD_ZERO(&select_readfds);//清零可读集合
        FD_SET(fd, &select_readfds);//将fd添加到可读集合
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;//500ms
        
        ret = select(fd+1, &select_readfds, NULL, NULL, &timeout);
        switch (ret)
        {
        case 0://超时
            printf("read timeout ! \r\n");
            break;

        case -1://错误
            printf("read error ! \r\n");
            break;

        default://可读
            if (FD_ISSET(fd, &select_readfds)){
                ret = read(fd, &keyvalue, sizeof(keyvalue));
                if (ret >= 0) {
                    printf("KEY0 Press, value = %#x \r\n", keyvalue);
                }
            }
            break;
        }
#else
        poll_readfds[0].fd = fd;
        poll_readfds[0].events = POLLIN;//监视可读事件
        ret = poll(&poll_readfds[0], 1, 500);//1为监视的文件描述符数量
        switch (ret)
        {
        case 0:
            printf("read timeout ! \r\n");
            break;
        
        case -1:
            printf("read error ! \r\n");
            break;

        default:
            ret = read(fd, &keyvalue, sizeof(keyvalue));
            if (ret >= 0) {
                printf("KEY0 Press, value = %#x \r\n", keyvalue);
            }
            break;
        }
#endif
    }

    /*close*/
    ret = close(fd);
    if (ret < 0){
        printf("close %s failed!!", filename);
        return -1;
    }

    return 0;
}