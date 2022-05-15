#ifdef __KERNEL__
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>//copy_to_user,copy_from_user
#include <linux/kthread.h>//kthread_work,
#include <linux/delay.h>//msleep_interruptible
#include <linux/compat.h>//compat_ulong_t
#include <linux/fs.h>//compat_ptr_ioctl
#include <linux/mutex.h>//init_MUTEX?
#include <linux/semaphore.h>//init_MUTEX

#include <asm/current.h>
#include <asm/uaccess.h>
struct fifo_driver {
  struct device_driver driver;
};
#else
struct fifo_driver {

};
typedef int umode_t;

#endif

#include "sample.h"



static struct class *fifo_class;
static struct device *fifo_device;

// キャラクタデバイスオブジェクト
static int minor_count=0;

extern char *sample_devnode(struct device *dev,umode_t *mode);

struct fifo_node {
  struct fifo_node*next;
  char *data;
};

struct fifo_dev {
  struct fifo_node *head;//先頭の量子セットへのポインタ
  int node_size;//現在のノード一つのサイズ
  unsigned long size;//格納されているデータ量
  struct mutex sem;//相互排他のセマフォ
  struct cdev cdev;//キャラクタ型デバイス構造体
};

//free list and its data
void fifo_trim(struct fifo_dev *dev){
       struct fifo_node* cur=dev->head;
       while(cur){
               struct fifo_node* next=cur->next;
               kfree(cur->data);
               kfree(cur);
               cur=next;
       }
       dev->size=0;
}

static int node_size=10;
/* module_param(node_size, int, (S_IRUGO | S_IWUGO) & ~S_IWOTH);  /\* 0666 *\/ */
/* MODULE_PARM_DESC(node_size, "Fifo node size (default: 10)"); */

struct fifo_node *fifo_follow(struct fifo_dev *dev, int node_index)// n is index
{
  if(!dev->head){
    printk(KERN_NOTICE "%s:head\n",__func__);
    dev->head = kmalloc(sizeof(struct fifo_node),GFP_KERNEL);
    if(!dev->head){
      printk(KERN_NOTICE "%s:no memory\n",__func__);
      return NULL;
    }
    dev->head->data = kmalloc(sizeof(char)*dev->node_size,GFP_KERNEL);
    if(!dev->head->data){
      printk(KERN_NOTICE "%s:no memory\n",__func__);
      return NULL;
    }
  }
  struct fifo_node *cur=dev->head;
  while(node_index--){
    if(!cur->next){
      cur->next = kmalloc(sizeof(struct fifo_node),GFP_KERNEL);
      if(!cur->next){
        printk(KERN_NOTICE "%s:no memory\n",__func__);
        return NULL;
      }
      cur->next->data = kmalloc(sizeof(char)*dev->node_size,GFP_KERNEL);
      if(!cur->next->data){
        printk(KERN_NOTICE "%s:no memory\n",__func__);
        return NULL;
      }
    }
    cur=cur->next;
  }
  return cur;
}

int fifo_open(struct inode *inode,struct file *filp){
       struct fifo_dev *dev;
       dev=container_of(inode->i_cdev,struct fifo_dev,cdev);//fifo_dev->cdevからfifo_devを得る
       printk(KERN_NOTICE "%s:%d\n",__func__,dev->node_size);
       filp->private_data=dev;//のちのために保存
       /* if((filp->f_flags&O_ACCMODE)==O_WRONLY){ */
       /*      if(mutex_lock_interruptible(&dev->sem)) */
       /*              return -ERESTARTSYS; */
       /*      fifo_trim(dev); */
       /*      mutex_unlock(&dev->sem); */
       /* } */
       return 0;
}

ssize_t fifo_read(struct file*filp,char __user *buf,size_t count,loff_t *f_pos){
  struct fifo_dev *dev=filp->private_data;
  ssize_t retval=0;
  printk(KERN_NOTICE "%s:called\n",__func__);
  if(mutex_lock_interruptible(&dev->sem))
         return -ERESTARTSYS;
  if(*f_pos >= dev->size)
         goto out;
  if(*f_pos+count > dev->size)
         count=dev->size-*f_pos;

  int node_index = *f_pos/dev->node_size;
  int rest  = *f_pos%dev->node_size;

  printk(KERN_NOTICE "%s:%px:head\n",__func__,dev->head);
  struct fifo_node *cur=fifo_follow(dev,node_index);
  printk(KERN_NOTICE "%s:%c",__func__,*(cur->data));
  if(!cur || !cur->data)
         goto out;

  if(copy_to_user(buf,cur->data+rest,count)){
         retval=-EFAULT;
         goto out;
  }

  printk(KERN_NOTICE "%s:%px:%c:%lld:%d:%d\n",__func__,cur,*(cur->data+rest),*f_pos,node_index,rest);

  *f_pos+=count;
  retval = count;

  out:
   mutex_unlock(&dev->sem);
   return retval;
}

