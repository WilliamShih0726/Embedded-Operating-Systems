#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/poll.h>

#define DEVICE_NAME "gpio_keys"
#define BUF_LEN 1

static int keys[] = {17, 27, 22, 23};  // A, B, C, D
static const char key_labels[] = {'A', 'B', 'C', 'D'};
static int irq_numbers[4];
static char key_buf;
static int key_pressed = 0;

static wait_queue_head_t wq;

static irqreturn_t key_irq_handler(int irq, void *dev_id) {
    int i;
    for (i = 0; i < 4; i++) {
        if (irq == irq_numbers[i]) {
            key_buf = key_labels[i];
            key_pressed = 1;
            printk(KERN_INFO "你按下了 %c 鍵\n", key_labels[i]);  // 顯示按鍵資訊
            wake_up_interruptible(&wq);
            break;
        }
    }
    return IRQ_HANDLED;
}

static int dev_open(struct inode *inodep, struct file *filep) {
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    if (wait_event_interruptible(wq, key_pressed))
        return -ERESTARTSYS;

    if (copy_to_user(buffer, &key_buf, 1))
        return -EFAULT;

    key_pressed = 0;
    return 1;
}

static unsigned int dev_poll(struct file *filep, poll_table *wait) {
    poll_wait(filep, &wq, wait);
    return key_pressed ? POLLIN | POLLRDNORM : 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
    .release = dev_release,
    .poll = dev_poll,
};

static int major;

static int __init gpio_keys_init(void) {
    int i, ret;

    init_waitqueue_head(&wq);
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) return major;

    for (i = 0; i < 4; i++) {
        ret = gpio_request_one(keys[i], GPIOF_IN, "gpio_key");
        if (ret) goto fail;

        irq_numbers[i] = gpio_to_irq(keys[i]);
        ret = request_irq(irq_numbers[i], key_irq_handler, IRQF_TRIGGER_FALLING, "gpio_key_irq", NULL);
        if (ret) goto fail;
    }

    printk(KERN_INFO "gpio_keys driver loaded with major %d\n", major);
    return 0;

fail:
    while (--i >= 0) {
        free_irq(irq_numbers[i], NULL);
        gpio_free(keys[i]);
    }
    unregister_chrdev(major, DEVICE_NAME);
    return -1;
}

static void __exit gpio_keys_exit(void) {
    int i;
    for (i = 0; i < 4; i++) {
        free_irq(irq_numbers[i], NULL);
        gpio_free(keys[i]);
    }
    unregister_chrdev(major, DEVICE_NAME);
    printk(KERN_INFO "gpio_keys driver unloaded\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI + 使用者");
MODULE_DESCRIPTION("GPIO Keyboard Driver for A/B/C/D with Print");
module_init(gpio_keys_init);
module_exit(gpio_keys_exit);
