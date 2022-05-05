#ifndef _SAMPLE_H_
#define _SAMPLE_H_

#define DEVICE_NAME "devsample"

#define DEV_FILE "samplehw"
#define CLASS_NAME "sample"

#define SAMPLE_MINOR_BASE 0
#define SAMPLE_MINOR_COUNT 3

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

#define SCULL_IOC_MAGIC 'k'
/* S(Set)       ポインタで「設定する」 */
/* T(Tell)     引数の値で直接「通知」 */
/* G(Get)      ポインタで応答を「もらう」 */
/* Q(Query)    「問い合わせ」の結果は戻り値 */
/* X(eXchange)  GとSをアトミックに「交換」 */
/* H(sHift)Tと  Qをアトミックに「シフト」 */
#define SCULL_IOCRESET    _IO(SCULL_IOC_MAGIC,0)
#define SCULL_IOCSQUANTUM _IOW(SCULL_IOC_MAGIC,1,int)
#define SCULL_IOCSQSET    _IOW(SCULL_IOC_MAGIC,2,int)
#define SCULL_IOCTQUANTUM _IO(SCULL_IOC_MAGIC,3)
#define SCULL_IOCTQSET    _IO(SCULL_IOC_MAGIC,4)
#define SCULL_IOCGQUANTUM _IOR(SCULL_IOC_MAGIC,5,int)
#define SCULL_IOCGQSET    _IOR(SCULL_IOC_MAGIC,6,int)
#define SCULL_IOCQQUANTUM _IO(SCULL_IOC_MAGIC,7)
#define SCULL_IOCQQSET    _IO(SCULL_IOC_MAGIC,8)
#define SCULL_IOCXQUANTUM _IOWR(SCULL_IOC_MAGIC, 9,int)
#define SCULL_IOCXQSET    _IOWR(SCULL_IOC_MAGIC,10,int)
#define SCULL_IOCHQUANTUM _IO(SCULL_IOC_MAGIC,11)
#define SCULL_IOCHQSET    _IO(SCULL_IOC_MAGIC,12)

#define SCULL_IOC_MAXNR 14

#define SCULL_QUANTUM 4000
#define SCULL_QSET    1000

#undef PDEBUG //
#ifdef SCULL_DEBUG
#  ifdef __KERNEL__ //カーネル空間でのデバッグ用
#    define PDEBUG(fmt,args...) printk(KERN_DEBUG "scull:" fmt,## args)
#  else//ユーザ空間用
#    define PDEBUG(fmt,args...) fprintf(stderr, fmt,## args)
#  endif
#else
#  define PDEBUG(fmt,args...) //デバッグ中ではないので何もしない
#endif

#undef PDEBUGG
#define PDEGUGG(fmt,args...)  //何もしない：定義だけしておく。PDEBUGを削除する代わりに使う？

#endif
