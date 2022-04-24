#ifndef _SAMPLE_H_
#define _SAMPLE_H_

#define SAMPLE_IOCTL 's'

#define READCMD  _IOR(SAMPLE_IOCTL,1,int)
#define WRITECMD _IOW(SAMPLE_IOCTL,2,int)

struct sample_data_cmd{
  unsigned char mask;
  unsigned long val;
};

#define DATACMD  _IOW(SAMPLE_IOCTL,3,struct sample_data_cmd)

#ifdef __KERNEL__
struct compat_sample_data_cmd{
  unsigned char mask;
  compat_ulong_t val;
};
#define I32DATACMD _IOW(SAMPLE_IOCTL,3,struct compat_sample_data_cmd)

#endif
#endif
