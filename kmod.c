#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/ptrace.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/net.h>

/* Buffer size */
#define PROCFS_MAX_SIZE 1024
/* Name of procfs file */
#define PROCFS_NAME "lab2"

/* Proc directory */
static struct proc_dir_entry *our_proc_file;

/* Temp buffer */
static char procfs_buffer[PROCFS_MAX_SIZE];

/* Temp length */
static unsigned long procfs_buffer_size = 0;

static int struct_id = 0;

static DEFINE_MUTEX(args_mutex);



static int write_tcp_info(char __user *buffer,
                             loff_t *offset, size_t buffer_length) {
  /* Declare structures */
  int len = 0;

  /* Init values */
  
  /* String to input */
  len += sprintf(procfs_buffer,
                 "Hello world") + 1;

  /* Copy string to user space */

  printk("off: %lld len: %d", *offset, len);
  if (*offset >= len) {
    pr_info("Can't copy to user space. By offset\n");
    return -EFAULT;
  }
  if (*offset >= len || copy_to_user(buffer, procfs_buffer, len)) {
    pr_info("Can't copy to user space\n");
    return -EFAULT;
  }

  /* Return value */
  *offset += len;
  return len;
}


static ssize_t procfile_read(struct file *filePointer, char __user *buffer,
                             size_t buffer_length, loff_t *offset) {
  if (buffer_length < PROCFS_MAX_SIZE) {
    pr_info("Not enough space in buffer\n");
    return -EFAULT;
  }
  mutex_lock(&args_mutex);
  if (struct_id == 1) {
    mutex_unlock(&args_mutex);
    return write_tcp_info(buffer, offset, buffer_length);
  }
  if (struct_id == 2) {
    mutex_unlock(&args_mutex);
    return write_tcp_info(buffer, offset, buffer_length); 
  }
  mutex_unlock(&args_mutex);
  return -EFAULT;
}

/* This function calls when user writes to proc file */

static ssize_t procfile_write(struct file *file, const char __user *buff,
                              size_t len, loff_t *off) {
  int num_of_args, a;

  /* We don't need to read more than 1024 bytes */
  procfs_buffer_size = len;
  if (procfs_buffer_size > PROCFS_MAX_SIZE)
    procfs_buffer_size = PROCFS_MAX_SIZE;

  /* Copy data from user space */
  if (copy_from_user(procfs_buffer, buff, procfs_buffer_size)) {
    pr_info("Can't copy from user space\n");
    return -EFAULT;
  }

  /* Read args from input */
  num_of_args = sscanf(procfs_buffer, "%d", &a);
  if (num_of_args != 1) {
    pr_info("Invalid number of args\n");
    return -EFAULT;
  }

  /* Copy args to program */
  mutex_lock(&args_mutex);
  struct_id = a;
  mutex_unlock(&args_mutex);

  pr_info("Struct id is: %d\n", struct_id);

  return procfs_buffer_size;
}

static const struct proc_ops proc_file_fops = {
    .proc_read = procfile_read,
    .proc_write = procfile_write,
};

static int __init procfs2_init(void) {
  /* Init proc file */
  our_proc_file = proc_create(PROCFS_NAME, 0644, NULL, &proc_file_fops);
  if (NULL == our_proc_file) {
    proc_remove(our_proc_file);
    pr_alert("Error:Could not initialize /proc/%s\n", PROCFS_NAME);
    return -ENOMEM;
  }
  pr_info("/proc/%s created\n", PROCFS_NAME);
  return 0;
}

static void __exit procfs2_exit(void) {
  /* Delete proc file */
  proc_remove(our_proc_file);
  pr_info("/proc/%s removed\n", PROCFS_NAME);
}

module_init(procfs2_init);
module_exit(procfs2_exit);

MODULE_LICENSE("GPL");
