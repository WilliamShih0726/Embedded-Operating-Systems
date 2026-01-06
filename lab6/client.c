// client.c
#include <stdio.h>
#include <stdlib.h>         // 提供 exit, atoi 等函式
#include <string.h>
#include <unistd.h>         // 提供 close(), read(), write() 等系統呼叫
#include <arpa/inet.h>      // 提供 sockaddr_in, htons(), inet_pton() 等網路函式

int main(int argc, char *argv[]) {
    // 檢查參數是否正確
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <ip> <port> <deposit/withdraw> <amount> <times>\n", argv[0]);
        exit(1);  // 若參數數量不正確則離開
    }

    char *ip = argv[1];             // Server IP 位址
    int port = atoi(argv[2]);       // Server port（字串轉整數）
    char *op = argv[3];             // 操作類型（deposit 或 withdraw）
    int amount = atoi(argv[4]);     // 單次操作的金額
    int times = atoi(argv[5]);      // 重複幾次操作

    // 重複執行指定次數的存提款操作
    for (int i = 0; i < times; i++) {
        // 建立 socket（TCP）
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("socket");
            exit(1);
        }

        // 設定 server 端位址結構
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;               // 使用 IPv4
        server_addr.sin_port = htons(port);             // 將 port 轉換成網路位元序
        inet_pton(AF_INET, ip, &server_addr.sin_addr);  // 將 IP 字串轉換成二進位格式

        // 嘗試連線到 server
        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("connect");
            close(sock);
            continue;
        }

        // 建立傳送與接收緩衝區
        char msg[128], res[1024];
        snprintf(msg, sizeof(msg), "%s %d", op, amount);  // 將操作與金額組成字串傳送給 server
        
        // 將資料寫入 socket（傳送至 server）
        write(sock, msg, strlen(msg));

        // 讀取 server 回應
        int n = read(sock, res, sizeof(res) - 1);
        if (n > 0) {
            res[n] = '\0';                                  // 將回應資料補上字串結尾符號
            printf("[Client] Server response: %s\n", res);  // 印出 server 回應
        }

        // 關閉與 server 的連線
        close(sock);

        // 若需避免過快發送，可打開這行 sleep
        //usleep(100000);
    }

    // 所有操作完成
    printf("[Client] Finished %d operations (%s %d)\n", times, op, amount);

    return 0;
}
