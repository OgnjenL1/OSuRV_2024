
#include <linux/module.h> // module_init(), module_exit()
#include <linux/fs.h> // file_operations
#include <linux/errno.h> // EFAULT
#include <linux/uaccess.h> // copy_from_user(), copy_to_user()

MODULE_LICENSE("Dual BSD/GPL");

#include "include/gpio_stream.h"
#include "gpio.h"


static int gpio_stream_open(struct inode *inode, struct file *filp) {
	return 0;
}

static int gpio_stream_release(struct inode *inode, struct file *filp) {
	return 0;
}

static int16_t pos_cmd[MOTOR_CLTR__N_SERVO];

static ssize_t gpio_stream_write(
	struct file* filp,
	const char *buf,
	size_t len,
	loff_t *f_pos
) {
	uint8_t ch;
	int16_t pc;
	uint32_t dp;
	dir_t dir;
	uint32_t threshold;

	if(copy_from_user((uint8_t*)pos_cmd + *f_pos, buf, len) != 0) {
		return -EFAULT;
	}else{
		for(ch = 0; ch < MOTOR_CLTR__N_SERVO; ch++){
			pc = pos_cmd[ch];

			// Extract direction.
			if(pc >= 0){
				dp = pc;
				dir = CW;
			}else{
				dp = -pc;
				dir = CCW;
			}

			bldc__set_dir(ch, dir);

			// Protection.
			if(dp > 1000){
				dp = 1000;
			}

			// Shift for 1 because 2000 is period.
			threshold = dp << 1;

			pwm__set_threshold(ch, threshold);
		}

		*f_pos += len;
		return len;
	}
}


static ssize_t gpio_stream_read(
	struct file* filp,
	char* buf,
	size_t len,
	loff_t* f_pos
) {
	gpio_stream__read_arg_fb_t a;
	uint8_t ch;

	for(ch = 0; ch < MOTOR_CLTR__N_SERVO; ch++){
#if FAKE_FEEDBACK
		a.pos_fb[ch] = pos_cmd[ch]; // Loop pos cmd back.
#else
		//TODO test
		servo_fb__get_pos_fb(ch, &a.pos_fb[ch]);
#endif
	}
	for(ch = 0; ch < MOTOR_CLTR__N_BLDC; ch++){
		bldc__get_pulse_cnt(ch, &a.pulse_cnt_fb[ch]);
	}

	if(copy_to_user(buf, (uint8_t*)&a + *f_pos, len) != 0){
		return -EFAULT;
	}else{
		*f_pos += len;
		return len;
	}
}


static long gpio_stream_ioctl(
	struct file* filp,
	unsigned int cmd,
	unsigned long arg
) {
	gpio_stream__ioctl_arg_moduo_t a;

	switch(cmd){
		case IOCTL_MOTOR_CLTR_SET_MODUO:
			a = *(gpio_stream__ioctl_arg_moduo_t*)&arg;
			pwm__set_moduo(a.ch, a.moduo);
			break;
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
	servo_fb__exit();
	bldc__exit();
	pwm__exit();
	gpio__exit();
	unregister_chrdev(DEV_MAJOR, DEV_NAME);

	printk(KERN_INFO DEV_NAME": Module removed.\n");
}

int gpio_ctrl_init(void) {
	int r;
	uint8_t ch;

	printk(KERN_INFO DEV_NAME": Inserting module...\n");

	r = register_chrdev(DEV_MAJOR, DEV_NAME, &gpio_stream_fops);
	if(r < 0){
		printk(KERN_ERR DEV_NAME": cannot obtain major number %d!\n", DEV_MAJOR);
		goto exit;
	}

	r = gpio__init();
	if(r){
		goto exit;
	}

	r = pwm__init();
	if(r){
		goto exit;
	}

	// 10us*2000 -> 20ms.
	for(ch = 0; ch < MOTOR_CLTR__N_SERVO; ch++){
		pwm__set_moduo(ch, 1000 << 1);
		pwm__set_threshold(ch, DEFUALT_THRESHOLD << 1);
	}

	r = bldc__init();
	if(r){
		goto exit;
	}

	r = servo_fb__init();
	if(r){
		goto exit;
	}

exit:
	if(r){
		printk(KERN_ERR DEV_NAME": %s() failed with %d!\n", __func__, r);
		gpio_stream_exit();
	}else{
		printk(KERN_INFO DEV_NAME": Inserting module successful.\n");
	}
	return r;
}


module_init(gpio_ctrl_init);
module_exit(gpio_ctrl_exit);
