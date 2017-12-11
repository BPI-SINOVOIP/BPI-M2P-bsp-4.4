#ifndef __ILITEK_DRV_ILIPROC_H__
#define __ILITEK_DRV_ILIPROC_H__

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
long ilitek_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#else
int  ilitek_file_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
#endif
ssize_t ilitek_file_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
ssize_t ilitek_file_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
int ilitek_file_close(struct inode *inode, struct file *filp);
int ilitek_file_open(struct inode *inode, struct file *filp);

#endif //__ILITEK_DRV_ILIPROC_H__
