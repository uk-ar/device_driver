#include <stdio.h>^M
#include <stdlib.h>^M
#include <sys/types.h>^M
#include <sys/stat.h>^M
#include <fcntl.h>^M
#include <unistd.h>^M
^M
int main(void){^M
       int fd0_A,fd0_B,fd1_A;^M
       ssize_t ret;^M
       char buf[256];^M
^M
       if((fd0_A=open("/dev/samplehw0", O_RDWR)<0)) perror("open");^M
^M
       int quantum;^M
       ioctl(fd0_A,SCULL_IOCSQUANTUM,&quantum);//ポインタで設定^M
       ioctl(fd0_A,SCULL_IOCTQUANTUM,quantum);//値で設定^M
^M
       ioctl(fd0_A,SCULL_IOCGQUANTUM,&quantum);//ポインタで取得^M
       quantum=ioctl(fd0_A,SCULL_IOCQQUANTUM);//戻り値で取得^M
       ^M
       ioctl(fd0_A,SCULL_IOCXQUANTUM,quantum);//ポインタで交換^M
       quantum=ioctl(fd0_A,SCULL_IOCHQUANTUM,quantum);//戻り値で交換^M
       ^M
       return 0;^M
^M
}^M
^M
