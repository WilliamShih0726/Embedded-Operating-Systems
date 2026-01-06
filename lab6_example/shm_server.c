/*
* shm_server.c -- creates the string and shared memory.
*/
#include <stdio.h>      // 提供輸出入函式，如 printf
#include <stdlib.h>     // 提供 exit()
#include <sys/ipc.h>    // 提供 key_t、IPC 機制
#include <sys/shm.h>    // 提供共享記憶體函式
#include <sys/types.h>  // 定義系統型別（如 pid_t）
#include <unistd.h>     // 提供 sleep()

#define SHMSZ 27        // 要建立的共享記憶體大小為 27 bytes

int main(int argc, char *argv[])
{
    char c;
    int shmid;          // 儲存共享記憶體段的 ID
    key_t key;          // 用來識別共享記憶體段的 key
    char *shm, *s;      // shm：指向共享記憶體的指標；s：字元寫入時用的走訪指標
    int retval;

    /* We'll name our shared memory segment "5678" */
    key = 5678;  // 指定一個固定的 key 作為共享記憶體的識別碼

    /* Create the segment */
    if ((shmid = shmget(key, SHMSZ, IPC_CREAT | 0666)) < 0) {
        // 建立共享記憶體段，大小為 SHMSZ，權限為 0666（任何人都可讀寫）
        perror("shmget");  // 建立失敗時顯示錯誤原因
        exit(1);
    }

    /* Now we attach the segment to our data space */
    if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
        // 將共享記憶體附加（attach）到目前行程的位址空間
        perror("shmat");
        exit(1);
    }

    printf("Server create and attach the share memory.\n");

    /* Now put some things into the memory for the other process to read */
    s = shm;  // 使用指標 s 寫入共享記憶體
    printf("Server write a ~ z to share memory.\n");
    for (c = 'a'; c <= 'z'; c++)
        *s++ = c;  // 將 'a' 到 'z' 寫入共享記憶體
    *s = '\0';     // 最後加上字串結尾符號，變成 C 字串

    /*
    * Finally, we wait until the other process changes the first
    * character of our memory to '*', indicating that it has read
    * what we put there.
    */
    printf("Waiting other process read the share memory ...\n");
    while (*shm != '*')  // 持續檢查共享記憶體第一個字元是否變成 '*'
        sleep(1);        // 若不是則等待 1 秒繼續檢查
    printf("Server read * from the share memory.\n");

    /* Detach the share memory segment */
    shmdt(shm);  // 將共享記憶體 detach（從本行程的位址空間移除）

    /* Destroy the share memory segment */
    printf("Server destroy the share memory.\n");
    retval = shmctl(shmid, IPC_RMID, NULL);  // 刪除共享記憶體段
    if (retval < 0)
    {
        fprintf(stderr, "Server remove share memory failed\n");
        exit(1);  // 如果刪除失敗就退出
    }

    return 0;
}
