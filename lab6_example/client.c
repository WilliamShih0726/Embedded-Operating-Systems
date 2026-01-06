#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>          // for close()
#include <arpa/inet.h>       // for sockaddr_in

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[1024];

    // 1. 建立 socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // 2. 設定 server 位址
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    // 3. 連線到 server
    connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // 4. 傳送資料
    write(sockfd, "Hello Server", 12);

    // 5. 接收回應
    read(sockfd, buffer, sizeof(buffer));
    printf("Received from server: %s\n", buffer);

    // 6. 關閉
    close(sockfd);
    return 0;
}