ssize_t fifo_write(struct file *filp,const char __user *buf,size_t count,loff_t *f_pos){
  struct fifo_dev *dev=filp->private_data;
  ssize_t retval= -ENOMEM;//値はgoto out文で使う

  printk(KERN_NOTICE "%s:\n",__func__);
  if (mutex_lock_interruptible(&dev->sem))
    return -ERESTARTSYS;

  int node_index = *f_pos/dev->node_size;
  int rest  = *f_pos%dev->node_size;
  printk(KERN_NOTICE "%s:%lld:%d:%d:%d\n",__func__,*f_pos,dev->node_size,node_index,rest);
  //正しい位置(開始位置)までリストをたぐる
  struct fifo_node *cur=fifo_follow(dev,node_index);
  if(!cur){
         printk(KERN_NOTICE "%s:no memory\n",__func__);
         goto out;
  }
  printk(KERN_NOTICE "%s:[2]:%ld:%d:%px\n",__func__,count,dev->node_size-rest,cur);
  //この量子の終わりまでしか書かない
  if(count> dev->node_size-rest )
         count=dev->node_size-rest;

  printk(KERN_NOTICE "%s:[3]:%d:%ld\n",__func__,rest,count);

  /*if(copy_from_user(cur->data+rest,buf+*f_pos,count)){
    printk(KERN_NOTICE "%s:copy error\n",__func__);
    retval=-EFAULT;
    goto out;
  }*/
  printk(KERN_NOTICE "%s:[4]:copied:%d:%ld\n",__func__,rest,count);

  *f_pos+=count;
  retval=count;
  for(int i=0;i<count;i++){
         printk(KERN_NOTICE "%s:%c\n",__func__,*(cur->data+rest+i));
  }
  /* printk(KERN_NOTICE "%s:%px:%c:%lld:%zd\n",__func__,cur,*(cur->data+rest),*f_pos,retval); */

  // サイズを更新する(いつも更新するわけではないのはseekするから？)
  if(dev->size<*f_pos){
    dev->size=*f_pos;
  }
 out:
  mutex_unlock(&dev->sem);
  return retval;
}

int fifo_release(struct inode *inode,struct file *filp){
       return 0;
}

struct file_operations fifo_fops={
  .owner = THIS_MODULE,
  //.llseek=fifo_llseek,
  .read   = fifo_read,
  .write  = fifo_write,
  .open   = fifo_open,
  .release= fifo_release,
};

int fifo_major=0;
#define FIFO_MINOR 0

static void fifo_setup_cdev(struct fifo_dev *dev,int index){
       int err,devno=MKDEV(fifo_major,FIFO_MINOR+index);
       cdev_init(&dev->cdev,&fifo_fops);
       dev->cdev.owner=THIS_MODULE;
       dev->cdev.ops=&fifo_fops;
       err=cdev_add(&dev->cdev,devno,1);
       if(err)
               printk(KERN_NOTICE "Error %d adding fifo%d",err,index);
}
int fifo_nr_devs=1;
struct fifo_dev *fifo_devices;

