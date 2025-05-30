#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/atomic.h>

static int default_capacity = 16;
module_param(default_capacity, int, 0644);
MODULE_PARM_DESC(default_capacity, "Default initial capacity of the integer buffer");

static int enable_auto_resize = 0;
module_param(enable_auto_resize, int, 0644);
MODULE_PARM_DESC(enable_auto_resize, "Enable automatic resizing when buffer is full (0=disabled, 1=enabled)");

#define INT_BUFFER_MAGIC 's'
#define INT_STACK_SET_MAX_SIZE _IOW(INT_BUFFER_MAGIC, 1, int)
#define CMD_GET_CAPACITY _IOR(INT_BUFFER_MAGIC, 2, int)
#define CMD_GET_USAGE _IOR(INT_BUFFER_MAGIC, 3, int)
#define CMD_CLEAR_BUFFER _IO(INT_BUFFER_MAGIC, 4)

struct buffer_stats {
    atomic_t push_count;
    atomic_t pop_count;
    atomic_t overflow_count;
    atomic_t underflow_count;
};

struct integer_buffer {
    int *elements;
    size_t capacity;
    size_t position;
    struct mutex op_lock;
    struct buffer_stats stats;
};

static struct integer_buffer *dev_buffer;

static int buffer_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int buffer_release(struct inode *inode, struct file *file)
{
    return 0;
}

static int resize_buffer(size_t new_capacity)
{
    int *new_array;
    size_t copy_size;
    
    if (new_capacity == 0) {
        if (dev_buffer->elements) {
            kfree(dev_buffer->elements);
            dev_buffer->elements = NULL;
        }
        dev_buffer->capacity = 0;
        dev_buffer->position = 0;
        return 0;
    }

    new_array = kzalloc(sizeof(int) * new_capacity, GFP_KERNEL);
    if (!new_array)
        return -ENOMEM;
        
    if (dev_buffer->elements && dev_buffer->position > 0) {
        copy_size = min(dev_buffer->position, new_capacity);
        memcpy(new_array, dev_buffer->elements, sizeof(int) * copy_size);
        
        kfree(dev_buffer->elements);
        
        dev_buffer->position = copy_size;
    } else {
        dev_buffer->position = 0;
    }
    
    dev_buffer->elements = new_array;
    dev_buffer->capacity = new_capacity;
    
    return 0;
}

static long buffer_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int result = 0;
    int value = 0;
    
    mutex_lock(&dev_buffer->op_lock);
    
    switch (cmd) {
    case INT_STACK_SET_MAX_SIZE:
        if (copy_from_user(&value, (int __user *)arg, sizeof(int))) {
            result = -EFAULT;
            break;
        }
        
        if (value < 0) {
            result = -EINVAL;
            break;
        }
        
        result = resize_buffer(value);
        break;
        
    case CMD_GET_CAPACITY:
        value = dev_buffer->capacity;
        if (copy_to_user((int __user *)arg, &value, sizeof(int)))
            result = -EFAULT;
        break;
        
    case CMD_GET_USAGE:
        value = dev_buffer->position;
        if (copy_to_user((int __user *)arg, &value, sizeof(int)))
            result = -EFAULT;
        break;
        
    case CMD_CLEAR_BUFFER:
        dev_buffer->position = 0;
        break;
        
    default:
        result = -ENOTTY;
    }
    
    mutex_unlock(&dev_buffer->op_lock);
    return result;
}

static ssize_t buffer_read(struct file *file, char __user *user_buffer, 
                          size_t count, loff_t *offset)
{
    int value;
    
    if (count < sizeof(int))
        return -EINVAL;
        
    mutex_lock(&dev_buffer->op_lock);
    
    if (dev_buffer->position == 0) {
        atomic_inc(&dev_buffer->stats.underflow_count);
        mutex_unlock(&dev_buffer->op_lock);
        return 0; 
    }
    
    dev_buffer->position--;
    value = dev_buffer->elements[dev_buffer->position];
    

    if (copy_to_user(user_buffer, &value, sizeof(int))) {
        dev_buffer->position++;
        mutex_unlock(&dev_buffer->op_lock);
        return -EFAULT;
    }
    
    atomic_inc(&dev_buffer->stats.pop_count);
    mutex_unlock(&dev_buffer->op_lock);
    
    return sizeof(int);
}

