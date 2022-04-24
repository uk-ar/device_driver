#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
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
#include "sample.h"

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

static struct task_struct *thread;
static struct kthread_worker kworker;

struct sample_work_data {
  struct kthread_work do_work;
  void *ptr;
  size_t count;
};
static struct sample_work_data workdata;

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

static DEFINE_MUTEX(g_mutex);

static inline bool buffer_readable(void){
  return (g_buffer_count > 0);
}
//バッファ共有　排他あり
static ssize_t sample_write(struct file *filp,const char __user *buf,size_t count,loff_t *f_pos){
  //書き込める最大バイト数

  mutex_lock(&g_mutex);//g_buffer_countとg_bufferを排他
  size_t len=min(BUFFER_MAX-g_buffer_count,count);
  if(len==0){
    printk("%s:%zu+%zu overflow\n",__func__,g_buffer_count,count);
    mutex_unlock(&g_mutex);
    return -ENOSPC;
  }
  //g_buffer_countの位置からlenだけ書き込み
  //戻り値は書き込めなかった残り
  //unsigned long rem = 0;
  unsigned long rem = copy_from_user(&g_buffer[g_buffer_count],buf,len);
  printk("%s copy_from_user %lu\n",__func__,rem);
  ssize_t written;
  if(rem){
    written=len-rem;
  }else{
    written=len;
  }
  g_buffer_count+=written;
  //読み込み待ちをしているプロセスに通知(割り込み可能)
  wake_up_interruptible(&g_read_wait);

  //writeのたびにqueueに追加されるわけではない
  workdata.ptr=(void*)buf;//ユーザー空間のポインタを渡してもカーネルスレッドからは参照できない(後からcopy_from_userすればOK？ or 非同期なので参照する頃にはデータが残っていない？)
  workdata.count=count;
  kthread_queue_work(&kworker,&workdata.do_work);

  mutex_unlock(&g_mutex);
  return written;
}

static int sample_do_ioctl(struct file *filp,unsigned int cmd,unsigned long arg){
  int ret;
  int val;
  void __user *argp=(void __user *)arg;
  struct sample_data_cmd data_cmd;
  struct compat_sample_data_cmd compat_data_cmd;

  switch(cmd){
  case READCMD:
    printk("READCMD\n");
    val = 0xcafebabe;
    ret = copy_to_user(argp,&val,sizeof(val));
    if(ret)
      return -EFAULT;
    break;
  case WRITECMD:
    printk("WRITECMD\n");
    ret=copy_from_user(&val,argp,sizeof(val));
    if(ret)
      return -EFAULT;
    printk("WRITECMD %x\n",val);
    break;
  case DATACMD:
    printk("DATACMD\n");
    ret=copy_from_user(&data_cmd,argp,sizeof(data_cmd));
    if(ret)
      return -EFAULT;
    printk("DATACMD: mask %x val %lx sizeof %zu\n",
           data_cmd.mask,data_cmd.val,sizeof(data_cmd));
    break;
  case I32DATACMD:
    printk("I32DATACMD\n");
    ret=copy_from_user(&compat_data_cmd,argp,sizeof(compat_data_cmd));
    if(ret)
      return -EFAULT;
    printk("I32DATACMD: mask %x val %x sizeof %zu\n",
           compat_data_cmd.mask,compat_data_cmd.val,sizeof(compat_data_cmd));
    break;
  default:
    return -EINVAL;
  }
  return 0;
}

