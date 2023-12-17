#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#define PROCFS_MAX_SIZE 1024
#define PROCFS_NAME "test"

static struct proc_dir_entry *our_proc_file;
static char procfs_buffer[PROCFS_MAX_SIZE];
static unsigned long procfs_buffer_size = 0;

static DEFINE_MUTEX(procfs_mutex);

static ssize_t procfile_read(struct file *file, char __user *buffer,
                              size_t buffer_length, loff_t *offset) {
    ssize_t len;
    mutex_lock(&procfs_mutex);
    len = simple_read_from_buffer(buffer, buffer_length, offset, procfs_buffer, procfs_buffer_size);
    mutex_unlock(&procfs_mutex);
    return len;
}

static ssize_t procfile_write(struct file *file, const char __user *buff,
                               size_t len, loff_t *off) {
    mutex_lock(&procfs_mutex);
    if (len > PROCFS_MAX_SIZE)
        len = PROCFS_MAX_SIZE;
    if (copy_from_user(procfs_buffer, buff, len)) {
        mutex_unlock(&procfs_mutex);
        return -EFAULT;
    }
    procfs_buffer_size = len;
    mutex_unlock(&procfs_mutex);
    return len;
}

static const struct proc_ops proc_file_fops = {
    .proc_read = procfile_read,
    .proc_write = procfile_write,
};

static int __init procfs_init(void) {
    our_proc_file = proc_create(PROCFS_NAME, 0644, NULL, &proc_file_fops);
    if (our_proc_file == NULL) {
        pr_alert("Error: Could not initialize /proc/%s\n", PROCFS_NAME);
        return -ENOMEM;
    }
    pr_info("/proc/%s created\n", PROCFS_NAME);
    return 0;
}

static void __exit procfs_exit(void) {
    proc_remove(our_proc_file);
    pr_info("/proc/%s removed\n", PROCFS_NAME);
}

module_init(procfs_init);
module_exit(procfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Simple /proc test module");
