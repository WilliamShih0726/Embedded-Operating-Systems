/* pipe.c
*
* child process read the content of file
* and write the content to parent process through pipe
*/
#include <fcntl.h>      // 提供 open()
#include <stdio.h>      // 提供 printf(), perror()
#include <stdlib.h>     // 提供 exit()
#include <string.h>     // 提供字串處理
#include <sys/stat.h>   // 提供檔案屬性操作
#include <sys/types.h>  // 提供 pid_t 等資料型別
#include <sys/wait.h>   // 提供 wait()
#include <unistd.h>     // 提供 fork(), pipe(), read(), write(), close()

int pfd[2]; /* pfd[0] is read end, pfd[1] is write end */
// 宣告 pipe 的兩端，pfd[0] 用來讀，pfd[1] 用來寫

// 子行程要執行的函式：讀取檔案，將內容寫入 pipe
void ChildProcess(char *path)
{
    int fd;
    int ret;
    char buffer[100];

    /* close unused read end */
    close(pfd[0]);  // 子行程只需要寫入資料，所以關閉讀取端

    /* open file */
    fd = open(path, O_RDONLY);  // 開啟檔案以供讀取
    if (fd < 0) {
        printf("Open %s failed.\n", path);
        exit(EXIT_FAILURE);
    }

    /* read file and write content to pipe */
    while (1) {
        /* read raw data from file */
        ret = read(fd, buffer, 100);  // 每次最多讀 100 bytes 到 buffer
        if (ret < 0) { /* error */
            perror("read()");
            exit(EXIT_FAILURE);
        }
        else if (ret == 0) { /* reach EOF */
            close(fd);      // 關閉檔案
            close(pfd[1]);  // 關閉 pipe 的寫入端，通知 parent EOF
            exit(EXIT_SUCCESS);
        }
        else { /* write content to pipe */
            write(pfd[1], buffer, ret);  // 將讀到的資料寫入 pipe
        }
    }
}

// 父行程要執行的函式：從 pipe 讀取子行程送來的資料並印出
void ParentProcess()
{
    int ret;
    char buffer[100];

    /* close unused write end */
    close(pfd[1]);  // 父行程只需要讀取資料，關閉寫入端

    /* read data from pipe until reach EOF */
    while (1) {
        ret = read(pfd[0], buffer, 100);  // 從 pipe 讀資料進 buffer
        if (ret > 0) { /* print data to screen */
            printf("%.*s", ret, buffer);  // 印出實際讀到的資料
        }
        else if (ret == 0) { /* reach EOF */
            close(pfd[0]);    // 關閉 pipe 讀取端
            wait(NULL);       // 等待子行程結束，避免殭屍行程
            exit(EXIT_SUCCESS);
        }
        else {
            perror("pipe read()");  // 讀取錯誤
            exit(EXIT_FAILURE);
        }
    }
}

// 主程式：建立 pipe、fork 子行程、分工
int main(int argc, char *argv[])
{
    pid_t cpid;

    if (argc != 2) {
        fprintf(stderr, "%s: specify a file\n", argv[0]);  // 檢查參數數量
        exit(1);
    }

    /* create pipe */
    if (pipe(pfd) == -1) {  // 建立 pipe，取得一對檔案描述符
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    /* fork child process */
    cpid = fork();  // 建立子行程
    if (cpid == -1) { /* error */
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (cpid == 0)
        ChildProcess(argv[1]);   // 子行程執行檔案讀取與 pipe 寫入
    else
        ParentProcess();         // 父行程從 pipe 讀取並印出

    return 0;
}