static ssize_t buffer_write(struct file *file, const char __user *user_buffer,
                           size_t count, loff_t *offset)
{
    int value;
    int result;
    
    if (count != sizeof(int))
        return -EINVAL;
        
    if (copy_from_user(&value, user_buffer, sizeof(int)))
        return -EFAULT;
        
    mutex_lock(&dev_buffer->op_lock);
    
    if (dev_buffer->position >= dev_buffer->capacity) {
        if (enable_auto_resize) {
            /* Double the capacity if auto-resize is enabled */
            size_t new_capacity = max(dev_buffer->capacity * 2, 8UL);
            result = resize_buffer(new_capacity);
            if (result < 0) {
                atomic_inc(&dev_buffer->stats.overflow_count);
                mutex_unlock(&dev_buffer->op_lock);
                return -ENOSPC;
            }
        } else {
            atomic_inc(&dev_buffer->stats.overflow_count);
            mutex_unlock(&dev_buffer->op_lock);
            return -ENOSPC;
        }
    }
    
    dev_buffer->elements[dev_buffer->position++] = value;
    atomic_inc(&dev_buffer->stats.push_count);
    
    mutex_unlock(&dev_buffer->op_lock);
    return sizeof(int);
}

static const struct file_operations buffer_fops = {
    .owner = THIS_MODULE,
    .open = buffer_open,
    .release = buffer_release,
    .read = buffer_read,
    .write = buffer_write,
    .unlocked_ioctl = buffer_ioctl,
    .compat_ioctl = buffer_ioctl,  /* For 32bit userspace on 64bit kernel */
};

static struct miscdevice buffer_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "int_stack",
    .fops = &buffer_fops,
    .mode = 0666, 
};

static void init_stats(struct buffer_stats *stats)
{
    atomic_set(&stats->push_count, 0);
    atomic_set(&stats->pop_count, 0);
    atomic_set(&stats->overflow_count, 0);
    atomic_set(&stats->underflow_count, 0);
}

static int __init integer_buffer_init(void)
{
    int result;
    
    dev_buffer = kzalloc(sizeof(struct integer_buffer), GFP_KERNEL);
    if (!dev_buffer)
        return -ENOMEM;
        
    dev_buffer->elements = NULL;
    dev_buffer->capacity = 0;
    dev_buffer->position = 0;
    mutex_init(&dev_buffer->op_lock);
    init_stats(&dev_buffer->stats);
    
    result = misc_register(&buffer_device);
    if (result < 0) {
        kfree(dev_buffer);
        return result;
    }
    
    if (default_capacity > 0) {
        mutex_lock(&dev_buffer->op_lock);
        result = resize_buffer(default_capacity);
        mutex_unlock(&dev_buffer->op_lock);
        
        if (result < 0) {
            misc_deregister(&buffer_device);
            kfree(dev_buffer);
            return result;
        }
    }
    
    printk(KERN_INFO "int_stack: initialized with capacity=%zu\n", 
           dev_buffer->capacity);
    return 0;
}

static void __exit integer_buffer_exit(void)
{
    printk(KERN_INFO "int_stack: usage stats: pushed=%d, popped=%d, overflows=%d, underflows=%d\n",
           atomic_read(&dev_buffer->stats.push_count),
           atomic_read(&dev_buffer->stats.pop_count),
           atomic_read(&dev_buffer->stats.overflow_count),
           atomic_read(&dev_buffer->stats.underflow_count));
           
    misc_deregister(&buffer_device);
    
    if (dev_buffer) {
        if (dev_buffer->elements)
            kfree(dev_buffer->elements);
        mutex_destroy(&dev_buffer->op_lock);
        kfree(dev_buffer);
    }
}

module_init(integer_buffer_init);
module_exit(integer_buffer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ivan Platonov");
MODULE_DESCRIPTION("Integer Buffer Kernel Module - LIFO Storage Device");
MODULE_VERSION("1.0");
