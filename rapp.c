#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(void){
  int fd0_A;
  ssize_t read_size=64;
  char buf[256];

  if((fd0_A=open("/dev/samplehw0", O_RDWR)<0)) perror("open");

  printf("reading %zd byte...\n",read_size);
  ssize_t ret = read(fd0_A,buf,read_size);
  if(ret==-1) perror("read");
  printf("read size %zd...\n",ret);
  for(int i=0;i<ret;i++)
    printf("%c",buf[i]);
  printf("\n");
  if(close(fd0_A)!=0)perror("close 0_A");
  return 0;

}
