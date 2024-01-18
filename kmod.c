#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <net/af_unix.h>
#include <linux/net.h>
#include <net/sock.h>
#include <linux/tcp.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>

#include <linux/sched.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define HAVE_PROC_OPS
#endif

/* Buffer size */
#define PROCFS_MAX_SIZE 1024
/* Name of procfs file */
#define PROCFS_NAME "my_procfs"

/* Struct MACRO */
#define TCP 0
#define UNIX_SOCKETS 1

/* Proc directory */
static struct proc_dir_entry *our_proc_file;

/* Temp buffer */
static char procfs_buffer[PROCFS_MAX_SIZE];

/* Temp length */
static unsigned long procfs_buffer_size = 0;

static int struct_id = 0;

static DEFINE_MUTEX(args_mutex);

//handler for tcp connections
static unsigned int my_hook_func(void *priv, struct sk_buff *skb,
                                 const struct nf_hook_state *state) {
  struct iphdr *ip_header;
  struct tcphdr *tcp_header;

  if (!skb) return NF_ACCEPT;

  ip_header = ip_hdr(skb);
  if (ip_header->protocol == IPPROTO_TCP) {
    tcp_header = tcp_hdr(skb);

    sprintf(procfs_buffer, "tcp\t%s%pI4:%d->%pI4:%d\n\n", procfs_buffer, &ip_header->saddr,
            ntohs(tcp_header->source), &ip_header->daddr,
            ntohs(tcp_header->dest));
  }

  return NF_ACCEPT;
}

static struct nf_hook_ops my_hook_ops = {
    .hook = my_hook_func,
    .pf = NFPROTO_IPV4,
    .hooknum = NF_INET_PRE_ROUTING,
    .priority = NF_IP_PRI_FIRST,
};

void get_tcp_connections(char *procfs_buffer) {
  nf_register_net_hook(&init_net, &my_hook_ops);
  mdelay(3000);
  nf_unregister_net_hook(&init_net, &my_hook_ops);
}

void get_unix_sockets(char *procfs_buffer) {
  struct net *net;
  char result[4096];
  int counter = 0;

  sprintf(result, "%s\nProto\tstate\t\t\ttype\t\tI-Node\t\tPath\n", result);
  for_each_net(net) {
    unsigned int hash;
    struct sock *s;

    for (hash = 0; hash < UNIX_HASH_SIZE; ++hash) {
      sk_for_each(s, &net->unx.table.buckets[hash]) {
        struct unix_sock *u = unix_sk(s);
        int type = s->sk_type;
        int inode = SOCK_INODE(s->sk_socket)->i_ino;

        sprintf(result, "%sunix\t\t%s\t\tCONNECTED\t\t|%d\t\t%s\n", result,
                type == 1 ? "STREAM\t\t" : "DGRAM", inode,
                u->addr->name->sun_path);
      }
    }
  }
  copy_to_user(procfs_buffer, result, 4096);
}


static ssize_t procfile_read(struct file *filePointer, char __user *buffer, size_t buffer_length, loff_t *offset)  {
  if (buffer_length < PROCFS_MAX_SIZE) {
    pr_info("Not enough space in buffer\n");
    return -EFAULT;
  }
  mutex_lock(&args_mutex);
    if (struct_id == TCP) {
      mutex_unlock(&args_mutex);
      get_tcp_connections(buffer);
      return 0;
    }
    if (struct_id == UNIX_SOCKETS) {
      mutex_unlock(&args_mutex);
      get_unix_sockets(buffer);
      return 0;
    }
  }
  mutex_unlock(&args_mutex);
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

#ifdef HAVE_PROC_OPS

static const struct proc_ops proc_file_fops = {
    .proc_read = procfile_read,
    .proc_write = procfile_write,
};

#else

static const struct file_operations proc_file_fops = {
    .read = procfile_read,
    .write = procfile_write,
};

#endif


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
