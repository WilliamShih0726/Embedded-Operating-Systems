#include <stdio.h>      // 提供輸出入函式如 printf()
#include <stdlib.h>     // 提供 exit()、atoi() 等函式
#include <string.h>     // 提供字串處理函式如 strcpy()
#include <signal.h>     // 提供 signal 與 sigaction 處理
#include <sys/shm.h>    // 提供共享記憶體 shm 系列函式
#include <sys/ipc.h>    // 提供 IPC 通訊 key 型別
#include <unistd.h>     // 提供 getpid(), pause(), 等系統函式

// 定義共享記憶體資料格式：猜測的數字與回應結果
typedef struct {
    int guess;          // 猜測的數字
    char result[8];     // 回應字串："Bigger"、"Smaller" 或 "Bingo"
} data;

// 全域變數：共享記憶體指標、shmid（識別碼）、target（正確答案）
data *shared_data;
int shmid;
int target;

// SIGUSR1 的 handler：當接收到猜測請求時，進行數值比較並寫入結果
void sigusr1_handler(int signum) {
    int g = shared_data->guess;  // 讀取猜測值

    if (g < target) {
        strcpy(shared_data->result, "Bigger");  // 若太小，回傳 "Bigger"
        printf("[Game] Guess %d, result: Bigger\n", g);
    } else if (g > target) {
        strcpy(shared_data->result, "Smaller"); // 若太大，回傳 "Smaller"
        printf("[Game] Guess %d, result: Smaller\n", g);
    } else {
        strcpy(shared_data->result, "Bingo");   // 若猜中，回傳 "Bingo"
        printf("[Game] Guess %d, result: Bingo\n", g);
    }
}

// SIGINT 的 handler：當 Ctrl+C 被按下時，清除共享記憶體資源
void cleanup(int signum) {
    printf("\n[Game] Caught SIGINT. Cleaning shared memory...\n");

    // 解除與共享記憶體的連結
    if (shmdt(shared_data) == -1) {
        perror("shmdt");
    }

    // 標記共享記憶體刪除（當所有附加者離開後，系統會自動刪除）
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
    }

    exit(0);  // 結束程式
}

// 主程式進入點
int main(int argc, char *argv[]) {
    // 檢查參數數量是否正確（需要 key 與目標數字）
    if (argc != 3) {
        fprintf(stderr, "Usage: ./game <key> <target_number>\n");
        exit(1);
    }

    // 將命令列參數轉為 key 與 target 整數
    key_t key = atoi(argv[1]);
    target = atoi(argv[2]);

    // 建立共享記憶體區段（若不存在則建立，權限為 0666）
    shmid = shmget(key, sizeof(data), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }
    printf("[Game] Shared memory ID (shmid): %d\n", shmid);

    // 附加共享記憶體到本程式的記憶體空間
    shared_data = (data *)shmat(shmid, NULL, 0);
    if ((void *)shared_data == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    // 設定 SIGUSR1 的處理函式（用於猜測通知）
    struct sigaction sa;
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    // 設定 Ctrl+C (SIGINT) 的處理函式，用於清除共享記憶體
    signal(SIGINT, cleanup);

    // 顯示本程式的 PID（供 guess 程式傳送訊號用）
    printf("[Game] PID: %d\n", getpid());
    printf("[Game] Waiting for guesses...\n");

    // 進入無限等待狀態，每次收到 signal 時會暫停被中斷
    while (1) {
        pause();  // 等待 signal
    }

    return 0;
}
