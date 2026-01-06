#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>

#define BUF_SIZE 256

typedef struct { char name[20]; int price; } Food;
typedef struct { char name[20]; int distance; Food items[2]; } Shop;

Shop shops[] = {
    {"Dessert shop",    3, {{"cookie",60}, {"cake",80}}},
    {"Beverage shop",   5, {{"tea",40},    {"boba",70}}},
    {"Diner",           8, {{"fried-rice",120}, {"Egg-drop-soup",50}}}
};
#define SHOP_COUNT (sizeof(shops)/sizeof(shops[0]))

int server_fd;

// 外送員資料
typedef struct {
    int remain_time;      // 外送員還需送餐的剩餘時間（單位：秒）
    pthread_mutex_t lock;
} DeliveryMan;

DeliveryMan deliverymen[2];

pthread_mutex_t order_mutex = PTHREAD_MUTEX_INITIALIZER;

// 捕捉 Ctrl+C
void sigint_handler(int _) {
    close(server_fd);
    exit(0);
}

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

int find_shop_by_food(const char *food) {
    for (int i = 0; i < SHOP_COUNT; i++)
        for (int j = 0; j < 2; j++)
            if (strcmp(food, shops[i].items[j].name) == 0)
                return i;
    return -1;
}

int find_food_index(Shop *s, const char *food) {
    for (int i = 0; i < 2; i++)
        if (strcmp(food, s->items[i].name) == 0)
            return i;
    return -1;
}

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

typedef struct {
    int fd;
} ThreadArg;

// 這個結構保存每個客戶端的訂單暫存
typedef struct {
    bool ordered;
    int shop_idx;
    int qty[2];
    int order_distance;
    int order_cost;
    bool is_waiting_long; // 超過 30 秒需等待客戶確認
    bool confirmed;       // 客戶已回覆 Yes
} OrderInfo;

// 配送指派，返回分配的外送員 index 及等待時間
int assign_deliveryman(int meal_time) {
    pthread_mutex_lock(&order_mutex);
    int chosen = 0;
    if (deliverymen[1].remain_time < deliverymen[0].remain_time)
        chosen = 1;
    int wait_time = deliverymen[chosen].remain_time + meal_time;
    pthread_mutex_unlock(&order_mutex);
    return chosen;
}

// 更新指派外送員的剩餘時間
void update_deliveryman(int man_idx, int meal_time) {
    pthread_mutex_lock(&order_mutex);
    deliverymen[man_idx].remain_time += meal_time;
    pthread_mutex_unlock(&order_mutex);
}

// 外送員剩餘時間倒數
void* deliveryman_timer(void* arg) {
    while (1) {
        sleep(1);
        pthread_mutex_lock(&order_mutex);
        for (int i = 0; i < 2; i++) {
            if (deliverymen[i].remain_time > 0)
                deliverymen[i].remain_time--;
        }
        pthread_mutex_unlock(&order_mutex);
    }
    return NULL;
}

void* handle_client(void* arg) {
    int fd = ((ThreadArg*)arg)->fd;
    free(arg);

    char buf[BUF_SIZE];
    OrderInfo order = {0}; // 初始化所有欄位為 0/false
    int deliveryman_idx = -1; // 哪位外送員會送這單

    while (recv(fd, buf, BUF_SIZE, 0) > 0) {
        buf[strcspn(buf, "\r\n")] = 0;

        if (strcmp(buf, "shop list") == 0) {
            send_shop_list(fd);

        } else if (strncmp(buf, "order ", 6) == 0) {
            char food[20];
            int num;
            sscanf(buf + 6, "%19s %d", food, &num);

            if (!order.ordered) {
                order.shop_idx = find_shop_by_food(food);
                order.ordered = true;
            }
            Shop *s = &shops[order.shop_idx];
            int idx = find_food_index(s, food);
            if (idx >= 0) order.qty[idx] += num;
            send_order_record(fd, s, order.qty);

        } else if (strcmp(buf, "confirm") == 0) {
            if (!order.ordered) {
                send(fd, "Please order some meals\n", BUF_SIZE, 0);
                continue;
            }
            // 計算本單所需配送時間與金額
            Shop *s = &shops[order.shop_idx];
            order.order_distance = s->distance;
            order.order_cost = s->items[0].price * order.qty[0]
                             + s->items[1].price * order.qty[1];

            // 指派給等待較短的外送員
            pthread_mutex_lock(&order_mutex);
            int wait_time[2] = {
                deliverymen[0].remain_time + order.order_distance,
                deliverymen[1].remain_time + order.order_distance
            };
            deliveryman_idx = (wait_time[1] < wait_time[0]) ? 1 : 0;
            int total_wait = wait_time[deliveryman_idx];
            pthread_mutex_unlock(&order_mutex);

            if (total_wait > 30) {
                order.is_waiting_long = true;
                send(fd, "Your delivery will take a long time, do you want to wait?\n", BUF_SIZE, 0);
            } else {
                // 直接配送
                send(fd, "Please wait a few minutes...\n", BUF_SIZE, 0);
                update_deliveryman(deliveryman_idx, order.order_distance);
                sleep(order.order_distance);
                char msg[BUF_SIZE];
                snprintf(msg, sizeof(msg),
                         "Delivery has arrived and you need to pay %d$\n", order.order_cost);
                send(fd, msg, BUF_SIZE, 0);
                break;
            }

        } else if (order.is_waiting_long &&
                   (strcmp(buf, "Yes") == 0 || strcmp(buf, "yes") == 0)) {
            // 顧客確認願意等待
            order.confirmed = true;
            send(fd, "Please wait a few minutes...\n", BUF_SIZE, 0);
            update_deliveryman(deliveryman_idx, order.order_distance);
            sleep(order.order_distance);
            char msg[BUF_SIZE];
            snprintf(msg, sizeof(msg),
                     "Delivery has arrived and you need to pay %d$\n", order.order_cost);
            send(fd, msg, BUF_SIZE, 0);
            break;

        } else if (order.is_waiting_long &&
                   (strcmp(buf, "No") == 0 || strcmp(buf, "no") == 0)) {
            // 不願意等待，直接結束
            break;

        } else if (strcmp(buf, "cancel") == 0) {
            break;

        } else {
            send(fd, "Invalid command\n", BUF_SIZE, 0);
        }
    }
    close(fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./hw3 <port>\n");
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
    listen(server_fd, 16);

    // 外送員倒數計時 thread
    for (int i = 0; i < 2; i++) {
        deliverymen[i].remain_time = 0;
        pthread_mutex_init(&deliverymen[i].lock, NULL);
    }
    pthread_t timer_thread;
    pthread_create(&timer_thread, NULL, deliveryman_timer, NULL);
    pthread_detach(timer_thread);

    while (1) {
        int fd = accept(server_fd, NULL, NULL);
        ThreadArg* arg = malloc(sizeof(ThreadArg));
        arg->fd = fd;
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, arg);
        pthread_detach(tid);
    }
    return 0;
}
