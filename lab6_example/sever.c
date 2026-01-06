#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>           // for close()
#include <arpa/inet.h>        // for sockaddr_in

int main() {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    char buffer[1024];

    // 1. 建立 TCP socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // 2. 設定 server 位址
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    addr.sin_addr.s_addr = INADDR_ANY;

    // 3. 綁定 IP + port
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));

    // 4. 開始監聽
    listen(server_fd, 5);
    printf("Server waiting for connection...\n");

    // 5. 等待 client 連線
    client_fd = accept(server_fd, NULL, NULL);

    // 6. 接收資料
    read(client_fd, buffer, sizeof(buffer));
    printf("Received from client: %s\n", buffer);

    // 7. 傳送回應
    write(client_fd, "Hello Client", 12);

    // 8. 關閉
    close(client_fd);
    close(server_fd);
    return 0;
}
