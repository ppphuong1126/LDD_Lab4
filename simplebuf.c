#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#define DEVICE_NAME "simplebuf"
#define BUFFER_SIZE 128
static char device_buffer[BUFFER_SIZE];
static int data_size = 0;
static int major_number;
static int simplebuf_open(struct inode *inode, struct file *filp)
{
	pr_info("simplebuf: device opened\n");
	return 0;
}
static int simplebuf_release(struct inode *inode, struct file *filp)
{
	pr_info("simplebuf: device released\n");
	return 0;
}
static ssize_t simplebuf_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
	size_t available;
	size_t not_copied;
	size_t copied;
	if(count == 0) return 0;
	if(*ppos <0 || *ppos > (loff_t)BUFFER_SIZE) return -EINVAL;
	if (*ppos == (loff_t)BUFFER_SIZE) return -ENOSPC;
	available = BUFFER_SIZE - (size_t)*ppos;
	if (count > available) count = available;
	not_copied = copy_from_user(device_buffer + (size_t)*ppos, buf, count);
	copied = count - not_copied;
	if (copied == 0) return -EFAULT;
	*ppos += copied;
	if (*ppos > (loff_t)data_size) data_size = (size_t)*ppos;
	pr_info("simplebuf: wrote %zu bytes, offset=%lld, data_size=%d\n",
        copied, (long long)*ppos, data_size);
	return copied;
}
static ssize_t simplebuf_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
    size_t available;
    size_t not_copied;
    size_t copied;
    if (count == 0) return 0;
    if (*ppos < 0 || *ppos > (loff_t)BUFFER_SIZE) return -EINVAL;
    if (*ppos >= (loff_t)data_size) return 0;
    available = data_size - (size_t)*ppos;
    if (count > available) count = available;
    not_copied = copy_to_user(buf, device_buffer + (size_t)*ppos, count);
    copied = count - not_copied;
    if (copied == 0) return -EFAULT;
    *ppos += copied;
    return copied;
}
static loff_t simplebuf_llseek(struct file *filp,
                               loff_t offset,
                               int whence)
{
    loff_t new_pos;

    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;

        case SEEK_CUR:
            new_pos = filp->f_pos + offset;
            break;

        case SEEK_END:
            new_pos = data_size + offset;
            break;

        default:
            return -EINVAL;
    }

    if (new_pos < 0 || new_pos > data_size) return -EINVAL;
    filp->f_pos = new_pos;

    return new_pos;
}
static const struct file_operations simplebuf_fops = {
	.owner = THIS_MODULE,
	.open = simplebuf_open,
	.release = simplebuf_release,
	.write = simplebuf_write,
    .read = simplebuf_read,
	.llseek = simplebuf_llseek,
};
static int __init simplebuf_init(void)
{
	major_number = register_chrdev(0, DEVICE_NAME, &simplebuf_fops);
	if (major_number < 0) {
		pr_err("simplebuf: failed to register device\n");
		return major_number;
	}
	pr_info("simplebuf: driver registered with major number %d\n", major_number);
	return 0;
}
static void __exit simplebuf_exit(void){
	unregister_chrdev(major_number, DEVICE_NAME);
	pr_info("simplebuf: driver unregistered\n");
}
module_init(simplebuf_init);
module_exit(simplebuf_exit);
MODULE_LICENSE("GPL");
