// writer.c
#include <stdio.h>      // fprintf, perror
#include <stdlib.h>     // atoi, exit
#include <string.h>     // strlen, snprintf
#include <fcntl.h>      // open
#include <unistd.h>     // write, close

#define DEVICE_PATH "/dev/mydev"  // 驅動裝置節點（請確保 driver 有創建這個裝置）

int main(int argc, char *argv[]) {
    // 檢查參數數量是否正確
    if (argc != 3) {
        fprintf(stderr, "Usage: %s [7seg|led] [value]\n", argv[0]);
        return 1;
    }

    // 檢查 mode 是否為 7seg 或 led（保守做法）
    if (strcmp(argv[1], "7seg") != 0 && strcmp(argv[1], "led") != 0) {
        fprintf(stderr, "Invalid mode: %s (must be '7seg' or 'led')\n", argv[1]);
        return 1;
    }

    // 開啟驅動裝置
    int fd = open(DEVICE_PATH, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }

    // 將兩個參數組成字串，例如 "7seg 1140"
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s %s", argv[1], argv[2]);

    // 寫入驅動（透過 driver 的 .write 處理邏輯）
    if (write(fd, buffer, strlen(buffer)) < 0) {
        perror("Failed to write to device");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
