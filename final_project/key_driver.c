#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/timer.h>

#define DEVICE_NAME "gpio_keys"
#define NUM_KEYS 4
#define KEY_PRESSED 1
#define KEY_RELEASED 0
#define DEBOUNCE_TIME_MS 50

static int keys[NUM_KEYS] = {17, 27, 22, 23};  // GPIO pins
static const char key_labels[NUM_KEYS] = {'A', 'B', 'C', 'D'};
static int irq_numbers[NUM_KEYS];
static char key_buf;
static atomic_t key_pressed = ATOMIC_INIT(0);
static spinlock_t key_lock;

static wait_queue_head_t wq;

// 自動創建設備節點的變量
static struct class *gpio_keys_class = NULL;
static struct device *gpio_keys_device = NULL;

// 按鍵狀態結構體
struct gpio_key_state {
    int gpio;
    char label;
    struct timer_list timer;
    atomic_t can_report;
};


static struct gpio_key_state key_states[NUM_KEYS];

// 定時器回調函數

static void key_debounce_timer(struct timer_list *t)
{
    struct gpio_key_state *state = from_timer(state, t, timer);

    // 等待使用者「釋放」按鍵（由 Low → High）
    if (gpio_get_value(state->gpio) == 1) {
        key_buf = state->label;
        atomic_set(&key_pressed, 1);
        wake_up_interruptible(&wq);
        printk(KERN_INFO "Key %c released (event triggered)\n", state->label);
        atomic_set(&state->can_report, 1);
    } else {
        // 尚未釋放，再次啟動定時器
        mod_timer(&state->timer, jiffies + msecs_to_jiffies(10));
    }
}





// 初始化每個按鍵的狀態
static void init_key_states(void)
{
    int i;
    for (i = 0; i < NUM_KEYS; i++) {
        key_states[i].gpio = keys[i];
        key_states[i].label = key_labels[i];
        atomic_set(&key_states[i].can_report, 1);
        timer_setup(&key_states[i].timer, key_debounce_timer, 0);
    }
}

// 修改後的中斷處理函數
static irqreturn_t key_irq_handler(int irq, void *dev_id)
{
    int i;
    unsigned long flags;
    
    spin_lock_irqsave(&key_lock, flags);
    
    for (i = 0; i < NUM_KEYS; i++) {
        if (irq == irq_numbers[i]) {
            if (!atomic_read(&key_states[i].can_report)) {
                spin_unlock_irqrestore(&key_lock, flags);
                return IRQ_HANDLED;
            }
            
            
            if (gpio_get_value(keys[i]) == 0) {
                // 不要送 key_buf 和 wake_up
                atomic_set(&key_states[i].can_report, 0);
                mod_timer(&key_states[i].timer, jiffies + msecs_to_jiffies(DEBOUNCE_TIME_MS));
                printk(KERN_INFO "Key %c pressed (start debounce)\n", key_labels[i]);
            }
            
            break;
        }
    }
    
    spin_unlock_irqrestore(&key_lock, flags);
    return IRQ_HANDLED;
}

// 以下保持不變...
static int dev_open(struct inode *inodep, struct file *filep)
{
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
    return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
    if (wait_event_interruptible(wq, atomic_read(&key_pressed)))
        return -ERESTARTSYS;

    if (copy_to_user(buffer, &key_buf, 1))
        return -EFAULT;

    atomic_set(&key_pressed, KEY_RELEASED);
    return 1;
}

static unsigned int dev_poll(struct file *filep, poll_table *wait)
{
    poll_wait(filep, &wq, wait);
    return atomic_read(&key_pressed) ? POLLIN | POLLRDNORM : 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
    .release = dev_release,
    .poll = dev_poll,
};

static int major;

static int __init gpio_keys_init(void)
{
    int i, ret;
    
    init_key_states();
    spin_lock_init(&key_lock);
    init_waitqueue_head(&wq);
    
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        printk(KERN_ERR "Failed to register char device\n");
        return major;
    }

    gpio_keys_class = class_create(THIS_MODULE, "gpio_keys_class");
    if (IS_ERR(gpio_keys_class)) {
        printk(KERN_ERR "Failed to create device class\n");
        ret = PTR_ERR(gpio_keys_class);
        goto err_class;
    }

    gpio_keys_device = device_create(gpio_keys_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
    if (IS_ERR(gpio_keys_device)) {
        printk(KERN_ERR "Failed to create device\n");
        ret = PTR_ERR(gpio_keys_device);
        goto err_device;
    }

    for (i = 0; i < NUM_KEYS; i++) {
        ret = gpio_request_one(keys[i], GPIOF_IN, "gpio_key");
        if (ret) {
            printk(KERN_ERR "Failed to request GPIO %d\n", keys[i]);
            goto err_gpio;
        }

        irq_numbers[i] = gpio_to_irq(keys[i]);
        ret = request_irq(irq_numbers[i], key_irq_handler, 
                         IRQF_TRIGGER_FALLING,  // 只使用下降沿觸發
                         "gpio_key_irq", NULL);
        if (ret) {
            printk(KERN_ERR "Failed to request IRQ for GPIO %d\n", keys[i]);
            gpio_free(keys[i]);
            goto err_irq;
        }
    }

    printk(KERN_INFO "gpio_keys driver loaded with major %d\n", major);
    return 0;

err_irq:
    for (; i >= 0; i--) {
        free_irq(irq_numbers[i], NULL);
        gpio_free(keys[i]);
    }
    
err_gpio:
    device_destroy(gpio_keys_class, MKDEV(major, 0));
err_device:
    class_destroy(gpio_keys_class);
err_class:
    unregister_chrdev(major, DEVICE_NAME);
    return ret;
}

static void __exit gpio_keys_exit(void)
{
    int i;
    
    for (i = 0; i < NUM_KEYS; i++) {
        del_timer_sync(&key_states[i].timer);
        free_irq(irq_numbers[i], NULL);
        gpio_free(keys[i]);
    }
    
    device_destroy(gpio_keys_class, MKDEV(major, 0));
    class_destroy(gpio_keys_class);
    unregister_chrdev(major, DEVICE_NAME);
    
    printk(KERN_INFO "gpio_keys driver unloaded\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI + User");
MODULE_DESCRIPTION("GPIO Keyboard Driver with debounce");
module_init(gpio_keys_init);
module_exit(gpio_keys_exit);