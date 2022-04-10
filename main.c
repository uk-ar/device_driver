#include <linux/module.h>
#include <linux/device.h>

#define MODULE_NAME "hello_driver"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("This is a hello driver");
MODULE_AUTHOR("Yuuki Arisawa");

static int g_param1 = 100;
module_param(g_param1,int,0);
MODULE_PARM_DESC(g_param1,"Test parameter 1 (default:100)");

static int g_param2 = 200;
module_param(g_param2,int,S_IRUGO);
MODULE_PARM_DESC(g_param2,"Test parameter 2 (default:200)");

static int g_param3 = 300;
module_param(g_param3,int,(S_IRUGO | S_IWUGO) & ~S_IWOTH);
MODULE_PARM_DESC(g_param3,"Test parameter 3 (default:300)");


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
