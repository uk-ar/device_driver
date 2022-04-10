#include <linux/module.h>

MODULE_LICENSE("Dual BSD/GPL");
#define DRIVER_NAME "hello"
#define DRIVER_MAJOR 63

extern void sub(void);

static int hello_open(struct inode *inode,struct file *file){
  printk(KERN_ALERT "driver opened\n");
  return 0;
}

static int hello_close(struct inode *inode,struct file *file){
  printk(KERN_ALERT "driver closed\n");
  return 0;
}

static int hello_read(struct file *file,char __user *buf,size_t count,loff_t *f_pos){
  printk(KERN_ALERT "driver read\n");
  buf[0]='A';
  return 1;//actual size
}

static int hello_write(struct file *file,char __user *buf,size_t count,loff_t *f_pos){
  printk(KERN_ALERT "driver wrote\n");
  return 1;//actual size
}

struct file_operations s_hello_fops={
  .open    = hello_open,
  .release = hello_close,
  .read    = hello_read,
  .write   = hello_write,
}

static int hello_init(void){
  printk(KERN_ALERT "driver loaded\n");
  register_chrdev(DRIVER_MAJOR,DRIVER_NAME,&s_hello_fops);
  return 0;
}

static void hello_exit(void){
  printk(KERN_ALERT "driver unloaded\n");
  unregister_chrdev(DRIVER_MAJOR,DRIVER_NAME);
}

module_init(hello_init);
module_exit(hello_exit);
