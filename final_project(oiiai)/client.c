#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024
volatile bool Game_finished;
char Receive_from_Server_buffer[BUFFER_SIZE],Send_to_Server_buffer[BUFFER_SIZE];
volatile int Q_counter =- 1;
volatile int Q_finished[5] = {0};
volatile int TimeOut_flag = 0;
volatile int answer_value = -1;
const char *Question_Set[5][5] = {

    {
        "What should you do if someone shows signs of a stroke?",
        "1. Wait and see if they get better",
        "2. Give them painkillers and let them rest",
        "3. Call an ambulance immediately",
        "4. Massage them to help relax"
    },
    {
        "Which activity can help improve hand and arm movement after a stroke?",
        "1. Watching TV",
        "2. Playing simple games with hands",
        "3. Taking long naps",
        "4. Drinking cold water"
    },
    {
        "If a stroke patient feels tired during rehab, what should they do?",
        "1. Stop all rehab forever",
        "2. Only do rehab once a week",
        "3. Rest a bit, then continue slowly",
        "4. Drink soda for energy"
    },
    {
        "Which of the following is a healthy daily habit?",
        "1. Smoking after meals",
        "2. Watching TV all day",
        "3. Skipping breakfast",
        "4. Drinking plenty of water"
    },
    {
        "What should you do if you feel dizzy or lightheaded?",
        "1. Sit or lie down and tell someone",
        "2. Ignore it and keep walking",
        "3. Drive a car quickly",
        "4. Start running"
    },
};


void Show_Q(int n){

    if (n < 0 || n >= sizeof(Question_Set)/sizeof(Question_Set[0])) {
        printf("Invalid question number: %d\n", n);
        return;
    }

    printf("Question %d:\n", n);
    for (int i = 0; i < 5; i++) {
        printf("%s\n", Question_Set[n][i]);
    }
}

int Get_Keyboard_Value() {
    ///有輸入回傳輸入值, 沒輸入回傳-1
    struct timeval timeout;
    fd_set readfds;
    char buf[16];  // 存輸入字元
    int val;

    // 設定要監聽 stdin（鍵盤輸入）
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    // 設定 0 秒 timeout：立即回傳
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);

    if (ready > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
        // 有輸入，可以讀取
        if (fgets(buf, sizeof(buf), stdin) != NULL) {
            if (sscanf(buf, "%d", &val) == 1) {
                return val;  // 成功讀到整數
            }
        }
    }

    return -1;  // 沒輸入
}

int IMU() {
    ///IMU計數達到設定值後回傳1, 期間Q_finished就回傳-1
    int input, counter = 0;
    char buf[16];

    while (1) {
        if (Q_finished[Q_counter]) {
            return -1;
        }

        // nonblocking檢查是否有輸入  
        fd_set readfds;
        struct timeval timeout = {0, 0};  // 不等待，立即回傳

        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int result = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
        if (result > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buf, sizeof(buf), stdin) != NULL) {
                if (sscanf(buf, "%d", &input) == 1 && input == 1) {
                    counter++;
                    if (counter > 5) return 1;
                }
            }
        }

        sleep(0.1);  // 0.1 秒睡一下，避免吃太多 CPU
    }
}

void Send_Answer_Request(){
    int action = 0;
    action = IMU();
    if(action==1){
        //send answer request to Server
        memset(Send_to_Server_buffer, 0, BUFFER_SIZE);
        snprintf(Send_to_Server_buffer, BUFFER_SIZE, "Request_to_Answer 0");
        if (send(Server_fd, Send_to_Server_buffer, BUFFER_SIZE , 0) == -1) {
            printf("Something error while sending Request msg to server");
            exit(1);
        } 
    }
    else if(action==-1){
        //question _finished
        //send Q_finished to Server
        memset(Send_to_Server_buffer, 0, BUFFER_SIZE);
        snprintf(Send_to_Server_buffer, BUFFER_SIZE, "Q_finished 0");
        if (send(Server_fd, Send_to_Server_buffer, BUFFER_SIZE , 0) == -1) {
            printf("Something error while sending Request msg to server");
            exit(1);
        } 
        return;
    }
    else{
        printf("Something error in IMU()");
        exit(1);
    }
}

