#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#define	BUF_SIZE 8
#define CDATA_MAJOR 121

#define USE_TIMER 1

struct cdata_t {
	char *buf;
	int idx;
	wait_queue_head_t wait;
	struct timer_list timer;
	struct work_struct work;
};

#ifdef USE_TIMER
void flush_buffer_timer(unsigned long arg);
void flush_buffer_timer(unsigned long arg)
{
	struct cdata_t *cdata = (struct cdata_t *)arg;

	cdata->buf[BUF_SIZE-1] = '\0';
	printk(KERN_INFO "buf = %s\n", cdata->buf);

	cdata->idx = 0;
	wake_up(&cdata->wait);
}
#else
void flush_buffer(struct work_struct *work);
void flush_buffer(struct work_struct *work)
{
	struct cdata_t *cdata = container_of(work, struct cdata_t, work);

	cdata->buf[BUF_SIZE-1] = '\0';
	printk(KERN_INFO "buf = %s\n", cdata->buf);

	cdata->idx = 0;
	wake_up(&cdata->wait);
}
#endif

static int cdata_open(struct inode *inode, struct file *filp)
{
	struct cdata_t *cdata;

	printk(KERN_ALERT "cdata in open: filp = %p\n", filp);

	cdata = kmalloc(sizeof(*cdata), GFP_KERNEL);
	cdata->buf = kmalloc(8, GFP_KERNEL);
	cdata->idx = 0;
	init_waitqueue_head(&cdata->wait);
#ifdef USE_TIMER
	init_timer(&cdata->timer);
#else
	INIT_WORK(&cdata->work, flush_buffer);
#endif
	filp->private_data = (void *)cdata;
	
	return 0;
}

static int cdata_close(struct inode *inode, struct file *filp)
{
	struct cdata_t *cdata = (struct cdata_t *)filp->private_data;
	int idx;

	idx = cdata->idx;

	cdata->buf[idx] = '\0';
	printk(KERN_ALERT "data in buffer: %s\n", cdata->buf);

	kfree(cdata->buf);
	kfree(cdata);

	return 0;
}

static ssize_t cdata_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos)
{
	struct cdata_t *cdata = (struct cdata_t *)filp->private_data;
	int idx;
	int i;
#ifdef USE_TIMER
	struct timer_list *timer;
#endif
	 DECLARE_WAITQUEUE(wait, current);

#ifdef USE_TIMER
	timer = &cdata->timer;
#endif
	idx = cdata->idx;

	for (i = 0; i < count; i++) {
		if (idx >= (BUF_SIZE -1 )) {
			// The buffer is full: make a context-switch
			add_wait_queue(&cdata->wait, &wait);
			current->state = TASK_UNINTERRUPTIBLE;
#ifdef USE_TIMER
			timer->expires = 10*HZ;
			timer->function = flush_buffer_timer;
			timer->data = (unsigned long)cdata;
			add_timer(timer);
#else
			schedule_work(&cdata->work);
#endif
			schedule();
			remove_wait_queue(&cdata->wait, &wait);
			idx = cdata->idx;
		}
		copy_from_user(&cdata->buf[idx], &buf[i], 1);
		idx++;
	}

	cdata->idx = idx;

	return 0;
}

static struct file_operations cdata_fops = {	
	open:		cdata_open,
	release:	cdata_close,
	write:		cdata_write
};

static struct miscdevice cdata_miscdev = {
	.minor	= 77,
	.name	= "cdata-misc",
	.fops	= &cdata_fops,
};

static int cdata_plat_probe(struct platform_device *pdev)
{
	int ret;

	ret = misc_register(&cdata_miscdev);
	if (ret < 0) {
	    printk(KERN_ALERT "misc_register failed\n");
	    return -1;
	}

	printk(KERN_ALERT "cdata module: registerd.\n");

	return 0;
}

static int cdata_plat_remove(struct platform_device *pdev)
{
	misc_deregister(&cdata_miscdev);
	printk(KERN_ALERT "cdata module: unregisterd.\n");

	return 0;
}

static struct platform_driver cdata_driver = {
	.probe		= cdata_plat_probe,
	.remove		= cdata_plat_remove,
	.driver		= {
		.name	= "cdata",
		.owner	= THIS_MODULE,
	}
};

static int cdata_init_module(void)
{
  	return platform_driver_register(&cdata_driver);
}

static void cdata_cleanup_module(void)
{
	platform_driver_unregister(&cdata_driver);
}

module_init(cdata_init_module);
module_exit(cdata_cleanup_module);

MODULE_LICENSE("GPL");
