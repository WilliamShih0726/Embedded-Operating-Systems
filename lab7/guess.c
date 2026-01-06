#include <stdio.h>        // 提供輸出入函式，例如 printf
#include <stdlib.h>       // 提供 atoi, exit 等函式
#include <string.h>       // 提供字串處理函式，例如 strcmp, strcpy
#include <signal.h>       // 提供 signal 與 sigaction 等訊號處理函式
#include <sys/shm.h>      // 提供共享記憶體操作函式 shmget, shmat
#include <sys/ipc.h>      // 提供 IPC 機制與 key 型別
#include <unistd.h>       // 提供 sleep, usleep, kill 等系統函式
#include <sys/time.h>     // 提供 setitimer() 及相關計時器結構

// 定義共享記憶體結構，包含猜測值與猜測結果回應
typedef struct {
    int guess;            // 要猜的數字
    char result[8];       // Game 回傳的結果： "Bigger"、"Smaller"、"Bingo"
} data;

// 全域變數：共享記憶體指標、猜測上下界與 Game 的 PID
data *shared_data;
int lower = 1, upper;
pid_t game_pid;

// timer 觸發時執行的 handler，每秒自動猜一次
void timer_handler(int signum) {
    if (lower > upper) {
        // 若搜尋範圍已無效，表示邏輯錯誤，直接結束
        printf("[guess] Search range invalid. Exiting.\n");
        exit(1);
    }

    int mid = (lower + upper) / 2;       // 使用二分搜尋猜中間的數字
    shared_data->guess = mid;            // 寫入猜測數字至 shared memory
    printf("[game] Guess: %d\n", mid);   // 印出猜測紀錄（為了 debug/demo）

    kill(game_pid, SIGUSR1);             // 傳送 SIGUSR1 通知 Game 進行判斷
    usleep(100000);                      // 等待 0.1 秒讓 Game 寫入結果

    // 根據 Game 的回應更新猜測範圍
    if (strcmp(shared_data->result, "Bigger") == 0) {
        lower = mid + 1;                 // 太小 → 往上猜
    } else if (strcmp(shared_data->result, "Smaller") == 0) {
        upper = mid - 1;                 // 太大 → 往下猜
    } else if (strcmp(shared_data->result, "Bingo") == 0) {
        printf("[guess] Bingo! The answer is %d\n", mid);  // 猜中 → 印出並結束
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    // 檢查參數格式是否正確：需有 key、最大值、Game 的 PID
    if (argc != 4) {
        fprintf(stderr, "Usage: ./guess <key> <upper_bound> <game_pid>\n");
        exit(1);
    }

    key_t key = atoi(argv[1]);         // 解析共享記憶體 key
    upper = atoi(argv[2]);            // 設定猜測的最大值
    game_pid = atoi(argv[3]);         // 取得 Game 程式的 PID

    // 印出遊戲資訊（Demo 或除錯用途）
    printf("[guess] Game PID: %d\n", game_pid);
    printf("[guess] Start guessing every second...\n");

    // 根據 key 取得共享記憶體 ID
    int shmid = shmget(key, sizeof(data), 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    // 將共享記憶體附加至本程式空間
    shared_data = (data *)shmat(shmid, NULL, 0);
    if ((void *)shared_data == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    // 設定 SIGALRM 處理函式為 timer_handler（每秒自動猜一次）
    struct sigaction sa;
    sa.sa_handler = timer_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    // 使用 setitimer 設定 timer：每秒觸發一次 SIGALRM
    struct itimerval timer;
    timer.it_value.tv_sec = 1;        // 第一次觸發間隔
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 1;     // 後續每次觸發間隔
    timer.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &timer, NULL);  // 啟動實時計時器

    while (1);  // 主程式持續等待 timer 觸發，不進行其他動作
}
