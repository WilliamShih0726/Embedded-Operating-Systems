/*
* shm_client.c -- attaches itself to the created shared memory
* and uses the string (printf).
*/
#include <stdio.h>      // 提供 printf、perror 等輸出功能
#include <stdlib.h>     // 提供 exit()
#include <sys/ipc.h>    // 提供 IPC 相關型別與函數
#include <sys/shm.h>    // 提供共享記憶體函數
#include <sys/types.h>  // 提供一些標準系統型別（如 pid_t）

#define SHMSZ 27        // 共享記憶體大小為 27 bytes（與 server 相同）

int main(int argc, char *argv[])
{
    int shmid;          // 儲存共享記憶體段的 ID
    key_t key;          // 用來識別共享記憶體的 key
    char *shm, *s;      // shm 為共享記憶體指標，s 用於走訪共享記憶體內容

    /* We need to get the segment named "5678", created by the server */
    key = 5678;  // 取得與 server 相同的 key（必須相同才能連上）

    /* Locate the segment */
    if ((shmid = shmget(key, SHMSZ, 0666)) < 0) {
        // 嘗試取得已存在的共享記憶體段（權限設定為可讀寫）
        perror("shmget");  // 若失敗，印出錯誤訊息
        exit(1);
    }

    /* Now we attach the segment to our data space */
    if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
        // 將共享記憶體附加到目前行程的位址空間
        perror("shmat");
        exit(1);
    }

    printf("Client attach the share memory created by server.\n");

    /* Now read what the server put in the memory */
    printf("Client read characters from share memory ...\n");
    for (s = shm; *s != '\0'; s++)
        putchar(*s);  // 將共享記憶體中的內容逐字元印出（直到遇到 '\0'）
    putchar('\n');

    /*
    * Finally, change the first character of the segment to '*',
    * indicating we have read the segment.
    */
    printf("Client write * to the share memory.\n");
    *shm = '*';  // 將共享記憶體的第一個字元改成 '*'，通知 server「我讀完了」

    /* Detach the share memory segment */
    printf("Client detach the share memory.\n");
    shmdt(shm);  // 將共享記憶體 detach（從行程中移除連結）

    return 0;
}
