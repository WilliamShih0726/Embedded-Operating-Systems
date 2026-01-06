#include <pthread.h>     // 引入 pthread 函式庫，用於 thread 操作
#include <stdio.h>       // 標準輸出入
#include <stdlib.h>      // 提供 exit() 等函式
#include <unistd.h>      // 提供 sleep() 等函式

// 檢查 pthread 操作是否成功，若失敗則輸出錯誤並結束程式
#define checkResults(string, val) { \
    if (val) { \
        printf("Failed with %d at %s", val, string); \
        exit(1); \
    } \
}

#define NUMTHREADS 3     // 定義 thread 數量為 3

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // 初始化 mutex
int sharedData = 0;      // 要被多個 thread 同步操作的共享資料 1
int sharedData2 = 0;     // 要被多個 thread 同步操作的共享資料 2

// 子 thread 執行的函式
void *theThread(void *parm)
{
    int rc;
    printf("\tThread %lu: Entered\n", (unsigned long) pthread_self()); // 印出 thread ID

    /* lock mutex */
    rc = pthread_mutex_lock(&mutex);              // 嘗試鎖住 mutex，若已被鎖住則等待
    checkResults("pthread_mutex_lock()\n", rc);

    /**************** Critical Section *****************/
    printf("\tThread %lu: Start critical section, holding lock\n",
           (unsigned long) pthread_self());       // 進入臨界區，開始操作共享資料

    /* Access to shared data goes here */
    ++sharedData;                                 // 對共享變數做加法
    --sharedData2;                                // 對共享變數做減法
    printf("\tsharedData = %d, sharedData2 = %d\n", 
           sharedData, sharedData2);              // 印出目前共享資料

    printf("\tThread %lu: End critical section, release lock\n",
           (unsigned long) pthread_self());       // 結束臨界區，準備解鎖
    /**************** Critical Section *****************/

    /* unlock mutex */
    rc = pthread_mutex_unlock(&mutex);            // 解鎖，讓其他 thread 可以進入臨界區
    checkResults("pthread_mutex_unlock()\n", rc);

    return NULL;                                  // 結束 thread
}

int main(int argc, char **argv)
{
    pthread_t thread[NUMTHREADS];                 // 建立 thread 陣列
    int rc = 0;
    int i;

    /* lock mutex */
    printf("Main thread hold mutex to prevent access to shared data\n");
    rc = pthread_mutex_lock(&mutex);              // 主程式一開始鎖住 mutex，阻擋其他 thread
    checkResults("pthread_mutex_lock()\n", rc);

    /* create thread */
    printf("Main thread create/start threads\n");
    for (i = 0; i < NUMTHREADS; ++i) {
        rc = pthread_create(&thread[i], NULL, theThread, NULL); // 建立 thread
        checkResults("pthread_create()\n", rc);
    }

    /* wait for thread creation complete */
    printf("Main thread wait a bit until 'done' with the shared data\n");
    sleep(3);                                     // 模擬主程式還在使用共享資料

    /* unlock mutex */
    printf("Main thread unlock shared data\n");
    rc = pthread_mutex_unlock(&mutex);            // 主程式釋放鎖，讓其他 thread 開始執行
    checkResults("pthread_mutex_lock()\n", rc);

    /* wait thread complete */
    printf("Main thread Wait for threads to complete, "
           "and release their resources\n");
    for (i = 0; i < NUMTHREADS; ++i) {
        rc = pthread_join(thread[i], NULL);       // 等待每個 thread 結束
        checkResults("pthread_join()\n", rc);
    }

    /* destroy mutex */
    printf("Main thread clean up mutex\n");
    rc = pthread_mutex_destroy(&mutex);           // 銷毀 mutex

    printf("Main thread completed\n");
    return 0;                                     // 正常結束程式
}
