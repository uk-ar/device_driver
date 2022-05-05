#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "../sample.h"

int main(void){
  int fd0_A;
       ssize_t ret;
       char buf[256];

       if((fd0_A=open("/dev/samplehw0", O_RDWR)<0)) perror("open");

       int quantum=100;
       int r = ioctl(fd0_A,SCULL_IOCSQUANTUM,&quantum);//ポインタで設定
       if(ret){
         printf("SCULL_IOCSQUANTUM: ioctl error:%d\n",r);
       }else{
         printf("SCULL_IOCSQUANTUM: ioctl success:\n");
       }
       printf("SCULL_IOCSQUANTUM:%d\n",quantum);
       ioctl(fd0_A,SCULL_IOCTQUANTUM,quantum);//値で設定
       printf("SCULL_IOCTQUANTUM:%d\n",quantum);

       r=ioctl(fd0_A,SCULL_IOCGQUANTUM,&quantum);//ポインタで取得
       if(ret){
         printf("SCULL_IOCGQUANTUM: ioctl error:%d\n",r);
       }else{
         printf("SCULL_IOCGQUANTUM: ioctl success:\n");
       }
       printf("SCULL_IOCGQUANTUM:%d\n",quantum);
       ioctl(fd0_A,SCULL_IOCGQUANTUM,&quantum);
       quantum=ioctl(fd0_A,SCULL_IOCQQUANTUM);//戻り値で取得
       printf("SCULL_IOCQQUANTUM:%d\n",quantum);

       quantum=1000;
       r=ioctl(fd0_A,SCULL_IOCXQUANTUM,quantum);//ポインタで交換
       if(ret){
         printf("SCULL_IOCXQUANTUM: ioctl error:%d\n",r);
       }else{
         printf("SCULL_IOCXQUANTUM: ioctl success:\n");
       }
       printf("SCULL_IOCXQUANTUM:%d\n",quantum);
       quantum=ioctl(fd0_A,SCULL_IOCHQUANTUM,quantum);//戻り値で交換
       printf("SCULL_IOCHQUANTUM:%d\n",quantum);

       if(close(fd0_A)!=0)
         perror("close 0_A");

       return 0;

}
