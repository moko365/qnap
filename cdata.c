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
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#define	BUF_SIZE 8
#define CDATA_MAJOR 121

struct cdata_t {
	char *buf;
	int idx;
	wait_queue_head_t wait;
	struct timer_list timer;
	struct mutex write_lock;
};

void flush_buffer(unsigned long arg);

void flush_buffer(unsigned long arg)
{
	struct cdata_t *cdata = (struct cdata_t *)arg;

	cdata->idx = 0;
	wake_up(&cdata->wait);
}

static int cdata_open(struct inode *inode, struct file *filp)
{
	struct cdata_t *cdata;

	printk(KERN_ALERT "cdata in open: filp = %p\n", filp);

	cdata = kmalloc(sizeof(*cdata), GFP_KERNEL);
	cdata->buf = kmalloc(8, GFP_KERNEL);
	cdata->idx = 0;
	init_waitqueue_head(&cdata->wait);
	init_timer(&cdata->timer);
	mutex_init(&cdata->write_lock);

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

	del_timer(&cdata->timer);
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
	DECLARE_WAITQUEUE(wait, current);

	mutex_lock_interruptible(&cdata->write_lock);
	idx = cdata->idx;

	for (i = 0; i < count; i++) {
		if (idx >= (BUF_SIZE -1 )) {
			printk(KERN_ALERT "cdata: buffer full\n");

repeat:
			prepare_to_wait(&cdata->wait, &wait, TASK_INTERRUPTIBLE);
			cdata->timer.function = flush_buffer;
			cdata->timer.data = (unsigned long)cdata;
			cdata->timer.expires = jiffies + 10*HZ;
			add_timer(&cdata->timer);

	mutex_unlock(&cdata->write_lock);
			schedule();
	mutex_lock_interruptible(&cdata->write_lock);

			if (!signal_pending(current))
				return -EINTR;

			idx = cdata->idx;
			if (idx >= (BUF_SIZE - 1)) goto repeat;

			remove_wait_queue(&cdata->wait, &wait);
		}
		copy_from_user(&cdata->buf[idx], &buf[i], 1);

		idx++;
	}

	cdata->idx = idx;
	mutex_unlock(&cdata->write_lock);

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
