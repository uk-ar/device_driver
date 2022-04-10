#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
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

static int sample_open(struct inode *node,struct file *filp){
  printk("%s entered\n",__func__);
  return 0;
}

static int sample_close(struct inode *node,struct file *filp){
  printk("%s entered\n",__func__);
  return 0;
}

static ssize_t sample_read(struct file *filp,char __user *buf,size_t count,loff_t *f_pos){
  printk("%s entered\n",__func__);
  buf[0]='A';
  return 1;
}

static ssize_t sample_write(struct file *filp,const char __user *buf,size_t count,loff_t *f_pos){
  printk("%s entered\n",__func__);
  return 1;
}



static char *sample_devnode(struct device *dev,umode_t *mode){
  int minor=MINOR(dev->devt);

  printk("%s: %p (%d)\n",__func__,mode,minor);

  if(mode && minor==0){
    //パーミッションの設定
    *mode=0666;
    printk("mode %o\n",*mode);
  }
  return NULL;
}
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
  //ret=register_chrdev(g_major_num,DEVICE_NAME,&sample_fops);
  ret=alloc_chrdev_region(&devid,SAMPLE_MINOR_BASE,SAMPLE_MINOR_COUNT,DEVICE_NAME);
  g_major_num=MAJOR(devid);
  if(ret){
    printk("register_chrdev error %d(%pe)\n",ret,ERR_PTR(ret));
    goto error;
  }
  printk("%s:Major number %d was assigned\n",__func__,g_major_num);
  registered=1;

  static const struct file_operations sample_fops = {
    .open    = sample_open,
    .release = sample_close,
    .read    = sample_read,
    .write   = sample_write,
  };
  cdev_init(&my_cdev,&sample_fops);
  ret=cdev_add(&my_cdev,devid,SAMPLE_MINOR_COUNT);
  if(ret<0){
    printk("cdev_add error %d(%pe)\n",ret,ERR_PTR(ret));
    goto error;
  }
  added=1;

  my_class=class_create(THIS_MODULE,CLASS_NAME);
  if(IS_ERR(my_class)){
    printk("class_create error (%pe)\n",ERR_CAST(my_class));
    ret=PTR_ERR(my_class);
    goto error;
  }
  //所有権を強制する
  my_class->devnode=sample_devnode;
  classed=1;

  for(int i=0;i<SAMPLE_MINOR_COUNT;i++){
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
    device_destroy(my_class,MKDEV(g_major_num,SAMPLE_MINOR_BASE+i));
    printk("%s:device_destroy\n",__func__);
  }
  if(classed){
    class_destroy(my_class);
  }
  if(registered){
    dev_t dev=MKDEV(g_major_num,SAMPLE_MINOR_BASE);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev,SAMPLE_MINOR_COUNT);
  }
  return ret;
}

static void hello_exit(struct hello_driver *drv){

  dev_t dev=MKDEV(g_major_num,SAMPLE_MINOR_BASE);
  printk(KERN_ALERT "hello driver unloaded\n");
  for(int i=0;i<minor_count;i++){
    device_destroy(my_class,MKDEV(g_major_num,SAMPLE_MINOR_BASE+i));
    printk("%s:device_destroy\n",__func__);
  }

  class_destroy(my_class);

  cdev_del(&my_cdev);
  unregister_chrdev_region(dev,SAMPLE_MINOR_COUNT);
}

static struct hello_driver he_drv={
  .driver = {
    .name = MODULE_NAME,
  }
};

module_driver(he_drv,hello_init,hello_exit);