void TimeOut_handler(int sig) {
    TimeOut_flag = 1;
}

void* countdown_display_thread(void* arg) {
    int seconds = *(int*)arg;

    for (int i = seconds; i > 0; i--) {
        if (answer_value != -1 ) break;  // 玩家輸入時提前結束顯示

        printf("\rTime left: %2d seconds ", i);
        fflush(stdout);
        sleep(1);
    }

    printf("\r                     \r");  // 清空倒數顯示
    return NULL;
}

void Answer_Phase() {
    struct sigaction sa;
    struct itimerval timer;
    answer_value = -1;
    TimeOut_flag = 0;

    // 綁定 SIGALRM handel finction
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &TimeOut_handler;
    sigaction(SIGALRM, &sa, NULL);

    // 設定10秒倒數timer
    // 10秒一到 執行TimeOut_handler 將TimeOut_flag設為1
    timer.it_value.tv_sec = 10;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &timer, NULL);

    ///創一個thread顯示倒數
    int timeout_seconds = 10;
    pthread_t countdown_thread;
    pthread_create(&countdown_thread, NULL, countdown_display_thread, &timeout_seconds);
    pthread_detach(countdown_thread); //結束自動回收

    
    while (TimeOut_flag == 0) {
        answer_value = Get_Keyboard_Value();
        if (answer_value > 0) {
            break;  // 有輸入
        }
        sleep(0.1);  // 每次 sleep 0.1 秒減少 CPU 使用
    }

    // 清除 timer
    setitimer(ITIMER_REAL, NULL, NULL);

    if (answer_value == -1) {
        printf("Time Out! You need to resend the request \n");
    } else {
        printf("Your answer is %d\n", answer_value);
    }

    // 將所選答案發送到 server //如若超時發送-1
    memset(Send_to_Server_buffer, 0, BUFFER_SIZE);
    snprintf(Send_to_Server_buffer, BUFFER_SIZE, "ANSWER %d", answer_value);
    if (send(Server_fd, Send_to_Server_buffer, BUFFER_SIZE , 0) == -1) {
        printf("something error while sending Request msg to server");
        exit(1);
    } 
}

void* Listen_Q_finished(void *arg){
    int Server_fd2 = *(int*)arg;
    char buffer[BUFFER_SIZE];
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t len = recv(Server_fd2, buffer, BUFFER_SIZE, 0); //卡在這直到server傳Q_finished
        if (len <= 0) {
            printf("error receive message from server (port2)");
            exit(1);
        }

        char command[30];
        int index;
        if (sscanf(buffer, "%s %d", command, &index) == 2 && strcmp(command, "Q_finished") == 0) {
            if (index >= 0 && index < 5) {
              if (index==Q_counter){
                Q_finished[index] = 1;
                printf("[System] Marked Q_finished[%d] = 1 from port2\n", index);
              }
              else{

                printf("Something wrong! Q_counter = %d, Q_finished index = %d\n", Q_counter, index)
              }
            }
        } else {
            printf("[Warning] Unknown message from port2: %s\n", buffer);
        }
    }
    return NULL;
}



