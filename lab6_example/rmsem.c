/* rmsem.c
*
* This program destroys a semaphore. The user should pass a number
* to be used as the semaphore key.
*/

#include <errno.h>      // 提供錯誤代碼與錯誤訊息的函式
#include <stdio.h>      // 標準輸入輸出，例如 printf, fprintf
#include <stdlib.h>     // 提供 exit() 等功能
#include <string.h>     // 提供 strerror() 錯誤訊息轉換
#include <sys/sem.h>    // 使用 System V semaphore 相關函數

int main(int argc, char **argv)
{
    int s;              // 儲存 semaphore ID
    long int key;       // 使用者輸入的 semaphore key

    // 檢查是否有輸入一個參數（semaphore key）
    if (argc != 2)
    {
        fprintf(stderr, "%s: specify a key (long)\n", argv[0]);
        exit(1);
    }

    /* get values from command line */
    // 將參數 argv[1] 轉換成 long 型態儲存於 key
    if (sscanf(argv[1], "%ld", &key)!=1)
    {
        /* convert arg to long integer */
        fprintf(stderr,
            "%s: argument #1 must be an long integer\n", argv[0]);
        exit(1);
    }

    /* find semaphore */
    // 嘗試用 key 取得已存在的 semaphore ID
    // 第二個參數=1 是 dummy（不會建立新 semaphore）
    // 第三個參數=0 代表「不建立，只查詢」
    s = semget(key,1,0);
    
    // 若找不到（semget 回傳 -1），則印出錯誤訊息
    if (s < 0)
    {
        fprintf(stderr, "%s: failed to find semaphore %ld: %s\n",
            argv[0], key, strerror(errno));
        exit(1);
    }

    // 找到 semaphore，印出訊息
    printf("Semaphore %ld found\n",key);

    /* remove semaphore */
    // 使用 semctl 移除該 semaphore（IPC_RMID）
    if (semctl (s, 0, IPC_RMID, 0) < 0)
    {
        fprintf (stderr, "%s: unable to remove semaphore %ld\n",
            argv[0], key);
        exit(1);
    }

    // 成功移除後，印出提示訊息
    printf("Semaphore %ld has been remove\n", key);

    return 0;
}
