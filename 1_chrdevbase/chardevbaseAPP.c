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
*命令调用: ./chardevbaseAPP  <filename> <1:2> 1读2写
*./chardevbaseAPP  /dev/chardevbase  1 
*./chardevbaseAPP  /dev/chardevbase  2
*/
int main(int argc, char *argv[]) 
{
    int fd = 0;
    int ret = 0;
    char *filename = argv[1];
    char readbuf[50] = {0};
    char *writebuf = "this is chardevbase test";

    if (argc != 3){
        printf("error usage!! Plese use : ./chardevbaseAPP  <filename> <1:2>");
        return -1;
    }

    /*open*/
    fd = open(filename, O_RDWR);
    if (fd < 0){
        printf("open %s failed!!\r\n", filename);
        return -1;
    }

    /*read*/
    if (atoi(argv[2]) == 1){
        ret = read(fd, readbuf, 50);
        if (ret < 0){
            printf("read %s failed!!\r\n", filename);
        }
        else{
            printf("user read data is: %s\r\n", readbuf);
        }
    }

    /*write*/
    if (atoi(argv[2]) == 2){
        ret = write(fd, writebuf, strlen(writebuf));
        if (ret < 0){
            printf("write %s failed!!\r\n", filename);
        }
        else{
            printf("user write data is: %s\r\n", writebuf);
        }
    }

    /*close*/
    ret = close(fd);
    if (ret < 0){
        printf("close %s failed!!", filename);
    }

    return 0;
}