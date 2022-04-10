#include <linux/module.h>
#include <linux/device.h>

#define MODULE_NAME "hello_driver"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("This is a hello driver");
MODULE_AUTHOR("Yuuki Arisawa");

struct hello_driver {
  struct device_driver driver;
};

static int hello_init(struct hello_driver *drv){
  printk(KERN_ALERT "hello driver loaded\n");
  return 0;
}
static void hello_exit(struct hello_driver *drv){
  printk(KERN_ALERT "hello driver unloaded\n");
}

static struct hello_driver he_drv={
  .driver = {
    .name = MODULE_NAME,
  }
};

module_driver(he_drv,hello_init,hello_exit);
