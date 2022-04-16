#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(void){
       int fd0_A,fd0_B,fd1_A;
       ssize_t ret;
       char buf[256];

       if((fd0_A=open("/dev/samplehw0", O_RDWR)<0)) perror("open");
       if((fd0_B=open("/dev/samplehw0", O_RDWR)<0)) perror("open");
       if((fd1_A=open("/dev/samplehw1", O_RDWR)<0)) perror("open");

       if(write(fd0_A,"0_A",4)<0)perror("write");
       if(write(fd0_B,"0_B",4)<0)perror("write");
       if(write(fd1_A,"1_A",4)<0)perror("write");

       if(read(fd0_A,buf,sizeof(buf))) perror("read");
       printf("%s\n",buf);
       if(read(fd0_B,buf,sizeof(buf))) perror("read");
       printf("%s\n",buf);
       if(read(fd1_A,buf,sizeof(buf))) perror("read");
       printf("%s\n",buf);

       if(close(fd0_A)!=0)perror("close 0_A");
       if(close(fd0_B)!=0)perror("close 0_B");
       if(close(fd1_A)!=0)perror("close 1_A");
       return 0;

}

#define DEVFILE "/dev/samplehw0"
int main0(void){
  int fd;
  ssize_t ret;
  char buf[32];

  fd=open(DEVFILE, O_RDWR);
  if(fd==-1){
    perror("open");
    exit(1);
  }
  ret=read(fd,buf,sizeof(buf));
  printf("%s\n",buf);
  if(ret==-1){
    perror("read");
  }
  close(fd);
  return 0;

}
