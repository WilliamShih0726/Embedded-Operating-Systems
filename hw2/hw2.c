#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUF_SIZE 256

// 資料結構
typedef struct { char name[20]; int price; } Food;
typedef struct { char name[20]; int distance; Food items[2]; } Shop;

// 店家資料
Shop shops[] = {
    {"Dessert shop",    3, {{"cookie",60}, {"cake",80}}},
    {"Beverage shop",   5, {{"tea",40},    {"boba",70}}},
    {"Diner",           8, {{"fried-rice",120}, {"Egg-drop-soup",50}}}
};
#define SHOP_COUNT (sizeof(shops)/sizeof(shops[0]))

int server_fd;

// 捕捉 Ctrl+C，關閉 socket 後結束
void sigint_handler(int _) {
    close(server_fd);
    exit(0);
}

// 發送商店列表
void send_shop_list(int fd) {
    char buf[BUF_SIZE] = "";
    for (int i = 0; i < SHOP_COUNT; i++) {
        Shop *s = &shops[i];
        char line[BUF_SIZE];
        snprintf(line, sizeof(line), "%s:%dkm\n- ", s->name, s->distance);
        int cnt = sizeof(s->items)/sizeof(s->items[0]);
        for (int j = 0; j < cnt; j++) {
            char item[BUF_SIZE];
            if (j < cnt - 1)
                snprintf(item, sizeof(item), "%s:$%d|", s->items[j].name, s->items[j].price);
            else
                snprintf(item, sizeof(item), "%s:$%d\n", s->items[j].name, s->items[j].price);
            strncat(line, item, sizeof(line) - strlen(line) - 1);
        }
        strncat(buf, line, sizeof(buf) - strlen(buf) - 1);
    }
    send(fd, buf, BUF_SIZE, 0);
}

// 找 food 所屬的店家索引
int find_shop_by_food(const char *food) {
    for (int i = 0; i < SHOP_COUNT; i++)
        for (int j = 0; j < 2; j++)
            if (strcmp(food, shops[i].items[j].name) == 0)
                return i;
    return -1;
}

// 在該店家中找 food 的索引
int find_food_index(Shop *s, const char *food) {
    for (int i = 0; i < 2; i++)
        if (strcmp(food, s->items[i].name) == 0)
            return i;
    return -1;
}

// 發送訂單記錄（只列出 qty>0 的項目，用 '|' 分隔）
void send_order_record(int fd, Shop *s, int qty[2]) {
    char buf[BUF_SIZE] = "";
    int printed = 0;
    for (int i = 0; i < 2; i++) {
        if (qty[i] > 0) {
            char part[BUF_SIZE];
            if (printed)
                snprintf(part, sizeof(part), "|%s %d", s->items[i].name, qty[i]);
            else
                snprintf(part, sizeof(part), "%s %d", s->items[i].name, qty[i]);
            strncat(buf, part, sizeof(buf) - strlen(buf) - 1);
            printed++;
        }
    }
    strcat(buf, "\n");
    send(fd, buf, BUF_SIZE, 0);
}

// 模擬配送
void simulate_delivery(int fd, int dist) {
    send(fd, "Please wait a few minutes...\n", BUF_SIZE, 0);
    sleep(dist);
}

// 處理單一客戶端整個流程
void handle_client(int fd) {
    char buf[BUF_SIZE];
    bool ordered = false;
    int shop_idx = -1;
    int qty[2] = {0, 0};

    while (recv(fd, buf, BUF_SIZE, 0) > 0) {
        // 去掉可能的換行
        buf[strcspn(buf, "\r\n")] = 0;

        if (strcmp(buf, "shop list") == 0) {
            send_shop_list(fd);

        } else if (strncmp(buf, "order ", 6) == 0) {
            char food[20];
            int num;
            sscanf(buf + 6, "%19s %d", food, &num);

            if (!ordered) {
                shop_idx = find_shop_by_food(food);
                ordered = true;
            }
            Shop *s = &shops[shop_idx];
            int idx = find_food_index(s, food);
            if (idx >= 0) qty[idx] += num;
            send_order_record(fd, s, qty);

        } else if (strcmp(buf, "confirm") == 0) {
            if (ordered) {
                int dist = shops[shop_idx].distance;
                simulate_delivery(fd, dist);
                int total = shops[shop_idx].items[0].price * qty[0]
                          + shops[shop_idx].items[1].price * qty[1];
                char msg[BUF_SIZE];
                snprintf(msg, sizeof(msg),
                         "Delivery has arrived and you need to pay %d$\n",
                         total);
                send(fd, msg, BUF_SIZE, 0);
                break;
            } else {
                send(fd, "Please order some meals\n", BUF_SIZE, 0);
            }

        } else if (strcmp(buf, "cancel") == 0) {
            break;

        } else {
            send(fd, "Invalid command\n", BUF_SIZE, 0);
        }
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./hw2 <port>\n");
        exit(1);
    }
    signal(SIGINT, sigint_handler);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(atoi(argv[1]));

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 1);

    while (1) {
        int fd = accept(server_fd, NULL, NULL);
        handle_client(fd);
    }
    return 0;
}
