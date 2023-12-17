#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>                 // kmalloc() (функция в ядре Linux,    предназначенная для выделения динамической памяти в ядре)
#include <linux/uaccess.h>              // copy_to/from_user()
#include <linux/proc_fs.h>
#include <stdio.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h> 
#include <linux/mutex.h>

#include <linux/pid.h>
#include <linux/signal.h>
#include <linux/ptrace.h>


#define BUF_SIZE 1024

MODULE_LICENSE("Dual BSD/GPL"); #лицензия, в соответствии с которой распространяется  мой  модуль ядра.
MODULE_DESCRIPTION("Stab linux module for operating system's lab");
MODULE_VERSION("1.0");

static int pid = 1;
static int struct_id = 1;

static struct proc_dir_entry *parent;

/*
** Function Prototypes
*/
static int      __init lab_driver_init(void);
static void     __exit lab_driver_exit(void);


/***************** Procfs Functions *******************/
static int      open_proc(struct inode *inode, struct file *file);
static int      release_proc(struct inode *inode, struct file *file);
static ssize_t  read_proc(struct file *filp, char __user *buffer, size_t length, loff_t * offset);
static ssize_t  write_proc(struct file *filp, const char *buff, size_t len, loff_t * off);

/*
** procfs operation structure
*/
static struct proc_ops proc_fops = {
       .proc_open = open_proc,
       .proc_read = read_proc,
       .proc_write = write_proc,
       .proc_release = release_proc,
       .fasync = fasync_proc, //Используется для уведомлений о событиях асинхронного ввода-вывода
       .llseek = llseek_proc, //Функция установки позиции в файле
};

// task_cputime_atomic

static size_t write_task_cputime_struct(char __user *ubuf) {
   char buf[BUF_SIZE];
   size_t len = 0;
   struct task_struct *task;
   struct cputime cputime;

   read_lock(&tasklist_lock);

   for_each_process(task) {
       cputime = task_cputime(task);
       len += snprintf(buf + len, BUF_SIZE - len, "user_time: %lld\n", cputime.utime);
       len += snprintf(buf + len, BUF_SIZE - len, "system_time: %lld\n", cputime.stime);
       len += snprintf(buf + len, BUF_SIZE - len, "sum_exec_runtime: %llu\n\n", task->start_time);
   }

   read_unlock(&tasklist_lock);

   if (copy_to_user(ubuf, buf, len)) {
       return -EFAULT;
   }

   return len;
}


// syscall_info

static size_t write_syscall_info_struct(char __user *ubuf, struct task_struct *task_struct_ref) {
char buf[BUF_SIZE];
size_t len = 0;

struct syscall_info *syscallInfo = &task_struct_ref->syscall_info;

len += snprintf(buf, BUF_SIZE - len, "syscall_count = %lu\n", syscallInfo->syscall_count);
len += snprintf(buf + len, BUF_SIZE - len, "syscall_error_count = %lu\n", syscallInfo->syscall_error_count);
len += snprintf(buf + len, BUF_SIZE - len, "syscall_success_count = %lu\n", syscallInfo->syscall_success_count);

if (copy_to_user(ubuf, buf, len)) {
return -EFAULT;
}

return len;
}


static DEFINE_MUTEX(procfs_mutex); // Определение мьютекса
static struct fasync_struct *async_queue;

/*
** Эта функция будет вызвана, когда мы ОТКРОЕМ файл procfs
*/

static int open_proc(struct inode *inode, struct file *file)
{
   mutex_lock(&procfs_mutex); // Захватываем мьютекс перед входом в критическую секцию

   // Проверяем, что файл уже не открыт
   if (file->private_data) {
      mutex_unlock(&procfs_mutex); // Освобождаем мьютекс при выходе из критической секции
      return -EBUSY;
   }

   file->private_data = (void *)1; // Устанавливаем метку открытого файла

   printk(KERN_INFO "proc file opened.....\t");
   return 0;
}

