#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(void){
  int fd;
  ssize_t ret;
  char buf[64];
  ssize_t read_size=sizeof(buf);
  fd=open("/dev/samplehw0", O_RDWR);
  if(fd==-1)
    perror("open");
  printf("reading %d byte...\n",read_size);
  ret = read(fd,buf,read_size);
  if(ret==-1)
    perror("read");
  printf("read size %zd...\n",ret);
  for(int i=0;i<ret;i++)
    printf("%c",buf[i]);
  printf("\n");
  if(close(fd)!=0)
    perror("close 0_A");
  return 0;

}
