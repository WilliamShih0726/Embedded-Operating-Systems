/*
 * sig_catch.c
 *
 * 程式用途：
 * 本程式示範如何使用 sigaction() 註冊自訂的 signal handler，
 * 並在收到 SIGUSR1 訊號時，執行 handler 函式來顯示傳送該訊號的行程 PID。
 *
 * 操作說明：
 * 執行本程式後，會列出本身的 PID 並進入 10 秒的等待期。
 * 在這段期間，可使用命令：
 *     kill -USR1 <PID>
 * 從其他 terminal 傳送 SIGUSR1 給該行程，觀察 handler 被觸發並顯示 sender PID。
 */

/*
* sig_catch.c
*/
#include <signal.h>      // 提供 signal 機制與 sigaction 結構
#include <stdio.h>       // 提供 printf 等輸出函式
#include <string.h>      // 提供 memset 等記憶體操作
#include <sys/types.h>   // 提供 pid_t 等型別
#include <unistd.h>      // 提供 getpid(), sleep 等函式

void handler(int signo, siginfo_t *info, void *context)  // 自訂 signal handler，符合 SA_SIGINFO 格式
{
    /* show the process ID sent signal */
    printf("Process (%d) sent SIGUSR1.n", info->si_pid);  // 顯示傳送 signal 的行程 PID（注意：這裡應該是 \n，但原程式沒改）
}

int main(int argc, char *argv[])  // 主程式進入點
{
    struct sigaction my_action;  // 宣告 sigaction 結構，用來設定 signal handler

    /* register handler to SIGUSR1 */
    memset(&my_action, 0, sizeof(struct sigaction));  // 將 my_action 初始化為 0（清空）
    my_action.sa_flags = SA_SIGINFO;                  // 設定 flag：啟用 sa_sigaction 而非 sa_handler
    my_action.sa_sigaction = handler;                 // 設定處理 SIGUSR1 的 handler 函式指標
    sigaction(SIGUSR1, &my_action, NULL);             // 註冊對 SIGUSR1 的處理方式為 my_action 所指定的 handler

    printf("Process (%d) is catching SIGUSR1 ...\n", getpid());  // 印出目前行程的 PID，方便手動用 kill 傳送 signal
    sleep(10);                                                   // 等待 10 秒，在這期間內可以傳送 SIGUSR1 給此行程
    printf("Done.\n");                                           // 等待結束後印出訊息

    return 0;  // 程式結束
}
