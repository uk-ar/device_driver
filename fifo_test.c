#include <linux/fs.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>//compat_ptr_ioctl

#ifdef __KERNEL__
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/uaccess.h>//copy_to_user,copy_from_user
#include <linux/kthread.h>//kthread_work,
#include <linux/delay.h>//msleep_interruptible
#include <linux/compat.h>//compat_ulong_t

#include <linux/mutex.h>//init_MUTEX?
#include <linux/semaphore.h>//init_MUTEX

#include <asm/current.h>
#include <asm/uaccess.h>
#else
struct inode{

};
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);

struct fifo_driver {

};
typedef int umode_t;
#define KERN_NOTICE

struct file {
  void *private_data;
};
#include <sys/types.h>//ssize_t
#define __user
#include <errno.h>

#define ERESTARTSYS 1
#include <stdio.h>
#define printk(...) printf(__VA_ARGS__)
#include <stdlib.h>
#define kmalloc(arg1,arg2) malloc(arg1)
#define mutex_lock_interruptible(arg) (0)
#define copy_to_user(arg1,arg2,arg3) (1)
#define mutex_unlock(arg) (0)
#include <string.h>
#define copy_from_user(dst,src,size) memcpy(dst,src,size),0
#endif
struct fifo_dev {
  struct fifo_node *head;//先頭の量子セットへのポインタ
  int node_size;//現在のノード一つのサイズ
  unsigned long size;//格納されているデータ量
  //struct mutex sem;//相互排他のセマフォ
  //struct cdev cdev;//キャラクタ型デバイス構造体
  int sem;//相互排他のセマフォ
};

struct fifo_node {
  struct fifo_node*next;
  char *data;
};

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
  dev=filp->private_data;
       //dev=container_of(inode->i_cdev,struct fifo_dev,cdev);//fifo_dev->cdevからfifo_devを得る
       printk(KERN_NOTICE "%s:%d\n",__func__,dev->node_size);
       //filp->private_data=dev;//のちのために保存
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

  printk(KERN_NOTICE "%s:[3]:%d:%d\n",__func__,rest,count);

  if(copy_from_user(cur->data+rest,buf,count)){
    printk(KERN_NOTICE "%s:copy error\n",__func__);
    retval=-EFAULT;
    goto out;
  }
  printk(KERN_NOTICE "%s:[4]:copied:%d:%ld\n",__func__,rest,count);

  *f_pos+=count;
  retval=count;
  for(int i=0;i<count;i++){
         printk(KERN_NOTICE "%s:%c\n",__func__,*(cur->data+rest+i));
  }
  /* printk(KERN_NOTICE "%s:%px:%c:%lld:%zd\n",__func__,cur,*(cur->data+rest),*f_pos,retval); */

  // サイズを更新する(いつも更新するわけではないのはseekするから？)
  /* if(dev->size<*f_pos){ */
  /*   dev->size=*f_pos; */
  /* } */
 out:
  mutex_unlock(&dev->sem);
  return retval;
}

int fifo_release(struct inode *inode,struct file *filp){
  printk("%s:",__func__);
  return 0;
}

int main(int argc,char** argv){
  printf("hello\n");
  struct inode inode={0};
  struct file filp={0};
  struct fifo_dev dev={0};
  dev.node_size=10;
  filp.private_data=&dev;
  fifo_open(&inode,&filp);
  //(struct file *filp,const char __user *buf,size_t count,loff_t *f_pos){
  char buf[]="12345678901234";
  loff_t p=0;
  int total=15;
  int r=fifo_write(&filp,buf,total,&p);
  printf("%d:\n",r);
  total-=r;
  while(total){
    r=fifo_write(&filp,buf,total,&p);
    total-=r;
  }
  fifo_release(&inode,&filp);
}
