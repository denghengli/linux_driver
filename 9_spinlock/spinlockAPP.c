#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    char data[1];

    if (argc != 3){
        printf("error usage!! Plese use : ./led  <filename> <1:2>");
        return -1;
    }

    /*open*/
    fd = open(filename, O_RDWR);
    if (fd < 0){
        printf("open %s failed!!\r\n", filename);
        return -1;
    }

    /*write*/
    data[0] = atoi(argv[2]);    
    ret = write(fd, data, sizeof(data));
    if (ret < 0){
        printf("led control failed!\r\n");
        close(fd);
        return -1;
    }

    /*模拟应用占用25S*/
    while(1)
    {
        sleep(5);
        cnt++;
        printf("app runing times:%d\r\n",cnt);
        if (cnt >= 5) break;
    }

    printf("app runing over!\r\n");

    /*close*/
    ret = close(fd);
    if (ret < 0){
        printf("close %s failed!!", filename);
        return -1;
    }

    return 0;
}