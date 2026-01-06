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

#include <sys/sem.h>
#include <sys/types.h>
#include <sys/ipc.h>

#define BUFFER_SIZE 1024
#define key 11223344
#define Player_num 2
volatile int semid = -1;
int Server_fd = -1,  Server_fd2 = -1;
int Port2_Client_fd[10];  // 假設最多支援 10 個 client
int Port2_Client_count = 0;
volatile bool Game_finished;
int Q_finished[5] = {0};
volatile bool player_ready = false;
int closed_player_count = 0;
pthread_mutex_t close_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t all_closed_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t Score_lock = PTHREAD_MUTEX_INITIALIZER;
volatile int Answer_Set[21] = {3,2,3,4,1,4,2,1,2,3,2,3,3,2,1,2,2,1,4,1,4};
volatile int Question_index_Set[5] = {0};
int Score[Player_num] = {0};
int PlayerScoreTime[Player_num] = {0};  // PlayerScoreTime[i]表示 player (i+1) 的最後一分是第幾個時間點得到的
int global_timestamp = 1;

typedef struct {
    int client_fd;
    int player_id;
} client_info_t;


void Clean(int sig) {
    if (semctl(semid, 0, IPC_RMID) < 0) {
        perror("semctl");
        exit(1);
    }
    if (Server_fd != -1) {
        close(Server_fd);
    }
    if (Server_fd2 != -1) {
        close(Server_fd2);
    }
    exit(0);
}
/*
int compare(const void *a, const void *b) {
    int i = *(int *)a;
    int j = *(int *)b;
    if (Score[j] == Score[i]) return 0;  // 分數相同，ID 小的排前面

    return (Score[j] - Score[i]);  // 分數高的排前面
 
}
*/
int compare(const void *a, const void *b) {
    int i = *(int *)a;
    int j = *(int *)b;

    if (Score[i] != Score[j])
        return Score[j] - Score[i];  // 分數高的排前面

    // 分數相同 → 誰先累積到這個分數，誰排前面
    int same_score = Score[i];

    if (PlayerScoreTime[i] < PlayerScoreTime[j]) return -1;
    if (PlayerScoreTime[i] > PlayerScoreTime[j]) return 1;


    return 0;
}

void generate_random_questions() {
    int pool[20];
    for (int i = 0; i < 20; i++) {
        pool[i] = i + 1;  // 數字從 1~20
    }

    // Fisher-Yates Shuffle
    for (int i = 19; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = pool[i];
        pool[i] = pool[j];
        pool[j] = temp;
    }

    // 取前 5 個隨機不重複的題號
    for (int i = 0; i < 5; i++) {
        Question_index_Set[i] = pool[i];
    }
}

