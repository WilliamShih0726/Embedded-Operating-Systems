#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define DEVICE_PATH "/dev/seg_device"//驅動裝置檔路徑，對應 7seg_driver.c 中建立的 /dev/seg_device
#define DELAY_MS 1000  // delay between digits

// delay function (milliseconds)
void delay_ms(int ms) {
    usleep(ms * 1000);
}

//"./7seg_writer 313512009"
//"./7seg_writer"是argv[0]
//"313512009是argv[1]"
int main(int argc, char *argv[]) {
    if (argc < 2) {
        //檢查是否有提供參數（學號），沒有參數就提示用法並結束程式
        //argc < 2代表沒有輸入到學號那部份(argv[1])
        fprintf(stderr, "Usage: %s <student_id>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *student_id = argv[1];

    // check if input is all digits
    for (int i = 0; i < strlen(student_id); i++) {
        if (!isdigit(student_id[i])) {
            //isdigit()來自 <ctype.h>，如果()為數字回傳非零值，內非數字回傳'0'
            fprintf(stderr, "Invalid input: only digits 0-9 allowed (found '%c')\n", student_id[i]);
            return EXIT_FAILURE;
        }
    }

    // open the device file
    int fd = open(DEVICE_PATH, O_WRONLY);
    //open()來自 <fcntl.h>，開啟成功回傳檔案描述符（非負整數Ex3,4...），開啟失敗回傳-1
    if (fd < 0) {
        perror("Failed to open /dev/seg_device");
        return EXIT_FAILURE;
    }

    // send each digit
    for (int i = 0; i < strlen(student_id); i++) {
        char digit = student_id[i];
        if (write(fd, &digit, 1) < 0) {
            //把學號中每個字元（例如 '3'）傳給 driver
            //write()回傳<0代表寫入失敗
            perror("Failed to write digit to device");
            close(fd);
            return EXIT_FAILURE;
        }
        printf("Displayed digit: %c\n", digit);
        delay_ms(DELAY_MS);//每顯示一個數字就等 1 秒，再顯示下一個
    }

    // turn off all segments
    char off = 'x';
    if (write(fd, &off, 1) < 0) {
        //寫入 'x' 給驅動程式，觸發關閉所有 segment 的功能
        //這對應到 driver.c 中的 if (input == 'x') {...}
        perror("Failed to send 'x' to turn off segments");
    } else {
        printf("All segments turned off.\n");
    }

    close(fd);
    return EXIT_SUCCESS;
}
