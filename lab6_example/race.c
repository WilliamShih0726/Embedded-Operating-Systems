/*
* race.c
* 本程式模擬多 process 同時存取檔案進行加總計數的情況，
* 可選擇是否使用 semaphore 來保護 critical section。
*/

#include <errno.h>      // 處理錯誤訊息用
#include <fcntl.h>      // 提供 open() 所需 flag 定義
#include <stdio.h>      // 標準輸入輸出
#include <stdlib.h>     // exit(), atoi(), malloc() 等
#include <string.h>     // 處理字串
#include <sys/ipc.h>    // IPC 用於 semaphore
#include <sys/sem.h>    // System V semaphore 函式
#include <sys/stat.h>   // 權限定義
#include <sys/types.h>  // 提供 pid_t 等型別
#include <sys/wait.h>   // waitpid()
#include <unistd.h>     // 提供 fork(), read(), write(), close() 等函式

#ifdef USE_SEM
#define SEM_MODE 0666           /* 設定 semaphore 權限 rw-rw-rw- */
#define SEM_KEY 1122334455      /* 設定 semaphore 的唯一 key */
int sem;                        // 儲存 semaphore ID

/* P () - semaphore wait 操作，進入 critical section 前呼叫 */
int P (int s)
{
    struct sembuf sop;         /* semaphore 操作的參數結構 */
    sop.sem_num = 0;           /* 使用第 0 個（唯一一個）semaphore */
    sop.sem_op = -1;           /* -1 表示減 1（若為 0 則會阻塞） */
    sop.sem_flg = 0;           /* 無特殊旗標 */
    if (semop (s, &sop, 1) < 0) {
        fprintf(stderr,"P(): semop failed: %s\n",strerror(errno));
        return -1;
    } else {
        return 0;
    }
}

/* V () - semaphore signal 操作，離開 critical section 時呼叫 */
int V(int s)
{
    struct sembuf sop;         /* semaphore 操作的參數結構 */
    sop.sem_num = 0;           /* 使用第 0 個 semaphore */
    sop.sem_op = 1;            /* +1 表示釋放資源 */
    sop.sem_flg = 0;           /* 無特殊旗標 */
    if (semop(s, &sop, 1) < 0) {
        fprintf(stderr,"V(): semop failed: %s\n",strerror(errno));
        return -1;
    } else {
        return 0;
    }
}
#endif

/* increment value saved in file
 * 此函式會讀取 counter.txt 的值，+1 後寫回，每次執行 10000 次。
 */
void Increment()
{
    int ret;
    int fd;                     /* 檔案描述子 */
    int counter;                /* 整數計數器 */
    char buffer[100];           /* 暫存字串 */
    int i = 10000;              /* 要加總的次數 */

    while(i)
    {
        /* open file */
        fd = open("./counter.txt", O_RDWR ); // 以可讀寫方式開啟 counter.txt
        if (fd < 0)
        {
            printf("Open counter.txt error.\n");
            exit(-1);
        }

#ifdef USE_SEM
        /* acquire semaphore */
        P(sem);                 // 若啟用 semaphore，這裡進行鎖定
#endif

/**************** Critical Section *****************/
        /* clear */
        memset(buffer, 0, 100); // 清空 buffer

        /* read raw data from file */
        ret = read(fd, buffer, 100); // 讀取目前檔案內容（預期是整數字串）
        if (ret < 0)
        {
            perror("read counter.txt");
            exit(-1);
        }

        /* transfer string to integer & increment counter */
        counter = atoi(buffer); // 將字串轉成整數
        counter++;              // 加 1

        /* write back to counter.txt */
        lseek(fd, 0, SEEK_SET); // 將檔案指標移回開頭
        memset(buffer, 0, 100); // 清空 buffer
        sprintf(buffer, "%d", counter); // 將新值轉為字串寫回去
        ret = write(fd, buffer, strlen(buffer));
        if (ret < 0)
        {
            perror("write counter.txt");
            exit(-1);
        }
/**************** Critical Section *****************/

#ifdef USE_SEM
        /* release semaphore */
        V(sem);                 // 解鎖 semaphore，釋放臨界區
#endif

        /* close file */
        close(fd);              // 關閉檔案
        i--;                    // 重複 10000 次
    }
}

int main(int argc, char **argv)
{
    int childpid;
    int status;

#ifdef USE_SEM
    /* create semaphore */
    sem = semget(SEM_KEY, 1, IPC_CREAT | IPC_EXCL | SEM_MODE); // 建立一個新的 semaphore
    if (sem < 0)
    {
        fprintf(stderr, "Sem %ld creation failed: %s\n", SEM_KEY,
                strerror(errno));
        exit(-1);
    }

    /* initial semaphore value to 1 (binary semaphore) */
    if ( semctl(sem, 0, SETVAL, 1) < 0 )
    {
        fprintf(stderr, "Unable to initialize Sem: %s\n", strerror(errno));
        exit(0);
    }

    printf("Semaphore %ld has been created & initialized to 1\n", SEM_KEY);
#endif

    /* fork process */
    if ((childpid = fork()) > 0) /* parent */
    {
        Increment();                         // 父 process 執行加總
        waitpid(childpid, &status, 0);       // 等待子 process 結束
    }
    else if (childpid == 0) /* child */
    {
        Increment();                         // 子 process 執行加總
        exit(0);
    }
    else /* error */
    {
        perror("fork");                      // fork 失敗
        exit(-1);
    }

#ifdef USE_SEM
    /* remove semaphore */
    if (semctl (sem, 0, IPC_RMID, 0) < 0)
    {
        fprintf (stderr, "%s: unable to remove sem %ld\n", argv[0],
                 SEM_KEY);
        exit(1);
    }

    printf("Semaphore %ld has been remove\n", SEM_KEY);
#endif

    return 0;
}
