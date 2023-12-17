#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/net.h>
#include <net/sock.h>
#include <net/tcp.h>
#include <net/udp.h>

#define PROCFS_MAX_SIZE 1024
#define PROCFS_NAME "lab2"

static struct proc_dir_entry *our_proc_file;
static char procfs_buffer[PROCFS_MAX_SIZE];
static unsigned long procfs_buffer_size = 0;

static int struct_id = 0;
static DEFINE_MUTEX(args_mutex);

static int write_net_info(char __user *buffer, loff_t *offset, size_t buffer_length) {
    int len = 0;

    if (struct_id == 1) { // TCP
        struct tcp_iter_state iter;
        struct tcp4_iter_state iter4;

        len += sprintf(procfs_buffer, "TCP Connections:\n");

        tcp_for_each_entry(net_generic(sock_net(current->files->f_path.dentry->d_inode->i_sb), tcp_hashinfo), &iter, &iter4) {
            len += sprintf(procfs_buffer + len, "Local Address: %pI4:%d\n", &iter.inode->i_sb, ntohs(iter.inode->i_ino));
            len += sprintf(procfs_buffer + len, "Remote Address: %pI4:%d\n", &iter.inode->i_sb, ntohs(iter.inode->i_ino));
            len += sprintf(procfs_buffer + len, "State: %u\n", tcp_sk_state(tcp_sk(iter.sk)));
            len += sprintf(procfs_buffer + len, "\n");
        }
    } else if (struct_id == 2) { // UDP
        struct udp_iter_state iter;

        len += sprintf(procfs_buffer, "UDP Connections:\n");

        udp_for_each_entry(net_generic(sock_net(current->files->f_path.dentry->d_inode->i_sb), udp_hashinfo), &iter) {
            len += sprintf(procfs_buffer + len, "Local Address: %pI4:%d\n", &iter.inode->i_sb, ntohs(iter.inode->i_ino));
            len += sprintf(procfs_buffer + len, "Remote Address: %pI4:%d\n", &iter.inode->i_sb, ntohs(iter.inode->i_ino));
            len += sprintf(procfs_buffer + len, "\n");
        }
    } else {
        return -EFAULT;
    }

    pr_info("off: %lld len: %d", *offset, len);

    if (*offset >= len) {
        pr_info("Can't copy to user space. By offset\n");
        return -EFAULT;
    }

    if (*offset >= len || copy_to_user(buffer, procfs_buffer, len)) {
        pr_info("Can't copy to user space\n");
        return -EFAULT;
    }

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
    ssize_t result = write_net_info(buffer, offset, buffer_length);
    mutex_unlock(&args_mutex);

    return result;
}

static ssize_t procfile_write(struct file *file, const char __user *buff,
                              size_t len, loff_t *off) {
    int num_of_args, a;

    procfs_buffer_size = len;
    if (procfs_buffer_size > PROCFS_MAX_SIZE)
        procfs_buffer_size = PROCFS_MAX_SIZE;

    if (copy_from_user(procfs_buffer, buff, procfs_buffer_size)) {
        pr_info("Can't copy from user space\n");
        return -EFAULT;
    }

    num_of_args = sscanf(procfs_buffer, "%d", &a);
    if (num_of_args != 1) {
        pr_info("Invalid number of args\n");
        return -EFAULT;
    }

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
    our_proc_file = proc_create(PROCFS_NAME, 0644, NULL, &proc_file_fops);
    if (NULL == our_proc_file) {
        pr_alert("Error: Could not initialize /proc/%s\n", PROCFS_NAME);
        return -ENOMEM;
    }
    pr_info("/proc/%s created\n", PROCFS_NAME);
    return 0;
}

static void __exit procfs2_exit(void) {
    proc_remove(our_proc_file);
    pr_info("/proc/%s removed\n", PROCFS_NAME);
}

module_init(procfs2_init);
module_exit(procfs2_exit);

MODULE_LICENSE("GPL");