/*
** Эта функция будет вызвана, когда мы ЗАКРОЕМ файл procfs
*/

static int release_proc(struct inode *inode, struct file *file)
{
   if (!file->private_data) {
      mutex_unlock(&procfs_mutex); // Освобождаем мьютекс при выходе из критической секции
      return -EINVAL; // Файл не был открыт, что-то пошло не так
   }

   // Отключаем уведомления асинхронного ввода-вывода
   if (async_queue)
      fasync_helper(-1, file, 0, &async_queue);

   file->private_data = NULL; // Сбрасываем метку открытого файла

   mutex_unlock(&procfs_mutex); // Освобождаем мьютекс при выходе из критической секции

   printk(KERN_INFO "proc file released.....\n");
   return 0;
}

/*
** Эта функция будет вызвана, когда процесс добавляется или удаляется из очереди уведомлений
*/
static int fasync_proc(struct file *file, struct file_operations *fops, int on)
{
   return fasync_helper(-1, file, on, &async_queue);
}

/*
** Эта функция будет вызвана, когда мы ПРОЧИТАЕМ файл procfs
*/
static ssize_t read_proc(struct file *filp, char __user *ubuf, size_t count, loff_t *ppos) {

char buf[BUF_SIZE];
int len = 0;
struct task_struct *task_struct_ref = get_pid_task(find_get_pid(pid), PIDTYPE_PID);


printk(KERN_INFO "proc file read.....\n");
if (*ppos > 0 || count < BUF_SIZE){
return 0;
}

if (task_struct_ref == NULL){
len += sprintf(buf,"task_struct for pid %d is NULL. Can not get any information\n",pid);

if (copy_to_user(ubuf, buf, len)){
return -EFAULT; //неправильный адрес
}
*ppos = len;
return len;
}

switch(struct_id){
default:
case 0:
len = write_task_cputime_struct(ubuf);
break;
case 1:
len = write_syscall_info_struct(ubuf, task_struct_ref);
break;
}

*ppos = len;

return len;
}

/*
** Эта функция будет вызвана, когда мы ЗАПИШЕМ в файл procfs
*/


static ssize_t write_proc(struct file *filp, const char __user *ubuf, size_t count, loff_t *ppos) {

int num_of_read_digits, c, a, b;
char buf[BUF_SIZE];


printk(KERN_INFO "proc file wrote.....\n");

if (*ppos > 0 || count > BUF_SIZE){
return -EFAULT;
}

if( copy_from_user(buf, ubuf, count) ) {
return -EFAULT;
}

num_of_read_digits = sscanf(buf, "%d %d", &a, &b);
if (num_of_read_digits != 2){ //два числа (0 или 1 наши структуры и второе - PID)
return -EFAULT;
}

struct_id = a;
pid = b;

c = strlen(buf);
*ppos = c;

return c;
}

/*
** Функция инициализации Модуля (создает файловый интерфейс в `/proc` для предоставления информации пользовательскому пространству в виде файлов)
*/
static int __init lab_driver_init(void) {
   /* Создание директории процесса. Она будет создана в файловой системе "/proc" */
   parent = proc_mkdir("lab",NULL);

   if( parent == NULL )
   {
       pr_info("Error creating proc entry");
       return -1;
   }

   /* Создание записи процесса в разделе "/proc/lab/" */
   proc_create("struct_info", 0666, parent, &proc_fops);

   pr_info("Device Driver Insert...Done!!!\n");
   return 0;
}

/*
** Функция выхода из Модуля
*/
static void __exit lab_driver_exit(void)
{
   /* Удаляет 1 запись процесса */
   //remove_proc_entry("lab/struct_info", parent);

   /* Удаление полностью /proc/lab */
   proc_remove(parent);

   pr_info("Device Driver Remove...Done!!!\n");
}

module_init(lab_driver_init);
module_exit(lab_driver_exit);