static long sample_ioctl(struct file *filp,unsigned int cmd,unsigned long arg){
  long ret;
  printk("%s entering\n",__func__);
  mutex_lock(&g_mutex);
  ret = sample_do_ioctl(filp,cmd,arg);
  mutex_unlock(&g_mutex);
  printk("%s entered\n",__func__);
  return ret;
}
//バッファ共有　排他あり
static ssize_t sample_read(struct file *filp,char __user *buf,size_t count,loff_t *f_pos){
  unsigned long rem;
  size_t copied,res;
  int ret=0;
  printk("%s entered\n",__func__);
  //実態はマクロ、conditionが0の場合に現在のプロセスを待ちにする
  //wake_up されるとconditionを判定し、0以外であれば待ちを解除(割り込み可能)
  ret=wait_event_interruptible(g_read_wait,buffer_readable());
  //wait_event(g_read_wait,buffer_readable());
  //ret=wait_event_interruptible(g_read_wait,g_buffer_count>0);でもOK
  //シグナル受信した場合
  if(ret<0){
    return -ERESTARTSYS;
  }
  mutex_lock(&g_mutex);
  //読み込み可能な最大バイト数
  copied=min(g_buffer_count,count);
  rem=copy_to_user(buf,g_buffer,copied);
  printk("%s copied\n",__func__);
  if(rem){
    mutex_unlock(&g_mutex);
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
  mutex_unlock(&g_mutex);
  return copied;
}

//      xxxx
//MAX       ^
//count   ^
//バッファ共有　排他なし
static ssize_t sample_write_shared(struct file *filp,const char __user *buf,size_t count,loff_t *f_pos){
  //書き込める最大バイト数
  size_t len=min(BUFFER_MAX-g_buffer_count,count);
  if(len==0){
    printk("%s:%zu+%zu overflow\n",__func__,g_buffer_count,count);
    return -ENOSPC;
  }
  //g_buffer_countの位置からlenだけ書き込み
  //戻り値は書き込めなかった残り
  //unsigned long rem = 0;
  unsigned long rem = copy_from_user(&g_buffer[g_buffer_count],buf,len);
  printk("%s copy_from_user %lu\n",__func__,rem);
  ssize_t written;
  if(rem){
    written=len-rem;
  }else{
    written=len;
  }
  g_buffer_count+=written;
  //読み込み待ちをしているプロセスに通知(割り込み可能)
  //wake_up_interruptible(&g_read_wait);
  wake_up(&g_read_wait);
  return written;
}

//バッファ共有　排他なし
static ssize_t sample_read_shared(struct file *filp,char __user *buf,size_t count,loff_t *f_pos){
  unsigned long rem;
  size_t copied,res;
  int ret=0;
  printk("%s entered\n",__func__);
  //実態はマクロ、conditionが0の場合に現在のプロセスを待ちにする
  //wake_up されるとconditionを判定し、0以外であれば待ちを解除(割り込み可能)
  //ret=wait_event_interruptible(g_read_wait,buffer_readable());
  wait_event(g_read_wait,buffer_readable());
  //ret=wait_event_interruptible(g_read_wait,g_buffer_count>0);でもOK
  //シグナル受信した場合
  if(ret<0){
    return -ERESTARTSYS;
  }
  //読み込み可能な最大バイト数
  copied=min(g_buffer_count,count);
  rem=copy_to_user(buf,g_buffer,copied);
  printk("%s copied\n",__func__);
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

static int dothread(void *arg){
  printk("Thread start\n");

  for(;;){
    if(kthread_should_stop()){
      printk("Thread cnancel\n");
      break;
    }
    msleep_interruptible(1000);

  }
  printk("Thread end\n");
  return 0;
}

static void sample_do_proc(struct kthread_work *ws){
  struct sample_work_data *wp;

  //kthread_work *wsをsample_work_data.do_workと見做してsample_work_dataのポインタを得る
  wp=container_of(ws,struct sample_work_data,do_work);

  printk("%s: %px %zu\n",__func__,wp->ptr,wp->count);
  for(int i=0;i<wp->count;i++)
    printk("%c",wp->ptr+i);
  printk("\n");
  msleep_interruptible(10*1000);

  printk("%s leaved.\n",__func__);
}

static int hello_init0(struct hello_driver *drv){
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
    .unlocked_ioctl = sample_ioctl,
    .compat_ioctl = compat_ptr_ioctl,//compat_ptr_ioctlは型を変換してunlocked_ioctlを呼び出す
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

  /* waitキューの初期化 */
  init_waitqueue_head(&g_read_wait);
  /* バッファの初期化 */
  g_buffer_count = 0;

  int err;

  kthread_init_worker(&kworker);
  thread=kthread_run(kthread_worker_fn,&kworker,"samplethread2");
  //thread = kthread_run(do_thread,&err,"samplethread");
  if(IS_ERR(thread)){
    err=PTR_ERR(thread);
    thread=NULL;
    printk("unable to create kernel thread %i\n",err);
    ret=err;
    goto error;
  }
  kthread_init_work(&workdata.do_work,sample_do_proc);

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

  if(thread){
    //リストに溜まっていたリクエストを処理
    kthread_flush_worker(&kworker);
    kthread_stop(thread);
  }
  /* if(thread){ */
  /*   kthread_stop(thread); */
  /*  } */

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
//list of data
struct scull_qset{//量子セットのノード
       void** data;
       struct scull_qset *next;
};

struct scull_dev{
  struct scull_qset *data;//先頭の量子セットへのポインタ
  int quantum;//現在の量子サイズ(量子一つの大きさ)
  int qset;//現在の量子配列サイズ
  unsigned long size;//ここに格納されているデータ量(リストの長さ*quantum*qset)
  unsigned int access_key;//sculluidとscullprivが使用
  struct mutex sem;//相互排他のセマフォ
  struct cdev cdev;//キャラクタ型デバイス構造体
};
int scull_quantum=5;
int scull_qset=4;
//free list and its data
int scull_trim(struct scull_dev *dev){
  struct scull_qset *next,*dptr;
  int qset=dev->qset;//データの配列サイズを取得
  for(dptr=dev->data;dptr;dptr=next){//リストのノードを回す
    if(dptr->data){//ノードにデータが存在
      for(int i=0;i<qset;i++)//データ配列を回す
        kfree(dptr->data[i]);
      kfree(dptr->data);
      dptr->data=NULL;
    }
    next=dptr->next;
    kfree(dptr);
  }
  dev->size=0;//格納されているデータ量を0に
  dev->quantum=scull_quantum;
  dev->qset=scull_qset;
  dev->data=NULL;
  return 0;
}

int scull_open(struct inode *inode,struct file *filp){
       struct scull_dev *dev;
       dev=container_of(inode->i_cdev,struct scull_dev,cdev);//scull_dev->cdevからscull_devを得る
       filp->private_data=dev;//のちのために保存
       //書き込み専用オープンならここでデバイス長を0に切り捨て
       //書き出し前に毎回バッファがクリアされる
       if((filp->f_flags&O_ACCMODE)==O_WRONLY){
         if(mutex_lock_interruptible(&dev->sem))
           return -ERESTARTSYS;
         scull_trim(dev);
         mutex_unlock(&dev->sem);
       }
       return 0;
}

struct scull_qset *scull_follow(struct scull_dev *dev, int n)
{
	struct scull_qset *qs = dev->data;

        /* Allocate first qset explicitly if need be */
	if (! qs) {
		qs = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
		if (qs == NULL)
			return NULL;  /* Never mind */
		memset(qs, 0, sizeof(struct scull_qset));
	}

	/* Then follow the list */
	while (n--) {
		if (!qs->next) {
			qs->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
			if (qs->next == NULL)
				return NULL;  /* Never mind */
			memset(qs->next, 0, sizeof(struct scull_qset));
		}
		qs = qs->next;
		continue;
	}
	return qs;
}
ssize_t scull_read(struct file*filp,char __user *buf,size_t count,loff_t *f_pos){
  struct scull_dev *dev=filp->private_data;
  struct scull_qset *dptr;//最初のリストアイテム(head?)
  int quantum=dev->quantum,qset=dev->qset;
  int itemsize=quantum*qset;//リストアイテムにあるバイト数
  int item,s_pos,q_pos,rest;
  ssize_t retval=0;
  if(mutex_lock_interruptible(&dev->sem))//lock
    return -ERESTARTSYS;
  if(*f_pos>=dev->size)
    goto out;
  if(*f_pos+count>dev->size)
    count=dev->size-*f_pos;//格納されているデータ量を超えて読み込みたい場合は、読み込むデータ量を格納されているデータ量まで切り詰める

  //f_posとして読み出し開始位置が渡ってくる
  //リストアイテムとqsetのインデックスを求め、量子の中でずらす
  item=(long)*f_pos/itemsize;//リストのインデックス(リストの長さ*quantum*qset/(quantum*qset))
  rest=(long)*f_pos%itemsize;//リストノードの中でのデータ量(リストの長さ*quantum*qset%(quantum*qset))
  s_pos=rest/quantum;//リストノードの中でのインデックス(dptr->data[s_pos]:quantum*i/quantum)
  q_pos=rest%quantum;//quantumより小さいデータの量？

  //正しい位置までリストをたぐる(よそで定義されている)
  dptr=scull_follow(dev,item);

  if(dptr==NULL||!dptr->data||!dptr->data[s_pos])
    goto out;//穴を埋めない

  //この量子の終わりまでしか読まない
  if(count>quantum-q_pos)
    count=quantum-q_pos;

  if(copy_to_user(buf,dptr->data[s_pos]+q_pos,count)){
    retval=-EFAULT;
    goto out;
  }
  //f_posとして次回読み出しの開始位置を更新する
  *f_pos+=count;
  retval=count;

 out:
  mutex_unlock(&dev->sem);
  return retval;
}

int scull_major=300;
int scull_minor=0;
ssize_t scull_write(struct file *filp,const char __user *buf,size_t count,loff_t *f_pos){
  struct scull_dev *dev=filp->private_data;
  struct scull_qset *dptr;
  int quantum=dev->quantum,qset=dev->qset;
  int itemsize=quantum*qset;
  int item,s_pos,q_pos,rest;
  ssize_t retval= -ENOMEM;//値はgoto out文で使う

  if (mutex_lock_interruptible(&dev->sem))
    return -ERESTARTSYS;
  //f_posとして書き出し開始位置が渡ってくる
  //リストアイテムとqsetのインデックスを求め、量子の中でずらす
  item=(long)*f_pos/itemsize;
  rest=(long)*f_pos%itemsize;
  s_pos=rest/quantum;q_pos=rest%quantum;

  //正しい位置までリストをたぐる
  dptr=scull_follow(dev,item);
  if(dptr==NULL)
    goto out;
  if(!dptr->data){
    dptr->data=kmalloc(qset*sizeof(char*),GFP_KERNEL);
    if(!dptr->data)
      goto out;
    memset(dptr->data,0,qset*sizeof(char*));
  }
  if(!dptr->data[s_pos]){
    dptr->data[s_pos]=kmalloc(quantum,GFP_KERNEL);
    if(!dptr->data[s_pos])
      goto out;
  }
  //この量子の終わりまでしか書かない
  if(count>quantum-q_pos)
    count=quantum-q_pos;
  if(copy_from_user(dptr->data[s_pos]+q_pos,buf,count)){
    retval=-EFAULT;
    goto out;
  }
  *f_pos+=count;
  retval=count;

  //サイズを更新する
  if(dev->size<*f_pos){
    dev->size=*f_pos;
  }
 out:
  mutex_unlock(&dev->sem);
  return retval;
}

int scull_release(struct inode *inode,struct file *filp){
       return 0;
}

struct file_operations scull_fops={
  .owner = THIS_MODULE,
  //.llseek=scull_llseek,
  .read   = scull_read,
  .write  = scull_write,
  //.ioctl=scull_ioctl,
  .open   = scull_open,
  .release= scull_release,
};
static void scull_setup_cdev(struct scull_dev *dev,int index){
       int err,devno=MKDEV(scull_major,scull_minor+index);
       cdev_init(&dev->cdev,&scull_fops);
       dev->cdev.owner=THIS_MODULE;
       dev->cdev.ops=&scull_fops;
       err=cdev_add(&dev->cdev,devno,1);
       if(err)
               printk(KERN_NOTICE "Error %d adding scull%d",err,index);
}

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
  scull_major=MAJOR(devid);
  if(ret){
    printk("register_chrdev error %d(%pe)\n",ret,ERR_PTR(ret));
    goto error;
  }
  printk("%s:Major number %d was assigned\n",__func__,g_major_num);
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
  /* cdev_init(&my_cdev,&sample_fops); */
  /* // cdev構造体をカーネルに登録する */
  /* ret=cdev_add(&my_cdev,devid,SAMPLE_MINOR_COUNT); */
  //scull_setup_cdev(struct scull_dev *dev,int index){
  int scull_nr_devs=1;
  int result;
  struct scull_dev *scull_devices = kmalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);
  if (!scull_devices) {
    result = -ENOMEM;
    goto error;  /* Make this more graceful */
  }
  memset(scull_devices, 0, scull_nr_devs * sizeof(struct scull_dev));

  /* Initialize each device. */
  for (int i = 0; i < scull_nr_devs; i++) {
    scull_devices[i].quantum = scull_quantum;
    scull_devices[i].qset = scull_qset;
    //init_MUTEX(&scull_devices[i].sem);
    mutex_init(&scull_devices[i].sem);
    scull_setup_cdev(&scull_devices[i], i);
  }
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

  /* waitキューの初期化 */
  init_waitqueue_head(&g_read_wait);
  /* バッファの初期化 */
  g_buffer_count = 0;

  int err;

  kthread_init_worker(&kworker);
  thread=kthread_run(kthread_worker_fn,&kworker,"samplethread2");
  //thread = kthread_run(do_thread,&err,"samplethread");
  if(IS_ERR(thread)){
    err=PTR_ERR(thread);
    thread=NULL;
    printk("unable to create kernel thread %i\n",err);
    ret=err;
    goto error;
  }
  kthread_init_work(&workdata.do_work,sample_do_proc);

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


module_driver(he_drv,hello_init,hello_exit);
//module_driver(he_drv,hello_init,hello_exit);
