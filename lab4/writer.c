// writer.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/mydev"//定義裝置檔的路徑。這個檔案對應核心空間的驅動程式 mydev.c 所建立的裝置

int main(int argc, char *argv[]) {
    if (argc != 2) {
        //參數 argv[1] 代表使用者輸入的名字(William)
        fprintf(stderr, "Usage: ./writer <name>\n");
        return 1;
    }

    //使用 open() 開啟 /dev/mydev 作為寫入模式（O_WRONLY）
    int fd = open(DEVICE_PATH, O_WRONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    //取得字串指標與長度，供之後的跑馬燈循環使用
    char *name = argv[1];
    int len = strlen(name);

    //每秒從輸入字串中取出一個字元，寫入驅動裝置中，形成循環跑馬燈效果
    while (1) {
        static int idx = 0;//記住目前送出的字母索引，static 使變數在多次迴圈間保留值
        char ch = name[idx];//取出字串中的第 idx 個字元
        write(fd, &ch, 1);//寫入一個字元到 driver（呼叫的是 mydev.c 中的 write()）
        idx = (idx + 1) % len;//索引遞增，每到尾端就回到開頭（環狀）
        sleep(1);//每秒鐘送出一個字母
    }

    close(fd);
    return 0;
}
//writer.c 送出單一字元 → mydev.c 把字母轉成 16-bit segment 顯示資料 → 儲存在 seg_buf[] 中 → reader.c 讀出並送給 GUI