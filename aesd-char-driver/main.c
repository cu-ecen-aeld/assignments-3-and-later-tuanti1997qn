/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include "aesd_ioctl.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("tuanti1997qn"); /** fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * handle open
     */
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * handle release
     */

    filp->private_data = NULL;
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    struct aesd_dev *dev;
    struct aesd_buffer_entry *entry;
    size_t entry_offset_byte;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * handle read
     */
    if((filp == NULL) || (buf == NULL))
    {
        return -EINVAL;
    }
    dev = filp->private_data;
    if(dev == NULL)
    {
        return -ENODEV;
    }
    if(mutex_lock_interruptible(&dev->aesd_mutex))
    {
        return -EINTR;
    }

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->cir_buff, *f_pos, &entry_offset_byte);
    if(entry == NULL)
    {
        mutex_unlock(&dev->aesd_mutex);
        return retval;
    }

    if(entry->size - entry_offset_byte < count)
    {
        retval = entry->size - entry_offset_byte;
    } else 
    {
        retval = count;
    }

    if(copy_to_user(buf, entry->buffptr + entry_offset_byte, retval))
    {
        retval = -EFAULT;
        mutex_unlock(&dev->aesd_mutex);
        return retval;
    }

    *f_pos += retval;
    mutex_unlock(&dev->aesd_mutex);

    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_dev *dev;
    char *write_buff;
    char *line_to_write;
    ssize_t byte_to_write = count; 
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * handle write
     */
    if (count == 0)
        return 0;
    if((filp == NULL) || (buf == NULL))
    {
        return -EINVAL;
    }

    dev = filp->private_data;
    if(dev == NULL)
    {
        return -ENODEV;
    }

    write_buff = kmalloc(count, GFP_KERNEL);
    if(write_buff == NULL)
    {
        return -ENOMEM;
    }
    
    if(copy_from_user(write_buff, buf, count))
    {
        retval = -EFAULT;
        goto END_WRITE;
    }

    line_to_write = memchr(write_buff, '\n', count);
    if(line_to_write)
    {
        byte_to_write = line_to_write - write_buff + 1;
    }

    if (mutex_lock_interruptible(&dev->aesd_mutex)) {
        retval = -ERESTARTSYS;
        goto END_WRITE;
    }

    dev->buffer_entry.buffptr = krealloc(dev->buffer_entry.buffptr,
                                         dev->buffer_entry.size + byte_to_write,
                                         GFP_KERNEL);
    if (!dev->buffer_entry.buffptr) {
        mutex_unlock(&dev->aesd_mutex);
        retval = -ENOMEM;
        goto END_WRITE;
    }

    memcpy((void *)dev->buffer_entry.buffptr + dev->buffer_entry.size, write_buff, byte_to_write);
    dev->buffer_entry.size += byte_to_write;

    if (line_to_write) {
        const char *overwrote_string = aesd_circular_buffer_add_entry(&dev->cir_buff, &dev->buffer_entry);
        if (overwrote_string)
            kfree(overwrote_string);
        dev->buffer_entry.buffptr = NULL;
        dev->buffer_entry.size = 0;
    }
    
    mutex_unlock(&dev->aesd_mutex);
    *f_pos += count;
    retval = count;
END_WRITE:
    kfree(write_buff);
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
    struct aesd_dev *dev = filp->private_data;
    loff_t file_size = 0;
    int newpos = 0;
    struct aesd_buffer_entry *entry;
    int index = 0;

    PDEBUG("llseek %lld %d", offset, whence);
    if (mutex_lock_interruptible(&dev->aesd_mutex))
        return -EINTR;

    AESD_CIRCULAR_BUFFER_FOREACH(entry,&dev->cir_buff,index) {
        file_size += entry->size;
    }

    newpos = fixed_size_llseek(filp, offset, whence, file_size);
    if (newpos < 0) {
        mutex_unlock(&dev->aesd_mutex);
        return newpos;
    }

    filp->f_pos = newpos;
    
    mutex_unlock(&dev->aesd_mutex);
    return newpos;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_seekto seekto;
    size_t valid_entries;
    size_t offset = 0;
    size_t i;
    
    if (cmd != AESDCHAR_IOCSEEKTO)
        return -ENOTTY;
    
    if (copy_from_user(&seekto, (struct aesd_seekto *)arg, sizeof(seekto)))
        return -EFAULT;
    
    mutex_lock(&dev->aesd_mutex);
    valid_entries = (dev->cir_buff.full) ? AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED : dev->cir_buff.in_offs;
    
    if (seekto.write_cmd >= valid_entries) {
        mutex_unlock(&dev->aesd_mutex);
        PDEBUG("got -einval\n");
        return -EINVAL;
    }
    PDEBUG( "seekto.write_cmd = %d, seekto.write_cmd_offset = %d\n",
           seekto.write_cmd, seekto.write_cmd_offset);
    
    for (i = 0; i < valid_entries; i++) {
        size_t index = (dev->cir_buff.out_offs + i) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        if (i == seekto.write_cmd) {
            if (seekto.write_cmd_offset >= dev->cir_buff.entry[index].size) {
                mutex_unlock(&dev->aesd_mutex);
                return -EINVAL;
            }
            offset += seekto.write_cmd_offset;
            break;
        } else {
            offset += dev->cir_buff.entry[index].size;
        }
    }
    
    PDEBUG("Calculated offset: %zu\n", offset);
    filp->f_pos = offset;
    mutex_unlock(&dev->aesd_mutex);
    return 0;
}


struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek = aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * initialize the AESD specific portion of the device
     */

    aesd_circular_buffer_init(&aesd_device.cir_buff);
    mutex_init(&aesd_device.aesd_mutex);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * cleanup AESD specific poritions here as necessary
     */

    struct aesd_buffer_entry *entry;
    int index = 0;
    AESD_CIRCULAR_BUFFER_FOREACH (entry, &aesd_device.cir_buff, index) {
        kfree(entry->buffptr);
    }
    mutex_destroy(&aesd_device.aesd_mutex);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
