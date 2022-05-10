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
#include "../sample.h"

struct hello_driver {
  struct device_driver driver;
};

static struct class *my_class;
static struct device *my_device;

// キャラクタデバイスオブジェクト
static int minor_count=0;

extern char *sample_devnode(struct device *dev,umode_t *mode);

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

int scull_major=0;
#define SCULL_MINOR 0
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

static DECLARE_WAIT_QUEUE_HEAD(wq);
static int flag=0;

ssize_t sleepy_read(struct file *filp,char __user *buf,size_t count,loff_t *pos){
       printk(KERN_DEBUG "procecc %i (%s) goint to sleep\n",current->pid,current->comm);
       wait_event_interruptible(wq,flag!=0);//競合を起こす可能性あり
       flag=0;
       printk(KERN_DEBUG "awoken %i (%s)\n",current->pid,current->comm);
       return 0;//EOF
}

ssize_t sleepy_write(struct file *filp,const char __user *buf,size_t count,loff_t *pos){
       printk(KERN_DEBUG "process %i (%s) awaking the readers...\n",current->pid,current->comm);
       flag=1;
       wake_up_interruptible(&wq);
       return count;//再試行を避けるため、成功を返す
}

int scull_release(struct inode *inode,struct file *filp){
       return 0;
}

struct scull_pipe{
  wait_queue_head_t inq,outq;//読み書きの待ち列
  char *buffer,*end;//バッファの先頭と末尾
  int buffersize;//ポインタ演算に使用
  char *rp,*wp;//読み出し元、書き込み先
  int nreaders,nwriters;//読み書きごとのオープン回数
  struct fasync_struct *asyc_queue;//非同期的な読み手
  struct mutex sem;//相互排他用のミューテックス
  struct cdev cdev;//キャラクタデバイス構造体
};

static ssize_t scull_p_read(struct file *filp,char __user *buf,size_t count,loff_t *f_pos){
       struct scull_pipe *dev=filp->private_data;

       if(mutex_lock_interruptible(&dev->sem))//devを排他
               return -ERESTARTSYS;

       while(dev->rp == dev->wp){//読むデータがない
               mutex_unlock(&dev->sem);//ロックを解除。devを開放
               if(filp->f_flags & O_NONBLOCK)//ノンブロッキング呼び出し
                       return -EAGAIN;
               PDEBUG("\"%s\" reading: goint to sleep\n",current->comm);
               if(wait_event_interruptible(dev->inq,(dev->rp!=dev->wp)))
                  return -ERESTARTSYS;//シグナル：fs層に依頼を処理
                //それ以外の場合はループ。ただし、最初は再ロック
               if(mutex_lock_interruptible(&dev->sem))//devを排他
                       return -ERESTARTSYS;
       }
        //ok　データが来たので返す処理を行う
        if(dev->wp>dev->rp)
          count = min(count,(size_t)(dev->wp - dev->rp));
        else//書き込みポインタがラップ（折り返し？）されたのでデータをdev->endまで返す
          count = min(count,(size_t)(dev->end - dev->rp));

        if(copy_to_user(buf,dev->rp,count)){
          mutex_unlock(&dev->sem);
          return -EFAULT;
        }

        dev->rp+=count;
        if(dev->rp==dev->end)
          dev->rp=dev->buffer;//ラップ(折り返し？)するから、バッファの先頭に戻す
        mutex_unlock(&dev->sem);

        //最後に書き手を目覚めさせて復帰
        wake_up_interruptible(&dev->outq);
        PDEBUG("\"%s\" did read %li bytes\n",current->comm,(long)count);
        return count;
}

