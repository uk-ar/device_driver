#include <linux/module.h>

MODULE_LICENSE("Dual BSD/GPL");

void sub(void){
  printk("%s: sub() called\n",__func__);
}
