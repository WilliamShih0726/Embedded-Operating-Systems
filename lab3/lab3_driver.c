/*
****************************************************************************
* \file 7seg_driver.c
* \details GPIO-based 7-segment display driver
* \author GPT with user request
* \Tested with Linux raspberrypi 6.1.93-v8+
*****************************************************************************
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

#define NUM_SEGMENTS 7

// GPIO mapping for 7 segments: a, b, c, d, e, f, g
//從單一 GPIO 改為 7 個對應七段顯示器 segment a–g 的 GPIO 腳位陣列
static unsigned int seg_gpios[NUM_SEGMENTS] = {17, 27, 22, 23, 24, 25, 5};

// Segment encoding table for 0–9, ordered a–g, 1 = ON, 0 = OFF
//這個編碼表定義每個數字要點亮哪些 segment
static uint8_t seg_encoding[10][NUM_SEGMENTS] = 
{
    {1, 1, 1, 1, 1, 1, 0}, // 0
    {0, 1, 1, 0, 0, 0, 0}, // 1
    {1, 1, 0, 1, 1, 0, 1}, // 2
    {1, 1, 1, 1, 0, 0, 1}, // 3
    {0, 1, 1, 0, 0, 1, 1}, // 4
    {1, 0, 1, 1, 0, 1, 1}, // 5
    {1, 0, 1, 1, 1, 1, 1}, // 6
    {1, 1, 1, 0, 0, 0, 0}, // 7
    {1, 1, 1, 1, 1, 1, 1}, // 8
    {1, 1, 1, 1, 0, 1, 1}  // 9
};

dev_t dev = 0;
static struct cdev seg_cdev;
static struct class *dev_class;

static int __init seg_driver_init(void);
static void __exit seg_driver_exit(void);

/*************** Driver functions **********************/
static int seg_open(struct inode *inode, struct file *file);
static int seg_release(struct inode *inode, struct file *file);
static ssize_t seg_write(struct file *file, const char __user *buf, size_t len, loff_t *off);
/******************************************************/

//File operation structure
static struct file_operations fops = 
{
    .owner = THIS_MODULE,
    .open = seg_open,
    .release = seg_release,
    .write = seg_write,
};

/*
** This function will be called when we open the Device file
*/
static int seg_open(struct inode *inode, struct file *file) 
{
    pr_info("7-Segment Device opened\n");
    return 0;
}

/*
** This function will be called when we close the Device file
*/
static int seg_release(struct inode *inode, struct file *file) 
{
    pr_info("7-Segment Device closed\n");
    return 0;
}

/*
** This function will be called when we write the Device file
*/
static ssize_t seg_write(struct file *file, const char __user *buf, size_t len, loff_t *off) 
{
    char input;
    if (copy_from_user(&input, buf, 1) != 0)
        return -EFAULT;

    if (input >= '0' && input <= '9') {
        //支援數字字元 '0'~'9'，轉換為對應的 segment 模式顯示
        int digit = input - '0';
        pr_info("Displaying digit: %d\n", digit);
        for (int i = 0; i < NUM_SEGMENTS; i++) {
            gpio_set_value(seg_gpios[i], seg_encoding[digit][i]);
        }
    } else if (input == 'x') {
        //多了 'x' 指令來熄滅每一段
        pr_info("Turning off all segments\n");
        for (int i = 0; i < NUM_SEGMENTS; i++) {
            gpio_set_value(seg_gpios[i], 0);  // OFF
        }
    } else {
        pr_err("Invalid input: expected 0-9 or x\n");
        return -EINVAL;
    }

    return len;
}

/*
** Module Init function
*/
static int __init seg_driver_init(void) 
{
    // Allocate device number
    if (alloc_chrdev_region(&dev, 0, 1, "seg_dev") < 0) {
        pr_err("Failed to allocate device number\n");
        return -1;
    }

    // Create device class
    if ((dev_class = class_create(THIS_MODULE, "seg_class")) == NULL) {
        pr_err("Failed to create class\n");
        goto r_class;
    }

    // Create device file
    if ((device_create(dev_class, NULL, dev, NULL, "seg_device")) == NULL) {
        pr_err("Failed to create device file\n");
        goto r_device;
    }

    // Init cdev
    cdev_init(&seg_cdev, &fops);
    if (cdev_add(&seg_cdev, dev, 1) < 0) {
        pr_err("Failed to add cdev\n");
        goto r_cdev;
    }

    // Init GPIOs
    //迴圈初始化每個 segment 的 GPIO
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        if (!gpio_is_valid(seg_gpios[i])) {
            pr_err("GPIO %d not valid\n", seg_gpios[i]);
            goto r_gpio;
        }
        if (gpio_request(seg_gpios[i], "SEG_GPIO")) {
            pr_err("GPIO %d request failed\n", seg_gpios[i]);
            goto r_gpio;
        }
        gpio_direction_output(seg_gpios[i], 0);  // OFF initially
    }

    pr_info("7-Segment Driver inserted\n");
    return 0;

r_gpio:
    for (int i = 0; i < NUM_SEGMENTS; i++) gpio_free(seg_gpios[i]);
r_cdev:
    device_destroy(dev_class, dev);
r_device:
    class_destroy(dev_class);
r_class:
    unregister_chrdev_region(dev, 1);
    return -1;
}

/*
** Module exit function
*/
static void __exit seg_driver_exit(void) 
{
    //利用迴圈釋放所有 GPIO 並全部設為 OFF
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        gpio_set_value(seg_gpios[i], 0);
        gpio_free(seg_gpios[i]);
    }

    device_destroy(dev_class, dev);
    class_destroy(dev_class);
    cdev_del(&seg_cdev);
    unregister_chrdev_region(dev, 1);
    pr_info("7-Segment Driver removed\n");
}

module_init(seg_driver_init);
module_exit(seg_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("EmbeTronicX <embetronicx@gmail.com>");
MODULE_DESCRIPTION("7-Segment GPIO driver");
MODULE_VERSION("1.0");
