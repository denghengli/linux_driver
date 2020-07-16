#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <linux/input.h>

/* 定义一个input_event变量，存放输入事件信息 */
static struct input_event inputevent;

/*
*argc 应用程序参数个数
*argv 具体的参数内容,字符串形式
*命令调用: ./led  <filename> <1:2> 1读2写
*./keyinputAPP  /dev/keyinput
*/
int main(int argc, char *argv[]) 
{
    int fd = 0;
    int err = 0, cnt = 0;
    char *filename = argv[1];
    int keyvalue;

    if (argc != 2){
        printf("error usage!! Plese use : ./keyinputAPP  <filename> ");
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
		err = read(fd, &inputevent, sizeof(inputevent));
		if (err > 0) { /* 读取数据成功 */
			switch (inputevent.type) {
				case EV_KEY:
					if (inputevent.code < BTN_MISC) { /* 键盘键值 */
						printf("key %d %s\r\n", inputevent.code, inputevent.value ? "press" : "release");
					} else {
						printf("button %d %s\r\n", inputevent.code, inputevent.value ? "press" : "release");
					}
					break;

				/* 其他类型的事件，自行处理 */
				case EV_REL:
					break;
				case EV_ABS:
					break;
				case EV_MSC:
					break;
				case EV_SW:
					break;
			}
		} else {
			printf("读取数据失败\r\n");
		}
    }

    /*close*/
    err = close(fd);
    if (err < 0){
        printf("close %s failed!!", filename);
        return -1;
    }

    return 0;
}