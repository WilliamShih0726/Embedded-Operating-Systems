/*
 * timer_diff.c
 *
 * 程式用途：
 * 本程式比較 Linux 三種計時器（ITIMER_REAL、ITIMER_VIRTUAL、ITIMER_PROF）的觸發行為。
 * 透過設定三種 timer 各自以 100ms 為週期，並統一執行一段密集的 I/O 操作，
 * 最後顯示每一種 signal 對應的觸發次數，讓使用者觀察 timer 差異。
 *
 * 操作說明：
 * 執行後會開始大量開檔讀檔（代表有 CPU 執行與系統呼叫），
 * 三種 signal handler 分別統計各自收到的次數。
 * 最後會印出 SIGALRM、SIGVTALRM、SIGPROF 被觸發的次數。
 */

/*
* timer_diff.c
*/
#include <fcntl.h>         // 提供 open()
#include <signal.h>        // 提供 signal 設定
#include <stdio.h>         // 提供 printf()
#include <stdlib.h>        // 提供 exit()
#include <string.h>        // 提供 memset()
#include <sys/stat.h>      // 提供 open() 用的 flag
#include <sys/time.h>      // 提供 setitimer(), struct itimerval
#include <sys/types.h>     // 提供基本型別
#include <unistd.h>        // 提供 read(), close()

/* counter */
int SIGALRM_count = 0;        // 計數 SIGALRM 被觸發的次數
int SIGVTALRM_count = 0;      // 計數 SIGVTALRM 被觸發的次數
int SIGPROF_count = 0;        // 計數 SIGPROF 被觸發的次數

/* handler of SIGALRM */
void SIGALRM_handler (int signum)
{
    SIGALRM_count++;  // 每次觸發就累加
}

/* handler of SIGVTALRM */
void SIGVTALRM_handler (int signum)
{
    SIGVTALRM_count++;  // 每次觸發就累加
}

/* handler of SIGPROF */
void SIGPROF_handler (int signum)
{
    SIGPROF_count++;  // 每次觸發就累加
}

void IO_WORKS();  // 宣告密集 I/O 函式

int main (int argc, char **argv)
{
    struct sigaction SA_SIGALRM, SA_SIGVTALRM, SA_SIGPROF;  // 各 signal 的處理器設定
    struct itimerval timer;                                 // 計時器結構

    /* Install SIGALRM_handler as the signal handler for SIGALRM */
    memset (&SA_SIGALRM, 0, sizeof (SA_SIGALRM));           // 初始化結構
    SA_SIGALRM.sa_handler = &SIGALRM_handler;               // 註冊對應 handler
    sigaction (SIGALRM, &SA_SIGALRM, NULL);                 // 設定 handler 給 SIGALRM

    /* Install SIGVTALRM_handler as the signal handler for SIGVTALRM */
    memset (&SA_SIGVTALRM, 0, sizeof (SA_SIGVTALRM));
    SA_SIGVTALRM.sa_handler = &SIGVTALRM_handler;
    sigaction (SIGVTALRM, &SA_SIGVTALRM, NULL);             // 設定 handler 給 SIGVTALRM

    /* Install SIGPROF_handler as the signal handler for SIGPROF */
    memset (&SA_SIGPROF, 0, sizeof (SA_SIGPROF));
    SA_SIGPROF.sa_handler = &SIGPROF_handler;
    sigaction (SIGPROF, &SA_SIGPROF, NULL);                 // 設定 handler 給 SIGPROF

    /* Configure the timer to expire after 100 msec */
    timer.it_value.tv_sec = 0;                              // 首次觸發時間：0 秒
    timer.it_value.tv_usec = 100000;                        // 首次觸發時間：100,000 微秒 = 100 毫秒

    /* Reset the timer back to 100 msec after expired */
    timer.it_interval.tv_sec = 0;                           // 重複間隔時間：0 秒
    timer.it_interval.tv_usec = 100000;                     // 重複間隔時間：100 毫秒

    /* Start timer */
    setitimer(ITIMER_REAL, &timer, NULL);                   // 啟動 ITIMER_REAL：實際時間，觸發 SIGALRM
    setitimer(ITIMER_VIRTUAL, &timer, NULL);                // 啟動 ITIMER_VIRTUAL：CPU 上執行時間，觸發 SIGVTALRM
    setitimer(ITIMER_PROF, &timer, NULL);                   // 啟動 ITIMER_PROF：CPU + kernel，觸發 SIGPROF

    /* Do some I/O operations */
    IO_WORKS();                                             // 執行密集的檔案操作，消耗 CPU 及系統資源

    printf("SIGALRM_count   = %d\n", SIGALRM_count);        // 印出各 signal 的觸發次數
    printf("SIGVTALRM_count = %d\n", SIGVTALRM_count);
    printf("SIGPROF_count   = %d\n", SIGPROF_count);

    return 0;
}

void IO_WORKS()
{
    int fd, ret;
    char buffer[100];
