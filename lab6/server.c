// server.c
#include <stdio.h>
#include <stdlib.h>         // 提供 exit(), atoi(), malloc() 等
#include <string.h>
#include <unistd.h>         // 提供 read(), write(), close() 等系統呼叫
#include <signal.h>         // 處理 Ctrl+C 等訊號
#include <sys/socket.h>     // 提供 socket 通訊介面
#include <netinet/in.h>     // 提供 sockaddr_in 結構與相關定義
#include <sys/shm.h>        // 使用 shared memory 的 API
#include <sys/sem.h>        // 使用 semaphore 的 API
#include <arpa/inet.h>      // 提供 IP 轉換函式（inet_pton 等）
#include <sys/wait.h>
#include <errno.h>          // 提供錯誤代碼解釋字串

#define SHM_KEY 5678  // Shared memory 的 key
#define SEM_KEY 1234  // Semaphore 的 key

int *balance;                   // 指向共享記憶體的指標，用來儲存帳戶餘額
int shm_id, sem_id, server_fd;  // 記錄 shm ID、sem ID、server socket 描述符

// P operation
int P(int s)
{
    struct sembuf sop;
    sop.sem_num = 0;    // 操作第 0 個 semaphore
    sop.sem_op = -1;    // P 操作：進入臨界區
    sop.sem_flg = 0;    // 沒有額外 flag
    if (semop(s, &sop, 1) < 0) {
        fprintf(stderr, "P(): semop failed: %s\n", strerror(errno));
        return -1;
    } else {
        return 0;
    }
}

// V operation
int V(int s)
{
    struct sembuf sop;
    sop.sem_num = 0;    // 操作第 0 個 semaphore
    sop.sem_op = 1;     // V 操作：釋放臨界區
    sop.sem_flg = 0;
    if (semop(s, &sop, 1) < 0) {
        fprintf(stderr, "V(): semop failed: %s\n", strerror(errno));
        return -1;
    } else {
        return 0;
    }
}

// Ctrl+C 處理：釋放資源
void cleanup(int sig) {
    printf("\n[Server] Shutting down, cleaning IPC resources...\n");

    close(server_fd);

    // detach shared memory(分離共享記憶體), 用shmdt()分離
    if (shmdt(balance) == 0) {
        printf("[Server] Detached shared memory.\n");
    } else {
        perror("[Server] shmdt failed");
    }

    // remove shared memory(移除共享記憶體), IPC_RMID 是一個指令，意思是「從系統中完全刪除這塊共享記憶體」
    if (shmctl(shm_id, IPC_RMID, NULL) == 0) {
        printf("[Server] Removed shared memory segment.\n");
    } else {
        perror("[Server] shmctl (IPC_RMID) failed");
    }

    // remove semaphore(移除信號量), IPC_RMID 對 semaphore 的意思也是「從系統中完全刪除」這組 semaphore
    if (semctl(sem_id, 0, IPC_RMID) == 0) {
        printf("[Server] Removed semaphore.\n");
    } else {
        perror("[Server] semctl (IPC_RMID) failed");
    }

    printf("[Server] Cleanup complete. Goodbye.\n");
    exit(0);
}

// 回收子行程
void sigchld_handler(int signum) {
    while (waitpid(-1, NULL, WNOHANG) > 0);  // 非同步回收所有 zombie process
}

// 處理 client 的邏輯
void handle_client(int client_sock) {
    char buf[1024];  // 接收用 buffer
    int n;

    while ((n = read(client_sock, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';  // 加上字串結尾符號
        char op[16];    // 存放 operation: deposit/withdraw
        int amount;     // 操作金額

        // 解析 client 傳來的操作字串
        if (sscanf(buf, "%s %d", op, &amount) != 2) {
            write(client_sock, "Invalid input.\n", 16);  // 格式錯誤
            continue;
        }
        
        // 進入臨界區（P 操作）
        if (P(sem_id) < 0) exit(1);

        // 處理存款提款
        if (strcmp(op, "deposit") == 0) {
            *balance += amount;
            printf("After deposit: %d\n", *balance);  // 顯示目前餘額在 server 終端
            //strcpy(buf, "OK\n");  // 回傳給 client（可自定）
            //snprintf(buf, sizeof(buf), "After deposit: %d\n", *balance);
        } 
        // 處理提款
        else if (strcmp(op, "withdraw") == 0) {
            if (*balance >= amount) {
                *balance -= amount;
                printf("After withdraw: %d\n", *balance);  // 顯示目前餘額在 server 終端
                //strcpy(buf, "OK\n");
                //snprintf(buf, sizeof(buf), "After withdraw: %d\n", *balance);
            } else {
                printf("Withdraw failed. Balance = %d\n", *balance);// 餘額不足
                strcpy(buf, "Withdraw failed\n");
                //snprintf(buf, sizeof(buf), "Withdraw failed. Balance = %d\n", *balance);
            }
        } 
        // 非法操作
        else {
            strcpy(buf, "Unknown operation\n");
            //snprintf(buf, sizeof(buf), "Unknown operation.\n");
        }

        // 離開臨界區（V 操作）
        if (V(sem_id) < 0) exit(1);

        // 回應 client
        write(client_sock, buf, strlen(buf));
    }

    close(client_sock); // 關閉與該 client 的連線
    exit(0);            // 子行程結束
}


int main(int argc, char *argv[]) {
    // 檢查參數是否為 port 號
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // 設定 Ctrl+C 與 zombie process 的訊號處理函式
    signal(SIGINT, cleanup);            // Ctrl+C 清除 IPC
    signal(SIGCHLD, sigchld_handler);   // 回收子行程

    int port = atoi(argv[1]);  // 將 port 字串轉成整數

    // 建立Shared memory
    shm_id = shmget(SHM_KEY, sizeof(int), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }

    //用 shmat() 附加（attach）進共享記憶體地址，初始化帳戶餘額為 0
    balance = (int *)shmat(shm_id, NULL, 0);
    *balance = 0;

    // 建立 semaphore 並初始化為 1, (1 表示你只建立了一組 semaphore 中的1 個元素，所以整個 semaphore set 中只有一個, 確保同一時間只有"一個 "process 可以操作 balance)
    sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (sem_id < 0) {
        perror("semget");
        exit(1);
    }
    semctl(sem_id, 0, SETVAL, 1);  // 設定初始值

    // 建立 socket server
    struct sockaddr_in addr, cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    server_fd = socket(AF_INET, SOCK_STREAM, 0);  // 建立 socket

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));  // 重複綁定 port

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // 接收所有 interface
    addr.sin_port = htons(port);        // 轉為網路位元序

    // 綁定 port
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    // 開始監聽，最多 10 個連線佇列
    listen(server_fd, 10);
    printf("[Server] Listening on port %d...\n", port);

    // 主迴圈：接受 client 連線
    while (1) {
        int client_sock = accept(server_fd, (struct sockaddr *)&cli_addr, &cli_len);
        if (client_sock < 0) continue;

        // fork child process 處理 client
        pid_t pid = fork();
        if (pid == 0) {                 // child process
            close(server_fd);           // 子行程關閉 listener
            handle_client(client_sock); // 處理該 client
        } else {
            close(client_sock);         // parent 關閉 client socket
        }
    }

    return 0;
}