void *handle_Client(void *arg) {
    client_info_t *info = (client_info_t *)arg;
    int client_fd = info->client_fd;
    int player_id = info->player_id;
    free(info);  // 釋放記憶體

    static int last_answered = 0;   // <<== 只要加這行
    struct sembuf Request, Release ;
    Request.sem_num = 0;
    Request.sem_op = -1;
    Request.sem_flg = SEM_UNDO;
    Release.sem_num = 0;
    Release.sem_op = 1;
    Release.sem_flg = SEM_UNDO;

    int answer = -1;
    int Q_counter = -1;
    char  command[30];
    char  Send_to_Client_buffer[BUFFER_SIZE];
    char  Receive_from_Client_buffer[BUFFER_SIZE];
    memset(Send_to_Client_buffer, 0, BUFFER_SIZE);
    snprintf(Send_to_Client_buffer, BUFFER_SIZE, "Connect 0 Connect success! You are Player %d!\n", player_id);
    send(client_fd, Send_to_Client_buffer, strlen(Send_to_Client_buffer), 0);

    printf("[Server] Player %d connected (fd=%d)\n", player_id, client_fd);

    // 後續處理這個 client 傳來的訊息
    // 例如 recv + 判斷 Request_to_Answer 等等...

    // 等待 player_ready 成為 true
    while (!player_ready) {
        sleep(0.1);  // 每 0.1 秒檢查一次
    }
    //memset(Score, 0, sizeof(Score));
    while(Q_finished[4]==0){
        //send neweQ
        if(Q_counter==-1){//-1 第一題
            Q_counter++; //0
            memset(Send_to_Client_buffer, 0, BUFFER_SIZE);
            snprintf(Send_to_Client_buffer, BUFFER_SIZE, "NewQ %d \n", Question_index_Set[Q_counter]);
            send(client_fd, Send_to_Client_buffer, strlen(Send_to_Client_buffer), 0);
        }
        else if(Q_counter<5){//0-4 第2-5題
            //pthread_mutex_lock(&Score_lock);
            //printf("[Debug] \n Player1 Score: %d \n Player2 Score: %d\n", Score[0],Score[1]);
            //pthread_mutex_unlock(&Score_lock);
            Q_counter++; 
            
            int index[Player_num];
            for (int i = 0; i < Player_num; i++) {
                index[i] = i;  // index[i] = i 對應 playerid = i+1
            }
            // 對 index[] 排序，但比較的是 Score[]
            qsort(index, Player_num, sizeof(int), compare);
            // 找此 client 的排名
            int my_rank = -1;
            for (int rank = 0; rank < Player_num; rank++) {
                if (index[rank] == player_id - 1) {
                    my_rank = rank + 1;
                    break;
                }
            }
            //塞ranking資訊 @ @
            memset(Send_to_Client_buffer, 0, BUFFER_SIZE);
            snprintf(Send_to_Client_buffer, BUFFER_SIZE,
                "NewQ %d Your current rank is: #%d\nRanking:\n",
                Question_index_Set[Q_counter], my_rank);

            for (int rank = 0; rank < Player_num; rank++) {
                int id = index[rank] + 1;;
            }

            // 把所有人的排名一行一行 strcat 加進 buffer
            for (int rank = 0; rank < Player_num; rank++) {
                int id = index[rank] + 1;
                char line[128];
                //pthread_mutex_lock(&Score_lock);
                snprintf(line, sizeof(line), "Rank %d: Player %d (Score: %d)\n",
                        rank + 1, id, Score[index[rank]]);
                //pthread_mutex_unlock(&Score_lock);
                strncat(Send_to_Client_buffer, line, BUFFER_SIZE - strlen(Send_to_Client_buffer) - 1);
            }
            send(client_fd, Send_to_Client_buffer, strlen(Send_to_Client_buffer), 0);
        }
        else {
            printf("Something error about NewQ");
        }
        while(1){///wait client request to answer
            if(Q_counter == 4 && Q_finished[4] == 1) { // 如果已經有一個 client 完成了最後一題
                // 但自己還沒處理過（例如還沒作答或還沒收到 Q_finished），也讓自己走一次最後一題流程
                if (!last_answered) {
                    // 強制讓自己進到送分數、顯示 final ranking 的區塊
                    // 可以直接跳到 break while，再進行遊戲結束流程
                    last_answered = 1;
                    break;
                }
            }
            if(Q_finished[Q_counter]==1) break;
            memset(Receive_from_Client_buffer, 0, BUFFER_SIZE);
            ssize_t recv_len = recv(client_fd, Receive_from_Client_buffer, BUFFER_SIZE, 0);
            if (recv_len < 0) {
                perror("error receive message from client");
                exit(1);
            }
            if (recv_len == 0) {
                break;
            }
            else if(recv_len > 0){
                //收到Request_to_Answer->指派semaphore
                //收到Q_finished ->跳過
                memset(command, 0, 30);
                sscanf(Receive_from_Client_buffer,"%s ",command);
                if(strcmp(command,"Request_to_Answer")==0){
                    // request semaphore //blocking type
                    if (semop(semid, &Request, 1)  == -1) {
                        perror("semop_request");
                        return NULL;
                    }

                    /***************************************/
                    /*******into critical section***********/
                    /***************************************/

                    ///拿到semaphore 如果題目已經結束-> break
                    if(Q_finished[Q_counter]==1){
                        if (semop(semid, &Release, 1)  == -1) {
                            perror("semop_release");
                            return NULL;
                        }
                        break;
                    } 

                    //通知client作答
                    memset(Send_to_Client_buffer, 0, BUFFER_SIZE);
                    snprintf(Send_to_Client_buffer, BUFFER_SIZE, "PleaseAnswer %d Please press the button to answer.\n", Q_counter);
                    send(client_fd, Send_to_Client_buffer, strlen(Send_to_Client_buffer), 0);
                    //boardcase to other clients "Player n is answering...."?
                    //PleaseWait ?


                    //收client的回答
                    memset(Receive_from_Client_buffer, 0, BUFFER_SIZE);
                    recv_len = recv(client_fd, Receive_from_Client_buffer, BUFFER_SIZE, 0);
                    if (recv_len < 0) {
                        perror("error receive message from client");
                        exit(1);
                    }
                    memset(command, 0, 30);
                    sscanf(Receive_from_Client_buffer,"%s %d",command, &answer);
                    if(strcmp(command,"ANSWER")==0){
                        if(answer == -1){ ///answer time out
                            memset(Send_to_Client_buffer, 0, BUFFER_SIZE);
                            snprintf(Send_to_Client_buffer, BUFFER_SIZE, "AnswerTimeOut 0 \n");
                            send(client_fd, Send_to_Client_buffer, strlen(Send_to_Client_buffer), 0);
                        }
                        else if(answer == Answer_Set[Question_index_Set[Q_counter]]){ //answer correct //Q_finished
                            //printf("[Debug] \n");
                            int ind;
                            ind = player_id-1;
                            //printf("[Debug] ind = %d\n",ind);
                            //pthread_mutex_lock(&Score_lock);
                            //printf("[Debug] Before adding. Score[%d] = %d\n", ind, Score[ind]);
                            Score[ind]+=1;
                            PlayerScoreTime[ind]= global_timestamp++;

                            // 1. 宣告和初始化 index 陣列
                            int index[Player_num];
                            for (int i = 0; i < Player_num; i++) {
                                index[i] = i;
                            }
                            qsort(index, Player_num, sizeof(int), compare);
                            
                            //printf("[Debug] After Player%d get 1 point. Score: %d\n", player_id, Score[ind]);
                            //pthread_mutex_unlock(&Score_lock);
                            Q_finished[Q_counter]=1;
                            ///port2 boardcase to other clients Q_finished[Q_counter]=1
                            
                            char port2_msg[BUFFER_SIZE];
                            snprintf(port2_msg, BUFFER_SIZE, "Q_finished %d %d\n", Q_counter, player_id);

                            for (int i = 0; i < Port2_Client_count; i++) {
                                send(Port2_Client_fd[i], port2_msg, strlen(Send_to_Client_buffer), 0);
                            }
                            
                            memset(Send_to_Client_buffer, 0, BUFFER_SIZE);
                            snprintf(Send_to_Client_buffer, BUFFER_SIZE, "AnswerCorrect 0 \n");
                            send(client_fd, Send_to_Client_buffer, strlen(Send_to_Client_buffer), 0);
                            // 2. 組出分數排名字串並送出
                            memset(Send_to_Client_buffer, 0, BUFFER_SIZE);
                            strcpy(Send_to_Client_buffer, "Ranking:\n");
                            for (int rank = 0; rank < Player_num; rank++) {
                                int id = index[rank] + 1;
                                char line[128];
                                snprintf(line, sizeof(line), "Rank %d: Player %d (Score: %d)\n",
                                        rank + 1, id, Score[index[rank]]);
                                strncat(Send_to_Client_buffer, line, BUFFER_SIZE - strlen(Send_to_Client_buffer) - 1);
                            }
                            send(client_fd, Send_to_Client_buffer, strlen(Send_to_Client_buffer), 0);
                        }
                        else{ //answer wrong
                            memset(Send_to_Client_buffer, 0, BUFFER_SIZE);
                            snprintf(Send_to_Client_buffer, BUFFER_SIZE, "AnswerWrong 0 Your answer is wrong, You need to resend the answer request\n");
                            send(client_fd, Send_to_Client_buffer, strlen(Send_to_Client_buffer), 0);
                        }

                    }
                    else{
                        printf("Answer command wrong from client");
                        exit(1);
                    }

                    
                    if (semop(semid, &Release, 1)  == -1) {
                        perror("semop_release");
                        return NULL;
                    }

                    /***************************************/
                    /******** exit critical section ********/
                    /***************************************/
                    
                }
                else if(strcmp(command,"Q_finished")==0){
                    usleep(500*1000); // 0.3 秒，確保 client 有時間收到最後一題排名
                    break;
                }
            }
            sleep(0.2);
        }
    }
    ///遊戲結束
    // ======== 遊戲結束 ========
    int index[Player_num];
    for (int i = 0; i < Player_num; i++) index[i] = i;
    qsort(index, Player_num, sizeof(int), compare);

    int my_rank = -1;
    for (int rank = 0; rank < Player_num; rank++) {
        if (index[rank] == player_id - 1) {
            my_rank = rank + 1;
            break;
        }
    }
    memset(Send_to_Client_buffer, 0, BUFFER_SIZE);
    snprintf(Send_to_Client_buffer, BUFFER_SIZE,
        "Gamefinished 0 Your final rank is: #%d\nRanking:\n", my_rank);
    for (int rank = 0; rank < Player_num; rank++) {
        int id = index[rank] + 1;
        char line[128];
        snprintf(line, sizeof(line), "Rank %d: Player %d (Score: %d)\n",
                rank + 1, id, Score[index[rank]]);
        strncat(Send_to_Client_buffer, line, BUFFER_SIZE - strlen(Send_to_Client_buffer) - 1);
    }
    send(client_fd, Send_to_Client_buffer, strlen(Send_to_Client_buffer), 0);
    close(client_fd);


    pthread_mutex_lock(&close_lock);
    closed_player_count++;
    if (closed_player_count == Player_num) {
        pthread_cond_signal(&all_closed_cond);  // 所有人離開了，通知主程式
    }
    pthread_mutex_unlock(&close_lock);


    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port1> <port2>\n", argv[0]);
        exit(1);
    }
    int server_port1 = atoi(argv[1]);
    int server_port2 = atoi(argv[2]);
    signal(SIGINT, Clean);

    //////TCP part/////////
    int yes = 1;
    //Socket 1
    int Server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (Server_fd == -1) {
        perror("socket");
        exit(1);
    }
    // allow reuse of address
    setsockopt(Server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    //建立server地址結構
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(argv[1]));

    //把 socket 綁定到指定的 port & IP 上
    if (bind(Server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(Server_fd);
        exit(1);
    }
    //開始聽這個 socket，最多允許 10 個
    if (listen(Server_fd, 10) < 0) {
        perror("listen");
        close(Server_fd);
        exit(1);
    }

    //Socket 2

    int Server_fd2 = socket(AF_INET, SOCK_STREAM, 0);
    if (Server_fd2 == -1) {
        perror("socket2");
        exit(1);
    }
    // allow reuse
    setsockopt(Server_fd2, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    
    struct sockaddr_in server_addr2;
    server_addr2.sin_family = AF_INET;
    server_addr2.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr2.sin_port = htons(server_port2);
    
    if (bind(Server_fd2, (struct sockaddr *)&server_addr2, sizeof(server_addr2)) < 0) {
        perror("bind2");
        close(Server_fd2);
        exit(1);
    }
    
    if (listen(Server_fd2, 10) < 0) {
        perror("listen2");
        close(Server_fd2);
        exit(1);
    }


    // set signal handler crtl+c -> Clean
    signal(SIGINT, Clean);

    // initial semaphore
    if ((semid = semget(key, 1, IPC_CREAT | 0666)) == -1) {
        perror("semget");
        exit(1);
    }
    if (semctl(semid, 0, SETVAL, 1) == -1) {//把 semaphore 的初始值設成 1
        perror("semctl");
        exit(1);
    }
    /////////

    
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while(1){ 
        //New game
        memset(Q_finished, 0, sizeof(Q_finished));
        memset(Score, 0, sizeof(Score));
        memset(Port2_Client_fd, 0, sizeof(Port2_Client_fd));
        Game_finished = false;
        player_ready = false;
        int player_count = 0;
        closed_player_count = 0;
        Port2_Client_count = 0;
        srand(time(NULL));
        generate_random_questions();
        global_timestamp=1;


        while (player_count < Player_num) {
            int client_fd = accept(Server_fd, (struct sockaddr *)&client_addr, &client_len);
            if (client_fd == -1) {
                perror("accept");
                continue;
            }
            
            int client2_fd = accept(Server_fd2, (struct sockaddr *)&client_addr, &client_len);
            if (client2_fd == -1) {
                perror("accept port2");
                continue;
            }
            Port2_Client_fd[Port2_Client_count++] = client2_fd;

            player_count++; 
    
            // 建立一個 thread 處理這個 client
            // 分配記憶體給 thread 參數
            client_info_t *info = malloc(sizeof(client_info_t));
            info->client_fd = client_fd;
            info->player_id = player_count;
    
            pthread_t tid;
            pthread_create(&tid, NULL, handle_Client, info);
            pthread_detach(tid);
    
            if(player_count >= Player_num){
                player_ready = true;
                printf("[Server] 玩家已到齊，開始遊戲\n");
            } 

        }

        ///wait all player close client_fd
        // 等待所有 client thread 結束
        pthread_mutex_lock(&close_lock);
        while (closed_player_count < Player_num) {
            pthread_cond_wait(&all_closed_cond, &close_lock);
        }
        pthread_mutex_unlock(&close_lock);
        for (int i = 0; i < Port2_Client_count; i++) {
            close(Port2_Client_fd[i]);
        }
        printf("[Server] 所有玩家已離線，重新開始新的一局\n");

    }
    
    

    return 0;
}