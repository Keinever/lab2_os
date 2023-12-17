#include <linux/kernel.h>
#include <linux/module.h>

static int __init my_module_init(void) {
    pr_info("Hello from my kernel module!\n");
    return 0;
}

static void __exit my_module_exit(void) {
    pr_info("Exiting my kernel module\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple kernel module");
