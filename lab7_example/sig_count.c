/*
 * sig_count.c
 *
 * 程式用途：
 * 本程式示範如何使用 signal handler 來統計 SIGUSR1 訊號的觸發次數，
 * 並使用 sig_atomic_t 確保在非同步情況下對全域變數的安全操作。
 *
 * 操作說明：
 * 程式執行後會睡眠 10 秒，在此期間若對此行程多次傳送 SIGUSR1（例如使用 kill -USR1 <PID>），
 * handler 會安全地累加計數器，最後顯示總共收到幾次 SIGUSR1。
 */

/*
* sig_count.c
*/
#include <signal.h>      // 提供 signal 與 sigaction
#include <stdio.h>       // 提供 printf()
#include <string.h>      // 提供 memset()
#include <sys/types.h>   // 提供 pid_t 等型別
#include <time.h>        // 提供 nanosleep(), timespec 結構
#include <unistd.h>      // 提供 getpid()

sig_atomic_t sigusr1_count = 0;  // 用來統計 SIGUSR1 次數，使用 sig_atomic_t 可避免中斷導致資料錯誤

void handler (int signal_number)  // signal handler：收到 SIGUSR1 時執行
{
    ++sigusr1_count; /* add one, protected atomic action */  // 原子操作：將計數加一，安全
}

int main ()
{
    struct sigaction sa;        // 用來註冊 signal handler
    struct timespec req;        // 用來設定 nanosleep() 時間
    int retval;

    /* set the sleep time to 10 sec */
    memset(&req, 0, sizeof(struct timespec));  // 初始化 req 結構
    req.tv_sec = 10;                           // 設定等待 10 秒
    req.tv_nsec = 0;

    /* register handler to SIGUSR1 */
    memset(&sa, 0, sizeof (sa));               // 初始化 sigaction 結構
    sa.sa_handler = handler;                   // 指定 handler 函式
    sigaction (SIGUSR1, &sa, NULL);            // 註冊 SIGUSR1 的處理方式

    printf("Process (%d) is catching SIGUSR1 ...\n", getpid());  // 印出目前行程 PID，供測試者發送 signal

    /* sleep 10 sec */
    do{
        retval = nanosleep(&req, &req);  // 使用 nanosleep 休眠，可在中斷後繼續剩餘時間
    } while(retval);                     // 若被 signal 中斷就再 sleep 剩餘時間

    printf ("SIGUSR1 was raised %d times\n", sigusr1_count);  // 印出收到 SIGUSR1 的次數

    return 0;
}
