#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define DEVFILE "/dev/samplehw"

int main(void){
  int fd;
  ssize_t ret;
  char buf[32];

  fd=open(DEVFILE, O_RDWR);
  if(fd==-1){
    perror("open");
    exit(1);
  }
  ret=read(fd,buf,sizeof(buf));
  if(ret==-1){
    perror("read");
  }
  close(fd);
  return 0;

}
