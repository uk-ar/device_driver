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

#define SAMPLE_MAJOR 300
#define DEVICE_NAME "devsample"
static int g_major_num=SAMPLE_MAJOR;
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

static const struct file_operations sample_fops = {
  .open = sample_open,
  .release = sample_close,
};

static int hello_init(struct hello_driver *drv){
  int ret=register_chrdev(g_major_num,DEVICE_NAME,&sample_fops);
  printk(KERN_ALERT "hello driver loaded\n");
  return ret;
}

static void hello_exit(struct hello_driver *drv){
  unregister_chrdev(g_major_num,DEVICE_NAME);
  printk(KERN_ALERT "hello driver unloaded\n");
}

static struct hello_driver he_drv={
  .driver = {
    .name = MODULE_NAME,
  }
};

module_driver(he_drv,hello_init,hello_exit);
