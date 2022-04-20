#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>//copy_to_user,copy_from_user

#include <asm/current.h>
#include <asm/uaccess.h>

#define MODULE_NAME "hello_driver"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("This is a hello driver");
MODULE_AUTHOR("Yuuki Arisawa");

#define DEVICE_NAME "devsample"

#define DEV_FILE "samplehw"
#define CLASS_NAME "sample"

#define SAMPLE_MINOR_BASE 0
#define SAMPLE_MINOR_COUNT 3

static struct class *my_class;
static struct device *my_device;

static int g_major_num;
struct hello_driver {
  struct device_driver driver;
};


#define NUM_BUFFER 256
struct _mydevice_file_data{
       unsigned char buffer[NUM_BUFFER];
};

static int sample_open(struct inode *node,struct file *filp){
  printk("%s entered\n",__func__);
  struct _mydevice_file_data *p=kmalloc(sizeof(struct _mydevice_file_data),GFP_KERNEL);
  if(p==NULL){
         printk(KERN_ERR "kmalloc\n");
         return -ENOMEM;
  }
  //固有データの初期化
  strlcat(p->buffer,"dummy",5);
  //確保したポインタはユーザーがfdの形で保持
  filp->private_data=p;
  return 0;
}

unsigned char stored_value[NUM_BUFFER];
static int sample_close(struct inode *node,struct file *filp){
  printk("%s entered\n",__func__);
  if(filp->private_data){
         kfree(filp->private_data);
         filp->private_data=NULL;
  }
  return 0;
}
#define BUFFER_MAX 32
static char g_buffer[BUFFER_MAX];
static size_t g_buffer_count;
wait_queue_head_t g_read_wait;

//      xxxx
//MAX       ^
//count   ^
static ssize_t sample_write(struct file *filp,const char __user *buf,size_t count,loff_t *f_pos){
  //書き込める最大バイト数
  size_t len=min(BUFFER_MAX-g_buffer_count,count);
  if(len==0){
    printk("%s:%zu+%zu overflow\n",__func__,g_buffer_count,count);
    return -ENOSPC;
  }
  //g_buffer_countの位置からlenだけ書き込み
  //戻り値は書き込めなかった残り
  unsigned long rem = copy_from_user(&g_buffer[g_buffer_count],buf,len);
  printk("copy_from_user %lu\n",rem);
  ssize_t written;
  if(rem){
    written=len-rem;
  }else{
    written=len;
  }
  g_buffer_count+=written;
  //読み込み待ちをしているプロセスに通知
  wake_up_interruptible(&g_read_wait);

  return written;
}

static inline bool buffer_readable(void){
  return (g_buffer_count > 0);
}

static ssize_t sample_read(struct file *filp,char __user *buf,size_t count,loff_t *f_pos){
  unsigned long rem;
  size_t copied,res;
  int ret;

  //実態はマクロ、conditionが0の場合に現在のプロセスを待ちにする
  //wake_up されるとconditionを判定し、0以外であれば待ちを解除
  ret=wait_event_interruptible(g_read_wait,buffer_readable());
  //ret=wait_event_interruptible(g_read_wait,g_buffer_count>0);でもOK
  //シグナル受信した場合
  if(ret<0){
    return -ERESTARTSYS;
  }
  //読み込み可能な最大バイト数
  copied=min(g_buffer_count,count);
  rem=copy_to_user(buf,g_buffer,copied);
  if(rem){
    return -EFAULT;
  }
  //       xxxx
  //MAX        ^
  //count    ^
  //copied  ^
  res=g_buffer_count-copied;
  //まだバッファにデータが残っている場合はデータを前方にコピー
  if(res>0)
    memmove(&g_buffer[0],&g_buffer[copied],res);

  g_buffer_count=res;
  return copied;
}
//いつ呼び出しても最後にwriteされた値を読む
static ssize_t sample_read1(struct file *filp,char __user *buf,size_t count,loff_t *f_pos){
       printk("%s entered\n",__func__);
       if(count > NUM_BUFFER )
               count = NUM_BUFFER;
       struct _mydevice_file_data*p=filp->private_data;
       if(copy_to_user(buf,p->buffer,count)!=0)
               return -EFAULT;
       return count;
}

static ssize_t sample_write1(struct file *filp,const char __user *buf,size_t count,loff_t *f_pos){
       printk("%s entered\n",__func__);
       if(count > NUM_BUFFER )
               count = NUM_BUFFER;
       struct _mydevice_file_data*p=filp->private_data;
       if(copy_from_user(p->buffer,buf,count)!=0)
               return -EFAULT;
       return count;
}

static ssize_t sample_read0(struct file *filp,char __user *buf,size_t count,loff_t *f_pos){
  printk("%s entered\n",__func__);
  if(count > NUM_BUFFER )
         count = NUM_BUFFER;
  if(copy_to_user(buf,stored_value,count)!=0)
         return -EFAULT;
  return count;
}

static ssize_t sample_write0(struct file *filp,const char __user *buf,size_t count,loff_t *f_pos){
  printk("%s entered\n",__func__);
  if(count > NUM_BUFFER )
         count = NUM_BUFFER;
  if(copy_from_user(stored_value,buf,count)!=0)
         return -EFAULT;
  return count;
}

static char *sample_devnode(struct device *dev,umode_t *mode){
  int minor=MINOR(dev->devt);

  printk("%s: %p (%d)\n",__func__,mode,minor);

  if(mode && minor==0){
    //デバイスファイルのパーミッション設定
    *mode=0666;
    printk("mode %o\n",*mode);
  }
  return NULL;
}
// キャラクタデバイスオブジェクト
static struct cdev my_cdev;
int minor_count=0;