int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <ip> <port1> <port2>\n", argv[0]);
        exit(1);
    }
    
    const char *server_ip = argv[1];
    int server_port1 = atoi(argv[2]);
    int server_port2 = atoi(argv[3]);
  
    //////TCP part/////////
    //Socket 1
    // create TCP socket
    int Server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (Server_fd == -1) {
        perror("socket 1");
        exit(1);
    }
    // configure server address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port1);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton1");
        exit(1);
    }
    // connect to server 嘗試連線到 server
    if (connect(Server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect 1");
        exit(1);
    }
    //Socket 2
    int Server_fd2 = socket(AF_INET, SOCK_STREAM, 0);
    if (Server_fd2 == -1) {
        perror("socket2");
        exit(1);
    }

    struct sockaddr_in server_addr2;
    server_addr2.sin_family = AF_INET;
    server_addr2.sin_port = htons(server_port2);
    if (inet_pton(AF_INET, server_ip, &server_addr2.sin_addr) <= 0) {
        perror("inet_pton2");
        exit(1);
    }

    if (connect(Server_fd2, (struct sockaddr *)&server_addr2, sizeof(server_addr2)) == -1) {
        perror("connect2");
        exit(1);
    }
    ///////////////////////

    pthread_t port2_thread;
    if (pthread_create(&port2_thread, NULL, Listen_Q_finished, &Server_fd2) != 0) {
        perror("pthread_create for port2");
        exit(1);
    }
    pthread_detach(port2_thread); // 自動清除 thread

    ///////////////////////
    
    
    while(1){//new game
        /*GPT
        這裡要判斷是否連線嗎?
        recv() 回傳 0 表示對方已關閉連線
        GPT*/
        Game_finished=false;
        int num=0;
        char show[BUFFER_SIZE]={0};
        char command[30]={0};
        Q_counter =- 1;
        for (int i = 0; i < 5; i++) Q_finished[i] = 0;


        //while(Game_finished)需要保護嗎?
        //以下recv可能要改成非阻塞嗎
        while(!Game_finished){
            memset(Receive_from_Server_buffer, 0, BUFFER_SIZE);
            memset(show, 0, BUFFER_SIZE);
            memset(command, 0, 30);
            ssize_t recv_len = recv(Server_fd, Receive_from_Server_buffer, BUFFER_SIZE, 0); 
            if (recv_len <= 0) {
                perror("error receive message from server");
                exit(1);
            }

            sscanf(Receive_from_Server_buffer,"%s %d %[^\n]",command, &num, show);
            

            if(strcmp(command,"Connect")==0){
                printf("%s",show);
                ///show player ID
                continue;
            }
            else if(strcmp(command,"NewQ")==0){
                Show_Q(num);
                //show the Question and options
                if(Q_counter<0){//第一題
                    Q_counter++; //0
                }
                else if(Q_counter>=5){
                    printf("Something error about NewQ");
                }
                else {//0-3
                    Q_finished[Q_counter]=1;
                    Q_counter++; //1-4
                }
                
                Send_Answer_Request();  //等到IMU計數達標 或是 此題結束
            }
            else if(strcmp(command,"PleaseAnswer")==0){
                printf("%s",show);
                Answer_Phase(); //最多卡十秒 或 送出答案
            }
            else if(strcmp(command,"PleaseWait")==0){
                printf("%s",show);
                continue;
            }
            else if(strcmp(command,"AnswerTimeOut")==0){
                //printf("%s",show);
                Send_Answer_Request();  //等到IMU計數達標 或是 此題結束
            }
            else if(strcmp(command,"AnsweerCorrect")==0){
                printf("%s",show);
                continue;
            }
            else if(strcmp(command,"AnswerWrong")==0){
                printf("%s",show);
                Send_Answer_Request();  //等到IMU計數達標 或是 此題結束
            }
            else if(strcmp(command,"Gamefinished")==0){//最後一題結束
                printf("%s",show);
                ///show the Rank
                Q_finished[Q_counter]=1;
                if(Q_counter!=4){
                    printf("Q_counter should be 4! Something wrong!");
                }
                Game_finished=true;
                break;
            }
            else{
                printf("There's something unexpected receive from Server");
                exit(1);
            }
        }
        //此局遊戲結束
        memset(Receive_from_Server_buffer, 0, BUFFER_SIZE);
        memset(show, 0, BUFFER_SIZE);
        memset(command, 0, 30);
        /////// 顯示紀錄巴拉拉
        ssize_t recv_len = recv(Server_fd, Receive_from_Server_buffer, BUFFER_SIZE, 0);
        if (recv_len <= 0) {
            perror("error receive message from server");
            exit(1);
        }
        printf("%s",Receive_from_Server_buffer);

    }
    pthread_cancel(port2_thread);
    close(Server_fd);
    close(Server_fd2);
    return 0;
}