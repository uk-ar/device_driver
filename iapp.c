#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "sample.h"

#include <sys/ioctl.h>//ioctl

int main(void){
  int fd;
  ssize_t ret;
  int val;
  struct sample_data_cmd dcmd;

  fd=open("/dev/samplehw0", O_RDWR);
  if(fd==-1)
    perror("open");

  val = 0;
  ret = ioctl(fd,READCMD,&val);
  if(ret){
    printf("READCMD: ioctl error:%d\n",ret);
  }else{
    printf("READCMD: ioctl val:0x%x\n",val);
  }

  val = 0x1234abcd;
  ret = ioctl(fd,WRITECMD,&val);
  if(ret){
    printf("WRITECMD: ioctl error:%d\n",ret);
  }else{
    printf("WRITECMD: ioctl success:\n");
  }

  dcmd.mask=0xfe;
  dcmd.val=~0;//?1111?
  printf("sizeof %zu\n",sizeof(dcmd));
  ret=ioctl(fd,DATACMD,&dcmd);
  if(ret){
    printf("DATACMD: ioctl error:%d\n",ret);
  }else{
    printf("DATACMD: ioctl success:\n");
  }

  if(close(fd)!=0)
    perror("close 0_A");
  return 0;

}
