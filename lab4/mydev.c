// mydev.c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

//定義裝置名稱為 "mydev"，並設定顯示資料的長度為 16
#define DEVICE_NAME "mydev"
#define BUF_LEN 16

MODULE_LICENSE("GPL");

//major：儲存主裝置編號
//seg_buf[]：儲存 segment 狀態（16 個字元，每個是 '0' 或 '1'）
static int major;
static char seg_buf[BUF_LEN] = "0000000000000000";

// 26 letters + 1 default
//定義每個字母（A~Z）對應的 16 段顯示二進位編碼。
//若輸入非英文字母，就會使用最後一個 default。
static const unsigned short seg_for_c[27] = {
    0b1111001100010001, // A
    0b0000011100000101, // b
    0b1100111100000000, // C
    0b0000011001000101, // d
    0b1000011100000001, // E
    0b1000001100000001, // F
    0b1001111100010000, // G
    0b0011001100010001, // H
    0b1100110001000100, // I
    0b1100010001000100, // J
    0b0000000001101100, // K
    0b0000111100000000, // L
    0b0011001110100000, // M
    0b0011001110001000, // N
    0b1111111100000000, // O
    0b1000001101000001, // P
    0b0111000001010000, // q
    0b1110001100011001, // R
    0b1101110100010001, // S
    0b1100000001000100, // T
    0b0011111100000000, // U
    0b0000001100100010, // V
    0b0011001100001010, // W
    0b0000000010101010, // X
    0b0000000010100100, // Y
    0b1100110000100010, // Z
    0b0000000000000000  // default
};

static int dev_open(struct inode *inode, struct file *file) {
    return 0;
}

static int dev_release(struct inode *inode, struct file *file) {
    return 0;
}

//將 seg_buf 中目前儲存的 segment 狀態字串（共 16 字元）傳送到使用者空間。
//若拷貝失敗，回傳錯誤 -EFAULT。
static ssize_t dev_read(struct file *filp, char *buffer, size_t len, loff_t *offset) {
    return copy_to_user(buffer, seg_buf, BUF_LEN) ? -EFAULT : BUF_LEN;
}

static ssize_t dev_write(struct file *filp, const char *buffer, size_t len, loff_t *offset) {
    char ch;
    //從使用者程式接收一個字元
    if (copy_from_user(&ch, buffer, 1)) return -EFAULT;

    //如果是小寫英文字母，轉為大寫
    if (ch >= 'a' && ch <= 'z') ch -= 32; // to uppercase
    int idx = (ch >= 'A' && ch <= 'Z') ? ch - 'A' : 26;

    //查表取得對應的 16-bit segment 代碼
    unsigned short code = seg_for_c[idx];
    for (int i = 0; i < BUF_LEN; i++) {
        //將代碼每一 bit 轉成字元 '0' 或 '1'，儲存到 seg_buf
        seg_buf[BUF_LEN - 1 - i] = (code & (1 << i)) ? '1' : '0';
    }

    return 1;
}

//將上面的函式與這個字符裝置綁定
static struct file_operations fops = {
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
};

static int __init dev_init(void) {
    major = register_chrdev(0, DEVICE_NAME, &fops);
    //使用 register_chrdev() 向核心註冊一個**匿名主編號（major number）**的字元裝置
    if (major < 0) {
        printk(KERN_ALERT "Registering char device failed\n");//這會在你執行 dmesg 時印出major number
        return major;
    }
    printk(KERN_INFO "Registered %s with major number %d\n", DEVICE_NAME, major);
    return 0;
}

static void __exit dev_exit(void) {
    unregister_chrdev(major, DEVICE_NAME);
    printk(KERN_INFO "Unregistered %s\n", DEVICE_NAME);
}

module_init(dev_init);
module_exit(dev_exit);