int fifo_init(struct fifo_driver *drv){
  int ret;
  int registered=0;
  int added=0;
  int classed=0;

  dev_t devid;


  printk(KERN_ALERT "fifo driver loaded2\n");

  //キャラクターデバイスの空いているメジャー番号を取得する
  ret=alloc_chrdev_region(&devid,FIFO_MINOR,fifo_nr_devs,DEVICE_NAME);

  // 獲得したdev_t(メジャー番号+マイナー番号)からメジャー番号を取り出す
  fifo_major=MAJOR(devid);
  if(ret){
    printk("register_chrdev error %d(%pe)\n",ret,ERR_PTR(ret));
    goto error;
  }
  printk("%s:Major number %d was assigned\n",__func__,fifo_major);
  registered=1;

  /* //各種システムコールに対応するハンドラテーブル */
  /* static const struct file_operations sample_fops = { */
  /*   .open    = sample_open, */
  /*   .release = sample_close, */
  /*   .read    = sample_read, */
  /*   .unlocked_ioctl = sample_ioctl, */
  /*   .compat_ioctl = compat_ptr_ioctl,//compat_ptr_ioctlは型を変換してunlocked_ioctlを呼び出す */
  /*   .write   = sample_write, */
  /* }; */

  /* // cdev構造体のコンストラクタでハンドラテーブルの登録 */
  /* cdev_init(&fifo_cdev,&sample_fops); */
  /* // cdev構造体をカーネルに登録する */
  /* ret=cdev_add(&fifo_cdev,devid,SAMPLE_MINOR_COUNT); */
  //fifo_setup_cdev(struct fifo_dev *dev,int index){

  int result;
  fifo_devices = kmalloc(fifo_nr_devs * sizeof(struct fifo_dev), GFP_KERNEL);
  if (!fifo_devices) {
    result = -ENOMEM;
    goto error;  /* Make this more graceful */
  }
  memset(fifo_devices, 0, fifo_nr_devs * sizeof(struct fifo_dev));
  printk("%s:%d:\n",__func__,node_size);
  /* Initialize each device. */
  for (int i = 0; i < fifo_nr_devs; i++) {
         printk("%s:%d:\n",__func__,node_size);
    fifo_devices[i].node_size = node_size;
    mutex_init(&fifo_devices[i].sem);
    fifo_setup_cdev(&fifo_devices[i], i);
  }
  if(ret<0){
    printk("cdev_add error %d(%pe)\n",ret,ERR_PTR(ret));
    goto error;
  }
  added=1;
  // デバイスクラス/デバイス情報(gpio,block,rtc...)を作成(本ドライバは独自クラス)
  // 作成したクラスは/sys/classに現れる
  fifo_class=class_create(THIS_MODULE,CLASS_NAME);
  if(IS_ERR(fifo_class)){
    printk("class_create error (%pe)\n",ERR_CAST(fifo_class));
    ret=PTR_ERR(fifo_class);
    goto error;
  }

  //誰でも読み書きできるようにデバイスファイルの所有権を強制する
  fifo_class->devnode=sample_devnode;
  classed=1;

  for(int i=0;i<fifo_nr_devs;i++){
    //クラスを指定してデバイスファイルの作成(/dev/samplehw*)
    fifo_device=device_create(fifo_class,NULL,
                            MKDEV(fifo_major,FIFO_MINOR+i),
                            NULL,
                            DEV_FILE"%d",i
                            );
    if(IS_ERR(fifo_device)){
      printk("device_create error (%pe)\n",ERR_CAST(fifo_device));
      ret=PTR_ERR(fifo_device);
      goto error;
    }
    minor_count++;
  }

  return 0;
 error:
  for(int i=0;i<minor_count;i++){
         //デバイスファイルの破棄<->device_create
    device_destroy(fifo_class,MKDEV(fifo_major,FIFO_MINOR+i));
    printk("%s:device_destroy\n",__func__);
  }
  if(classed){
         //デバイスクラスの破棄<->class_create
    class_destroy(fifo_class);
  }
  if(registered){
    //キャラクタデバイスの破棄<->cdev_add
    for(int i=0;i<minor_count;i++){
      cdev_del(&fifo_devices[i].cdev);
    }
    //キャラクタデバイスの登録解除<->alloc_chrdev_region
    dev_t dev=MKDEV(fifo_major,FIFO_MINOR);
    unregister_chrdev_region(dev,fifo_nr_devs);
  }
  return ret;
}

void fifo_exit(struct fifo_driver *drv){
  printk(KERN_ALERT "fifo driver unloaded\n");

  //for(int i=0;i<minor_count;i++){
  for (int i = 0; i < fifo_nr_devs; i++) {
    device_destroy(fifo_class,MKDEV(fifo_major,FIFO_MINOR+i));
    printk("%s:device_destroy\n",__func__);
  }

  class_destroy(fifo_class);
  for(int i=0;i<minor_count;i++){
      cdev_del(&fifo_devices[i].cdev);
    }
  dev_t dev=MKDEV(fifo_major,FIFO_MINOR);
  unregister_chrdev_region(dev,fifo_nr_devs);
}
