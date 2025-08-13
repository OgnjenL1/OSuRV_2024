
#include <linux/module.h> // module_init(), module_exit()
#include <linux/fs.h> // file_operations
#include <linux/errno.h> // EFAULT
#include <linux/uaccess.h> // copy_from_user(), copy_to_user()

MODULE_LICENSE("Dual BSD/GPL");



#include "include/gpio_ctrl.h"
#include "gpio.h"



static int gpio_stream_open(struct inode *inode, struct file *filp) {
	return 0;
}

static int gpio_stream_release(struct inode *inode, struct file *filp) {
	return 0;
}

gpio_ctrl__stream_pkg_t pkg;
uint8_t rd_val = 0;

static ssize_t gpio_stream_write(
	struct file* filp,
	const char *buf,
	size_t len,
	loff_t *f_pos
) {
	int i;

	printk(KERN_INFO DRV_NAME": %s() len = %d\n", __func__, len);
	for(i = 0; i < len; i++){
		printk(KERN_INFO DRV_NAME": %s() buf[%d] = %d\n", __func__, i, (int)buf[i]);
	}
/*
	if(
		copy_from_user(
			(uint8_t*)&pkg,
			buf,
			len)
			!= 0
		) {
		return -EFAULT;
	}else{

		printk(KERN_INFO DRV_NAME": %s() pkg.gpio_num = %d\n", __func__, pkg.gpio_no);
		printk(KERN_INFO DRV_NAME": %s() pkg.op = %d\n", __func__, pkg.op);
		printk(KERN_INFO DRV_NAME": %s() pkg.wr_val = %d\n", __func__, pkg.wr_val);


		return len;
	}
	*/
	return len;
}


static ssize_t gpio_stream_read(
	struct file* filp,
	char* buf,
	size_t len,
	loff_t* f_pos
) {
	if(copy_to_user(buf, &rd_val, len) != 0){
		return -EFAULT;
	}else{
		return len;
	}
}


static long gpio_stream_ioctl(
	struct file* filp,
	unsigned int cmd,
	unsigned long arg
) {
	switch(cmd){
		//case IOCTL_MOTOR_CLTR_SET_MODUO:
			//TODO
			//break;
		default:
			break;
	}

	return 0;
}

loff_t gpio_stream_llseek(
	struct file* filp,
	loff_t offset,
	int whence
) {
	switch(whence){
		case SEEK_SET:
			filp->f_pos = offset;
			break;
		case SEEK_CUR:
			filp->f_pos += offset;
			break;
		case SEEK_END:
			return -ENOSYS; // Function not implemented.
		default:
			return -EINVAL;
		}
	return filp->f_pos;
}

static struct file_operations gpio_stream_fops = {
	open           : gpio_stream_open,
	release        : gpio_stream_release,
	read           : gpio_stream_read,
	write          : gpio_stream_write,
	unlocked_ioctl : gpio_stream_ioctl,
	llseek         : gpio_stream_llseek
};


void gpio_ctrl_exit(void) {
	gpio__exit();

	unregister_chrdev(DEV_STREAM_MAJOR, DEV_STREAM_NAME);

	printk(KERN_INFO DRV_NAME": Module removed.\n");
}

int gpio_ctrl_init(void) {
	int r;

	printk(KERN_INFO DRV_NAME": Inserting module...\n");

	r = register_chrdev(DEV_STREAM_MAJOR, DEV_STREAM_NAME, &gpio_stream_fops);
	if(r < 0){
		printk(KERN_ERR DRV_NAME": cannot obtain major number %d!\n", DEV_STREAM_MAJOR);
		goto exit;
	}

	r = gpio__init();
	if(r){
		goto exit;
	}

exit:
	if(r){
		printk(KERN_ERR DRV_NAME": %s() failed with %d!\n", __func__, r);
		gpio_ctrl_exit();
	}else{
		printk(KERN_INFO DRV_NAME": Inserting module successful.\n");
	}
	return r;
}


module_init(gpio_ctrl_init);
module_exit(gpio_ctrl_exit);
