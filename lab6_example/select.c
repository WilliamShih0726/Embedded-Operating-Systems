/*
* select.c
*/
#include <stdio.h>      // 提供 printf、perror 等輸出函式
#include <stdlib.h>     // 提供 exit、srand、random 等
#include <string.h>     // 提供 memset、memcpy、strlen 等
#include <sys/time.h>   // 提供 timeval 結構，用於 select timeout
#include <sys/types.h>  // 提供型別，如 pid_t
#include <time.h>       // 提供 time()，設定亂數種子
#include <unistd.h>     // 提供 pipe、fork、close、read、write、sleep 等

#define max(a, b) ((a > b) ? a : b)  // 定義最大值 macro

void ChildProcess(int *pfd, int sec)
{
    char buffer[100];
    /* close unused read end */
    close(pfd[0]);  // 子行程不讀，只寫入資料，所以關閉讀端

    /* sleep a random time to wait parent process enter select() */
    printf("Child process (%d) wait %d secs\n", getpid(), sec);
    sleep(sec);  // 延遲 sec 秒，模擬非同步事件

    /* write message to parent process */
    memset(buffer, 0, 100);  // 清空 buffer
    sprintf(buffer, "Child process (%d) sent message to parent process\n", getpid());  // 組成訊息
    write(pfd[1], buffer, strlen(buffer));  // 將訊息寫入 pipe

    /* close write end */
    close(pfd[1]);  // 寫完資料關閉寫端，通知 parent EOF
    exit(EXIT_SUCCESS);  // 正常結束
}

int main(int argc, char *argv[])
{
    int pfd1[2], pfd2[2];             // pipe 的描述符陣列
    int cpid1, cpid2;                 // 子行程 PID
    fd_set rfds, arfds;              // 兩個 fd_set：arfds 是固定監控清單，rfds 是 select 使用的臨時集合
    int max_fd;                      // select 需要的最大 fd + 1
    struct timeval tv;               // select 的 timeout 結構
    int retval;                      // select 或 read 回傳值
    int fd_index;                    // 用於迴圈檢查哪個 fd 準備好了
    char buffer[100];

    /* random seed */
    srand(time(NULL));  // 設定亂數種子，讓 sleep 時間每次不同

    /* create pipe */
    pipe(pfd1);  // 建立 pipe1，給第一個子行程用
    pipe(pfd2);  // 建立 pipe2，給第二個子行程用

    /* create 2 child processes and set corresponding pipe & sleep time */
    cpid1 = fork();  // 建立第一個子行程
    if (cpid1 == 0)
        ChildProcess(pfd1, random() % 5);  // 第一個子行程延遲 0~4 秒

    cpid2 = fork();  // 建立第二個子行程
    if (cpid2 == 0)
        ChildProcess(pfd2, random() % 4);  // 第二個子行程延遲 0~3 秒

    /* close unused write end */
    close(pfd1[1]);  // 父行程只讀資料，關閉寫端
    close(pfd2[1]);

    /* set pfd1[0] & pfd2[0] to watch list */
    FD_ZERO(&rfds);             // 清空讀取用集合
    FD_ZERO(&arfds);            // 清空監控集合
    FD_SET(pfd1[0], &arfds);    // 加入 pipe1 的讀端
    FD_SET(pfd2[0], &arfds);    // 加入 pipe2 的讀端
    max_fd = max(pfd1[0], pfd2[0]) + 1;  // select 的第一個參數（最大的 fd + 1）

    /* Wait up to five seconds. */
    tv.tv_sec = 5;  // 設定 select 最多等待 5 秒
    tv.tv_usec = 0;

    while(1)
    {
        /* config fd_set for select */
        memcpy(&rfds, &arfds, sizeof(rfds));  // 每次 select 都要重新設定 rfds（因為 select 會修改它）

        /* wait until any fd response */
        retval = select(max_fd, &rfds, NULL, NULL, &tv);  // 監控讀取事件
        if (retval == -1) { /* error */
            perror("select()");
            exit(EXIT_FAILURE);
        }
        else if (retval) { /* # of fd got respone */
            printf("Data is available now.\n");  // 有資料可讀
        }
        else { /* no fd response before timer expired */
            printf("No data within five seconds.\n");  // timeout
            break;
        }

        /* check if any response */
        for (fd_index = 0; fd_index < max_fd; fd_index++)
        {
            if (!FD_ISSET(fd_index, &rfds))
                continue;  // 這個 fd 沒有資料，跳過

            retval = read(fd_index, buffer, 100);  // 從 pipe 讀資料
            if (retval > 0) /* read data from pipe */
                printf("%.*s", retval, buffer);  // 印出讀到的資料
            else if (retval < 0) /* error */
                perror("pipe read()");
            else { /* write fd closed */
                /* close read fd */
                close(fd_index);         // 關閉這個 fd（表示對方已關閉）
                /* remove fd from watch list */
                FD_CLR(fd_index, &arfds);  // 從監控集合移除
            }
        }
    }

    return 0;
}
