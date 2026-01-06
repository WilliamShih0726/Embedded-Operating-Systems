// driver.c - 整合 LED 與 7-segment 顯示的 Raspberry Pi 驅動程式
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/string.h>

#define DEVICE_NAME "mydev"
#define BUF_LEN 64

MODULE_LICENSE("GPL");

static int major;
static char cmd_buf[BUF_LEN];

// 7-segment 的七段控制腳位 (a-g)
static int seg_gpios[] = {17, 27, 22, 23, 24, 25, 26};
#define SEG_GPIO_COUNT 7

// 對應數字 0-9 的七段顯示資料（共陰型：1 代表亮）
static int seg_data[10][7] = {
    {1,1,1,1,1,1,0}, // 0
    {0,1,1,0,0,0,0}, // 1
    {1,1,0,1,1,0,1}, // 2
    {1,1,1,1,0,0,1}, // 3
    {0,1,1,0,0,1,1}, // 4
    {1,0,1,1,0,1,1}, // 5
    {1,0,1,1,1,1,1}, // 6
    {1,1,1,0,0,0,0}, // 7
    {1,1,1,1,1,1,1}, // 8
    {1,1,1,1,0,1,1}  // 9
};

// LED 腳位分配
static int led_gpios[] = {5, 6, 12, 13, 16, 19, 20, 21};
#define LED_GPIO_COUNT 8

static void display_digit(int num) {
    int i;
    for (i = 0; i < SEG_GPIO_COUNT; i++) {
        gpio_set_value(seg_gpios[i], seg_data[num][i]);
    }
}

static void clear_segment(void) {
    int i;
    for (i = 0; i < SEG_GPIO_COUNT; i++) {
        gpio_set_value(seg_gpios[i], 0);
    }
}

static void handle_7seg(int value) {
    int digits[10], count = 0, i;

    if (value == 0) {
        digits[0] = 0;
        count = 1;
    } else {
        while (value > 0 && count < 10) {
            digits[count++] = value % 10;
            value /= 10;
        }
    }

    for (i = count - 1; i >= 0; i--) {
        display_digit(digits[i]);
        msleep(500);
    }
    
    clear_segment(); //結束熄滅所有燈
}

static void handle_led(int count) {
    int i;
    // 限制最多點亮 8 顆
    if (count > LED_GPIO_COUNT) count = LED_GPIO_COUNT;

    for (i = 0; i < count; i++) {
        gpio_set_value(led_gpios[i], 1);
    }
    for (i = count - 1; i >= 0; i--) {
        ssleep(1);
        gpio_set_value(led_gpios[i], 0);
    }
}

static ssize_t dev_write(struct file *file, const char __user *buf, size_t len, loff_t *offset) {
    char mode[16];
    int value;

    if (len >= BUF_LEN) return -EINVAL;
    if (copy_from_user(cmd_buf, buf, len)) return -EFAULT;
    cmd_buf[len] = '\0';

    if (sscanf(cmd_buf, "%s %d", mode, &value) != 2) return -EINVAL;

    if (strcmp(mode, "7seg") == 0) {
        handle_7seg(value);
    } else if (strcmp(mode, "led") == 0) {
        handle_led(value);
    } else {
        printk("[mydev] Unknown command: %s\n", mode);
        return -EINVAL;
    }

    return len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = dev_write
};

static int __init mydev_init(void) {
    int i;
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        printk("[mydev] Failed to register char device\n");
        return major;
    }

    printk("[mydev] Registered with major number %d\n", major);

    // 初始化 GPIO
    for (i = 0; i < SEG_GPIO_COUNT; i++) {
        gpio_request(seg_gpios[i], "seg");
        gpio_direction_output(seg_gpios[i], 0);
    }
    for (i = 0; i < LED_GPIO_COUNT; i++) {
        gpio_request(led_gpios[i], "led");
        gpio_direction_output(led_gpios[i], 0);
    }

    return 0;
}

static void __exit mydev_exit(void) {
    int i;
    unregister_chrdev(major, DEVICE_NAME);
    for (i = 0; i < SEG_GPIO_COUNT; i++) gpio_free(seg_gpios[i]);
    for (i = 0; i < LED_GPIO_COUNT; i++) gpio_free(led_gpios[i]);
    printk("[mydev] Unregistered\n");
}

module_init(mydev_init);
module_exit(mydev_exit);
