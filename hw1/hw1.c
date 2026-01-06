#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 定義一間店最多有兩種餐點，總共有三間店
#define MAX_ITEM 2
#define MAX_SHOP 3

// 餐點結構：名稱、價格、已點份數
typedef struct {
    char name[32]; //若要用char name[]需在struct最後一項，且需要搭配 malloc，較複雜
    int price;
    int quantity;  // 目前已點的份數（會累加）
} MenuItem;

// 餐廳結構：名稱、距離、內含兩個 MenuItem
typedef struct {
    char name[32];
    int distance;         // 用來決定點亮幾顆 LED
    MenuItem items[MAX_ITEM]; // 餐點資訊
} Shop;

// 宣告三間餐廳與其餐點與價格
Shop shops[MAX_SHOP] = {
    { "Dessert shop", 3, { {"cookie", 60, 0}, {"cake", 80, 0} } },
    { "Beverage shop", 5, { {"tea", 40, 0}, {"boba", 70, 0} } },
    { "Diner", 8, { {"fried rice", 120, 0}, {"egg-drop soup", 50, 0} } }
};

// 顯示主選單，提供 shop list 與 order 兩個選項
void show_main_menu() {
    printf("\n=== Main Menu ===\n");
    printf("1. shop list\n");
    printf("2. order\n");
    printf("Choose: ");
}

// 顯示三間商店與其距離
void show_shop_list() {
    printf("\n=== Shop List ===\n");
    for (int i = 0; i < MAX_SHOP; i++) {
        printf("%d. %s: %dkm\n", i + 1, shops[i].name, shops[i].distance);
    }
    printf("Press Enter to return...");
    getchar(); getchar(); // 吃掉 scanf 殘留的 '\n'，再等一次 Enter 回主選單
}

// 訂餐流程，處理選店、點餐、取消與確認的邏輯
void order_flow() {
    int shop_choice;

    // 顯示餐廳列表讓使用者選擇
    printf("\nPlease choose from 1~3:\n");
    for (int i = 0; i < MAX_SHOP; i++) {
        printf("%d. %s\n", i + 1, shops[i].name);
    }

    printf("Choice: ");
    scanf("%d", &shop_choice);

    if (shop_choice < 1 || shop_choice > MAX_SHOP) return; // 錯誤輸入

    Shop *shop = &shops[shop_choice - 1]; // 取得使用者選擇的店家指標

    // 初始化此筆訂單（將先前的點餐數量清零）
    for (int i = 0; i < MAX_ITEM; i++) {
        shop->items[i].quantity = 0;
    }

    int choice;
    while (1) {
        // 顯示點餐選單
        printf("\nPlease choose from 1~4\n");
        for (int i = 0; i < MAX_ITEM; i++) {
            printf("%d. %s: $%d (current: %d)\n", i + 1, shop->items[i].name,
                   shop->items[i].price, shop->items[i].quantity);
        }
        printf("3. confirm\n");
        printf("4. cancel\n");
        printf("Choice: ");
        scanf("%d", &choice);

        if (choice >= 1 && choice <= 2) {
            // 點餐：輸入數量並累加
            int qty;
            printf("How many? ");
            scanf("%d", &qty);
            shop->items[choice - 1].quantity += qty;
        }
        else if (choice == 3) { // 確認訂單
            int total = 0;

            // 計算總金額
            for (int i = 0; i < MAX_ITEM; i++) {
                total += shop->items[i].price * shop->items[i].quantity;
            }

            if (total == 0) {
                printf("You haven't ordered anything.\n");
                continue; // 若沒點東西則留在訂餐選單
            }

            printf("Please wait for a few minutes...\n");

            // 呼叫 writer 程式控制 7-segment 顯示總金額
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "./standalong_delivery_writer 7seg %d", total);
            system(cmd);

            // 呼叫 writer 程式控制 LED 顯示外送距離
            snprintf(cmd, sizeof(cmd), "./standalong_delivery_writer led %d", shop->distance);
            system(cmd);

            // 模擬送餐完成
            printf("Please pick up your meal!\n");
            printf("Press Enter to return...");
            getchar(); getchar(); // 防止 scanf 遺留換行影響輸入
            break; // 回主選單
        }
        else if (choice == 4) {
            printf("Order cancelled.\n");
            break; // 回主選單
        }
        else {
            printf("Invalid choice.\n");
        }
    }
}

// 主程式迴圈，持續顯示主選單與處理輸入
int main() {
    int input;
    while (1) {
        show_main_menu(); // 顯示主選單
        scanf("%d", &input);

        if (input == 1) {
            show_shop_list(); // 顯示餐廳列表
        }
        else if (input == 2) {
            order_flow();     // 進入訂餐流程
        }
        else {
            printf("Invalid choice.\n");
        }
    }
    return 0;
}