static int hello_init(struct hello_driver *drv){
  int ret;
  int registered=0;
  int added=0;
  int classed=0;

  dev_t devid;

  printk(KERN_ALERT "hello driver loaded\n");
  // charactor device

  //キャラクターデバイスの空いているメジャー番号を取得する
  ret=alloc_chrdev_region(&devid,SAMPLE_MINOR_BASE,SAMPLE_MINOR_COUNT,DEVICE_NAME);

  // 獲得したdev_t(メジャー番号+マイナー番号)からメジャー番号を取り出す
  g_major_num=MAJOR(devid);
  if(ret){
    printk("register_chrdev error %d(%pe)\n",ret,ERR_PTR(ret));
    goto error;
  }
  printk("%s:Major number %d was assigned\n",__func__,g_major_num);
  registered=1;

  //各種システムコールに対応するハンドラテーブル
  static const struct file_operations sample_fops = {
    .open    = sample_open,
    .release = sample_close,
    .read    = sample_read,
    .write   = sample_write,
  };

  // cdev構造体のコンストラクタでハンドラテーブルの登録
  cdev_init(&my_cdev,&sample_fops);
  // cdev構造体をカーネルに登録する
  ret=cdev_add(&my_cdev,devid,SAMPLE_MINOR_COUNT);
  if(ret<0){
    printk("cdev_add error %d(%pe)\n",ret,ERR_PTR(ret));
    goto error;
  }
  added=1;
  // デバイスクラス/デバイス情報(gpio,block,rtc...)を作成(本ドライバは独自クラス)
  // 作成したクラスは/sys/classに現れる
  my_class=class_create(THIS_MODULE,CLASS_NAME);
  if(IS_ERR(my_class)){
    printk("class_create error (%pe)\n",ERR_CAST(my_class));
    ret=PTR_ERR(my_class);
    goto error;
  }

  //誰でも読み書きできるようにデバイスファイルの所有権を強制する
  my_class->devnode=sample_devnode;
  classed=1;

  for(int i=0;i<SAMPLE_MINOR_COUNT;i++){
    //クラスを指定してデバイスファイルの作成(/dev/samplehw*)
    my_device=device_create(my_class,NULL,
                            MKDEV(g_major_num,SAMPLE_MINOR_BASE+i),
                            NULL,
                            DEV_FILE"%d",i
                            );
    if(IS_ERR(my_device)){
      printk("device_create error (%pe)\n",ERR_CAST(my_device));
      ret=PTR_ERR(my_device);
      goto error;
    }
    minor_count++;
  }
  return 0;
 error:
  for(int i=0;i<minor_count;i++){
         //デバイスファイルの破棄<->device_create
    device_destroy(my_class,MKDEV(g_major_num,SAMPLE_MINOR_BASE+i));
    printk("%s:device_destroy\n",__func__);
  }
  if(classed){
         //デバイスクラスの破棄<->class_create
    class_destroy(my_class);
  }
  if(registered){
         //キャラクタデバイスの破棄<->cdev_add
    dev_t dev=MKDEV(g_major_num,SAMPLE_MINOR_BASE);
    cdev_del(&my_cdev);
    //キャラクタデバイスの登録解除<->alloc_chrdev_region
    unregister_chrdev_region(dev,SAMPLE_MINOR_COUNT);
  }
  return ret;
}

static void hello_exit(struct hello_driver *drv){
  printk(KERN_ALERT "hello driver unloaded\n");
  for(int i=0;i<minor_count;i++){

    device_destroy(my_class,MKDEV(g_major_num,SAMPLE_MINOR_BASE+i));
    printk("%s:device_destroy\n",__func__);
  }

  class_destroy(my_class);

  cdev_del(&my_cdev);
  dev_t dev=MKDEV(g_major_num,SAMPLE_MINOR_BASE);
  unregister_chrdev_region(dev,SAMPLE_MINOR_COUNT);
}

static struct hello_driver he_drv={
  .driver = {
    .name = MODULE_NAME,
  }
};
#if 0
//list of data
struct scull_qset{
       void** data;
       struct scull_qset *next;
};

struct scull_dev{
       struct scull_qset *data;
       int quantum;
       int qset;
       unsigned long size;
       unsigned int access_key;
       struct semaphore sem;
       struct cdev cdev;
};
//free list
int scull_trim(struct scull_dev *dev){
       struct scull_qset *next,*dptr;
       int qset=dev->qset;
       int i;
       for(dptr=dev->data;dptr;dptr=next){
               if(dptr->data){

               }
       }
}
static void scull_setup_cdev(struct scull_dev *dev,int index){
       int err,devno=MKDEV(scull_major,scull_minor+index);
       cdev_init(&dev->cdev,&scull_fops);
       dev->cdev.owner=THIS_MODULE;
       dev->cdev.ops=&scull_fops;
       err=cdev_add(&dev->cdev,devno,1);
       if(err)
               printk(KERN_NOTICE "Error %d adding scull%d",err,index);
}

int scull_open(struct inode *inode,struct file *filp){
       struct scull_dev *dev;
       dev=container_of(inode->i_cdev,struct scull_dev,cdev);//scull_dev->cdevからscull_devを得る
       filp->private_data=dev;//のちのために保存
       //書き込み専用オープンならここでデバイス長を0に切り捨て
       if((filp->f_flags&O_ACCMODE)==O_WRONLY){
               scull_trim(dev);
       }
       return 0;
}

int scull_release(struct inode *inode,struct file *filp){
       return 0;
}
#endif
module_driver(he_drv,hello_init,hello_exit);
