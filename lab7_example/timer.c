/*
 * timer.c
 *
 * 程式用途：
 * 本程式示範如何使用 setitimer() 配合 SIGVTALRM 虛擬計時器，
 * 每隔 250 毫秒（以 process 實際在 CPU 上執行時間計）觸發一次 signal，
 * 並執行對應的 signal handler 顯示觸發次數。
 *
 * 操作說明：
 * 執行後程式會持續進行 busy loop（無限迴圈），
 * 每當 process 實際執行累積達 250 毫秒，handler 會觸發並印出觸發次數。
 */

/*
* timer.c
*/
#include <signal.h>       // 提供 signal 處理功能，如 sigaction()
#include <stdio.h>        // 提供 printf()
#include <string.h>       // 提供 memset()
#include <sys/time.h>     // 提供 setitimer(), itimerval 結構等

void timer_handler (int signum)  // signal handler：處理 SIGVTALRM 時執行
{
    static int count = 0;  // 靜態區域變數，累積觸發次數
    printf ("timer expired %d times\n", ++count);  // 印出目前觸發次數
}

int main (int argc, char **argv)
{
    struct sigaction sa;          // 用於註冊 signal handler
    struct itimerval timer;       // 用於設定計時器時間參數

    /* Install timer_handler as the signal handler for SIGVTALRM */
    memset (&sa, 0, sizeof (sa));             // 將 sigaction 結構清空初始化
    sa.sa_handler = &timer_handler;           // 指定 handler 函式
    sigaction (SIGVTALRM, &sa, NULL);         // 註冊 SIGVTALRM 的處理方式

    /* Configure the timer to expire after 250 msec */
    timer.it_value.tv_sec = 0;                // 第一次觸發秒數設為 0
    timer.it_value.tv_usec = 250000;          // 第一次觸發微秒設為 250,000（即 250ms）

    /* Reset the timer back to 250 msec after expired */
    timer.it_interval.tv_sec = 0;             // 每次重設間隔秒數為 0
    timer.it_interval.tv_usec = 250000;       // 每次重設間隔為 250ms，表示週期性觸發

    /* Start a virtual timer */
    setitimer (ITIMER_VIRTUAL, &timer, NULL); // 啟動虛擬計時器（只計算 process 自身執行時間）

    /* Do busy work */
    while (1);  // 持續進行忙碌迴圈，避免程式結束，保持 process 在 CPU 上執行

    return 0;  // 永遠不會到這裡，但符合語法完整性
}
