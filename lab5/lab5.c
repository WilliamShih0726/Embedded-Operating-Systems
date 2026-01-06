#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int server_fd; // 全域變數，儲存 server 的 socket 描述符，讓 signal handler 也能使用

// 當子程序（例如火車動畫）結束時，被呼叫清除 zombie process，避免出現 defunct
void sigchld_handler(int signum) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// 當使用者按 Ctrl+C 結束 server 時，負責關閉 socket，安全退出
void sigint_handler(int signum) {
    close(server_fd);
    printf("\n[Server] Socket closed. Server shutting down.\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    // 確保使用者正確輸入 port，否則提示使用方式
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);

    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);

    // 告訴系統：收到 SIGCHLD 和 SIGINT 時，要執行我們自己寫的 handler
    signal(SIGCHLD, sigchld_handler);
    signal(SIGINT, sigint_handler);

    // 建立 TCP socket，傳回一個檔案描述符
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 允許 port 被重複使用，避免 server crash 後馬上重啟失敗
    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    // 初始化並設定 server 的 IP、port（用 INADDR_ANY 接所有 IP）
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // 綁定 IP + port 到 socket（server 會用這個門牌號碼等 client）
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // 	告訴作業系統：這個 socket 開始接受連線（最多排隊 10 個）
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // 印出目前伺服器正在等誰連進來
    printf("[Server] Listening on port %d...\n", port);

    // Main loop to accept connections
    while (1) { // 主迴圈，無限接收 client 連線
        // 當有 client（例如 nc localhost 4444）連上來，這邊會醒來並取得新的 socket
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        // 	建立一個子程序來處理這個 client
        pid_t pid = fork();
        if (pid == 0) { // 	Child process部分 client 的火車專用流程
            close(server_fd); // 子程序不需要再接收其他 client，關掉 server socket

            // 	印出是哪個 IP 和 port 的 client 連進來
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
            printf("[Child] Connection from %s:%d\n", ip, ntohs(client_addr.sin_port));

            dup2(client_fd, STDOUT_FILENO); // 把火車動畫的輸出導到 socket（不是螢幕）
            close(client_fd); //原 socket fd 可以關了（stdout 已經接管）

            execlp("sl", "sl", "-l", NULL); // 執行火車動畫程式 sl（-l 是小火車）
            perror("execlp");
            exit(EXIT_FAILURE);

        } else if (pid > 0) { // Parent process部分
            printf("[Server] Train PID %d launched.\n", pid);
            close(client_fd);
        } else { // fork 失敗，印出錯誤並關掉該 socket
            perror("fork");
            close(client_fd);
        }
    }

    return 0;
}
