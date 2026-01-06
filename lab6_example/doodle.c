/* doodle.c
*
* This program shows how P () and V () can be implemented,
* then uses a semaphore that everyone has access to.
* 本程式示範如何實作 P() 和 V() 函數，並透過指定的 key 使用共享的 semaphore，
* 進入與離開 critical section（臨界區）。
*/

#include <errno.h>      // 提供錯誤訊息處理函式 strerror()、errno
#include <stdio.h>      // 標準輸入輸出：printf、scanf、fprintf
#include <stdlib.h>     // 提供 exit() 等函式
#include <string.h>     // 處理字串與錯誤訊息轉換
#include <sys/sem.h>    // System V semaphore 操作的標頭檔
#include <unistd.h>     // sleep() 等函式

#define DOODLE_SEM_KEY 1122334455  // 預設的 semaphore key（此處沒使用）

/* P () - returns 0 if OK; -1 if there was a problem
 * 嘗試進入 critical section：如果 semaphore 值為 0，會阻塞等待。
 */
int P(int s)
{
    struct sembuf sop;         /* 定義 semaphore 操作的參數結構 */
    sop.sem_num = 0;           /* 操作第 0 個（也是唯一一個）semaphore */
    sop.sem_op = -1;           /* 將 semaphore 值減 1（P操作） */
    sop.sem_flg = 0;           /* 無特別旗標 */

    if (semop (s, &sop, 1) < 0) {   // 執行 semaphore 操作
        fprintf(stderr,"P(): semop failed: %s\n",strerror(errno));
        return -1;
    } else {
        return 0;
    }
}

/* V() - returns 0 if OK; -1 if there was a problem
 * 離開 critical section：將 semaphore 值加 1，喚醒其他等待中的 process。
 */
int V(int s)
{
    struct sembuf sop;         /* 定義 semaphore 操作的參數結構 */
    sop.sem_num = 0;           /* 操作第 0 個 semaphore */
    sop.sem_op = 1;            /* 將 semaphore 值加 1（V操作） */
    sop.sem_flg = 0;           /* 無特殊旗標 */

    if (semop(s, &sop, 1) < 0) {   // 執行 semaphore 操作
        fprintf(stderr,"V(): semop failed: %s\n",strerror(errno));
        return -1;
    } else {
        return 0;
    }
}

int main ( int argc, char **argv)
{
    int s, secs;       // s: semaphore ID, secs: 逗留時間（使用者輸入）
    long int key;      // semaphore 的 key（從參數取得）

    // 檢查參數數量，必須正確傳入一個 key
    if (argc != 2) {
        fprintf(stderr, "%s: specify a key (long)\n", argv[0]);
        exit(1);
    }

    /* get values from command line */
    // 將輸入的參數轉換為 long 整數形式（key）
    if (sscanf(argv[1], "%ld", &key)!=1)
    {
        /* convert arg to long integer */
        fprintf(stderr,
            "%s: argument #1 must be an long integer\n", argv[0]);
        exit(1);
    }

    // 嘗試透過 key 取得 semaphore ID，不會建立新 semaphore（第三參數為 0）
    s = semget(key, 1, 0);
    if (s < 0) {
        fprintf (stderr,
            "%s: cannot find semaphore %ld: %s\n",
            argv[0], key, strerror(errno));
        exit(1);
    }

    // 進入主迴圈：讓使用者指定進入 critical section 的秒數
    while (1) {
        printf ("#secs to doodle in the critical section? (0 to exit):");
        scanf ("%d",&secs);

        // 若使用者輸入 0，則跳出程式
        if (secs == 0)
            break;

        printf ("Preparing to enter the critical section..\n");

        P(s);  // 嘗試進入臨界區（可能會阻塞）

        printf ("Now in the critical section! Sleep %d secs..\n", secs);

        // 模擬在臨界區執行的時間（每秒倒數輸出）
        while (secs) {
            printf ("%d...doodle...\n",secs--);
            sleep (1);
        }

        printf ("Leaving the critical section..\n");

        V(s);  // 離開臨界區，釋放 semaphore
    }

    return 0;
}
