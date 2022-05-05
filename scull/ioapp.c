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
       //char buf[256];

       if((fd0_A=open("/dev/samplehw0", O_RDWR))<0)
         perror("open");

       int val;
       ret = ioctl(fd0_A,READCMD,&val);
       if(ret){
         printf("READCMD: ioctl error:%d\n",ret);//expected error
       }else{
         printf("READCMD: ioctl val:0x%x\n",val);
       }

       int quantum=100;
       ret = ioctl(fd0_A,SCULL_IOCSQUANTUM,&quantum);//ポインタで設定
       if(ret){
         printf("SCULL_IOCSQUANTUM: ioctl error:%d\n",ret);
       }else{
         printf("SCULL_IOCSQUANTUM: ioctl success:\n");
       }
       printf("quantum:%d\n",ioctl(fd0_A,SCULL_IOCQQUANTUM));

       quantum=101;
       ioctl(fd0_A,SCULL_IOCTQUANTUM,quantum);//値で設定
       printf("SCULL_IOCTQUANTUM:%d\n",quantum);
       printf("quantum:%d\n",ioctl(fd0_A,SCULL_IOCQQUANTUM));

       ret = ioctl(fd0_A,SCULL_IOCGQUANTUM,&quantum);//ポインタで取得
       if(ret){
         printf("SCULL_IOCGQUANTUM: ioctl error:%d\n",ret);
       }else{
         printf("SCULL_IOCGQUANTUM: ioctl success:\n");
       }
       printf("SCULL_IOCGQUANTUM:%d\n",quantum);

       quantum=ioctl(fd0_A,SCULL_IOCQQUANTUM);//戻り値で取得
       printf("SCULL_IOCQQUANTUM:%d\n",quantum);

       quantum=1000;
       ret=ioctl(fd0_A,SCULL_IOCXQUANTUM,&quantum);//ポインタで交換
       printf("SCULL_IOCXQUANTUM:%d->%d\n",ret,quantum);
       printf("quantum:%d\n",ioctl(fd0_A,SCULL_IOCQQUANTUM));

       quantum=1001;
       quantum=ioctl(fd0_A,SCULL_IOCHQUANTUM,quantum);//戻り値で交換
       printf("SCULL_IOCHQUANTUM:%d\n",quantum);
       printf("quantum:%d\n",ioctl(fd0_A,SCULL_IOCQQUANTUM));

       if(close(fd0_A)!=0)
         perror("close 0_A");

       return 0;

}