long int scull_ioctl(struct file *filp,unsigned int cmd,unsigned long arg){
       int err=0,tmp;
       int retval=0;
       printk("%s:\n",__func__);
       /* printk("%s:cmd:%u\n",__func__,cmd); */
       /* printk("%s:DIR:%d\n",__func__,_IOC_DIR(cmd)); */
       /* printk("%s:TYPE:%c\n",__func__,_IOC_TYPE(cmd)); */
       /* printk("%s:NR:%d\n",__func__,_IOC_NR(cmd)); */
       /* printk("%s:SIZE:%d\n",__func__,_IOC_SIZE(cmd)); */

       if(_IOC_TYPE(cmd)!=SCULL_IOC_MAGIC){
         return -ENOTTY;
       }

       if(_IOC_NR(cmd)>SCULL_IOC_MAXNR){
         printk("%s:SCULL_IOC_MAXNR\n",__func__);
         return -ENOTTY;
       }

       err=!access_ok((void __user *)arg,_IOC_SIZE(cmd));
       if(err){
         printk("%s:access_ok\n",__func__);
         return -EFAULT;
       }

       switch(cmd){
       case SCULL_IOCRESET:
               scull_quantum=SCULL_QUANTUM;
               scull_qset=SCULL_QSET;
               printk("%s:SCULL_IOCRESET\n",__func__);
               break;
       case SCULL_IOCSQUANTUM:
         /* if(! capable(CAP_SYS_ADMIN)){ */
         /*   printk("%s:CAP_SYS_ADMIN:SCULL_IOCSQUANTUM\n",__func__); */
         /*   return -EPERM; */
         /* } */
         retval=__get_user(scull_quantum,(int __user *)arg);
         printk("%s:SCULL_IOCSQUANTUM:%d\n",__func__,scull_quantum);
         break;
       case SCULL_IOCTQUANTUM:
         /* if(! capable(CAP_SYS_ADMIN)){ */
         /*   printk("%s:CAP_SYS_ADMIN:SCULL_IOCTQUANTUM\n",__func__); */
         /*   return -EPERM; */
         /* } */
         scull_quantum=arg;
         printk("%s:SCULL_IOCTQUANTUM:%d\n",__func__,scull_quantum);
         break;
       case SCULL_IOCGQUANTUM:
               retval=__put_user(scull_quantum,(int __user*)arg);
               printk("%s:SCULL_IOCGQUANTUM:%d\n",__func__,scull_quantum);
               break;
       case SCULL_IOCQQUANTUM:
         printk("%s:SCULL_IOCQQUANTUM:%d\n",__func__,scull_quantum);
         return scull_quantum;
       case SCULL_IOCXQUANTUM:
               /* if(! capable(CAP_SYS_ADMIN)) */
               /*         return -EPERM; */
               tmp=scull_quantum;
               retval=__get_user(scull_quantum,(int __user*)arg);
               if(retval==0)
                 retval=__put_user(tmp,(int __user*)arg);
               printk("%s:SCULL_IOCXQUANTUM:%d\n",__func__,scull_quantum);
               break;
       case SCULL_IOCHQUANTUM:
               /* if(! capable(CAP_SYS_ADMIN)) */
               /*         return -EPERM; */
               tmp=scull_quantum;
               scull_quantum=arg;
               printk("%s:SCULL_IOCHQUANTUM:%d\n",__func__,scull_quantum);
               return tmp;
       default://蛇足すでにcmdはMAXNRで照合されている
               printk("%s:default\n",__func__);
               return -ENOTTY;

       }
       return retval;

}

struct file_operations scull_fops={
  .owner = THIS_MODULE,
  //.llseek=scull_llseek,
  .read   = scull_read,
  .write  = scull_write,
  .unlocked_ioctl=scull_ioctl,
  .open   = scull_open,
  .release= scull_release,
};
static void scull_setup_cdev(struct scull_dev *dev,int index){
       int err,devno=MKDEV(scull_major,SCULL_MINOR+index);
       cdev_init(&dev->cdev,&scull_fops);
       dev->cdev.owner=THIS_MODULE;
       dev->cdev.ops=&scull_fops;
       err=cdev_add(&dev->cdev,devno,1);
       if(err)
               printk(KERN_NOTICE "Error %d adding scull%d",err,index);
}
int scull_nr_devs=1;
struct scull_dev *scull_devices;
int scull_init(struct hello_driver *drv){
  int ret;
  int registered=0;
  int added=0;
  int classed=0;

  dev_t devid;


  printk(KERN_ALERT "hello driver loaded\n");
  // charactor device

  //キャラクターデバイスの空いているメジャー番号を取得する
  ret=alloc_chrdev_region(&devid,SCULL_MINOR,scull_nr_devs,DEVICE_NAME);

  // 獲得したdev_t(メジャー番号+マイナー番号)からメジャー番号を取り出す
  //g_major_num=MAJOR(devid);
  scull_major=MAJOR(devid);
  if(ret){
    printk("register_chrdev error %d(%pe)\n",ret,ERR_PTR(ret));
    goto error;
  }
  printk("%s:Major number %d was assigned\n",__func__,scull_major);
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

  int result;
  scull_devices = kmalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);
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

  for(int i=0;i<scull_nr_devs;i++){
    //クラスを指定してデバイスファイルの作成(/dev/samplehw*)
    my_device=device_create(my_class,NULL,
                            MKDEV(scull_major,SCULL_MINOR+i),
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
    device_destroy(my_class,MKDEV(scull_major,SCULL_MINOR+i));
    printk("%s:device_destroy\n",__func__);
  }
  if(classed){
         //デバイスクラスの破棄<->class_create
    class_destroy(my_class);
  }
  if(registered){
    //キャラクタデバイスの破棄<->cdev_add
    for(int i=0;i<minor_count;i++){
      cdev_del(&scull_devices[i].cdev);
    }
    //キャラクタデバイスの登録解除<->alloc_chrdev_region
    dev_t dev=MKDEV(scull_major,SCULL_MINOR);
    unregister_chrdev_region(dev,scull_nr_devs);
  }
  return ret;
}

void scull_exit(struct hello_driver *drv){
  printk(KERN_ALERT "hello driver unloaded\n");

  //for(int i=0;i<minor_count;i++){
  for (int i = 0; i < scull_nr_devs; i++) {
    device_destroy(my_class,MKDEV(scull_major,SCULL_MINOR+i));
    printk("%s:device_destroy\n",__func__);
  }

  class_destroy(my_class);
  for(int i=0;i<minor_count;i++){
      cdev_del(&scull_devices[i].cdev);
    }
  dev_t dev=MKDEV(scull_major,SCULL_MINOR);
  unregister_chrdev_region(dev,scull_nr_devs);
}
