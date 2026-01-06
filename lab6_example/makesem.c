/* makesem.c
*
* This program creates a semaphore. The user should pass
* a number to be used as the semaphore key and initial
* value as the only command line arguments. If that
* identifier is not taken, then a semaphore will be created.
* If a semaphore is set so that then no semaphore will be
* created. The semaphore is set so that anyone on the system
* can use it.
*/

#include <errno.h>      // 提供錯誤訊息處理（strerror、errno）
#include <stdio.h>      // 標準輸入輸出功能
#include <stdlib.h>     // 提供 exit(), atoi(), malloc() 等
#include <string.h>     // 提供字串處理
#include <sys/sem.h>    // 使用 System V semaphore API

#define SEM_MODE 0666   /* 設定 semaphore 權限為 rw-rw-rw-（所有使用者皆可操作） */

int main (int argc, char **argv)
{
    int s;              // s 將儲存 semaphore 的識別碼（ID）
    long int key;       // 使用者輸入的 semaphore key（類似名稱）
    int val;            // semaphore 初始值

    // 檢查參數數量，應為 2 個參數（key 和 初始值）
    if (argc != 3)
    {
        fprintf(stderr, "%s: specify a key (long) and initial value (int)\n", argv[0]);
        exit(1);
    }

    /* get values from command line */
    // 嘗試將 argv[1] 轉換為 long 型別（作為 key 使用）
    if (sscanf(argv[1], "%ld", &key) != 1)
    {
        /* convert arg to long integer */
        fprintf(stderr, "%s: argument #1 must be an long integer\n", argv[0]);
        exit(1);
    }

    // 嘗試將 argv[2] 轉換為 int 型別（作為 semaphore 的初始值）
    if (sscanf(argv[2], "%d", &val) != 1)
    {
        /* convert arg to long integer */
        fprintf(stderr, "%s: argument #2 must be an integer\n", argv[0]);
        exit(1);
    }

    /* semget() takes three parameters */
    /* Options:
    * IPC_CREAT - create a semaphore if not exists
    * IPC_EXCL - creation fails if it already exists
    * SEM_MODE - access permission
    */

    // 呼叫 semget 建立新的 semaphore（key 為使用者指定，數量為 1，權限為 0666）
    s = semget( 
        key, /* the unique name of the semaphore on the system */
        1, /* we create an array of semaphores, but just need 1. */
        IPC_CREAT | IPC_EXCL | SEM_MODE);  // 建立 semaphore，若已存在則報錯

    /* If semget () returns -1 then it failed. However,
    * if it returns any other number >= 0 then that becomes
    * the identifier within the program for accessing the semaphore.
    */

    // 檢查 semget 是否成功，若失敗則印出錯誤訊息並結束程式
    if (s < 0)
    {
        fprintf(stderr,
            "%s: creation of semaphore %ld failed: %s\n", argv[0],
            key, strerror(errno));
        exit(1);
    }

    // 成功建立 semaphore 後，印出提示訊息
    printf("Semaphore %ld created\n", key);

    /* set semaphore (s[0]) value to initial value (val) */
    // 設定 semaphore（第 0 個） 的初始值為 val
    if ( semctl(s, 0, SETVAL, val) < 0 )
    {
        fprintf(stderr,
            "%s: Unable to initialize semaphore: %s\n",
            argv[0], strerror(errno));
        exit(0);
    }

    // 成功設定初始值後，印出提示訊息
    printf("Semaphore %ld has been initialized to %d\n", key, val);

    return 0;
}
