import socket
import sys
import select
import time
import threading
import signal
from threading import Lock

import matplotlib.pyplot as plt

import smbus2
import pygame
import os
import RPi.GPIO as GPIO

background_img = None
try:
    background_img = pygame.image.load("background.jpg")
    background_img = pygame.transform.scale(background_img, (800, 600))
except Exception as e:
    background_img = None
    print("Background image load failed:", e)

BUFFER_SIZE = 1024
shake_speed_history = []  # 紀錄每題的搖動速率時間與值

Server_fd = None
Game_finished = False
Receive_from_Server_buffer = bytearray(BUFFER_SIZE)
Send_to_Server_buffer = bytearray(BUFFER_SIZE)
Q_counter = -1
Q_finished = [0] * 5
TimeOut_flag = 0
answer_value = -1
Q_finished_lock = Lock()

BUZZER_PIN = 22  # 實體腳位 22
# 音符頻率（Hz）
NOTES = {
    'C5': 523,
    'E5': 659,
    'G5': 784,
    'C6': 1046
}
# 慶祝音效旋律（頻率, 時長）
MELODY = [
    (NOTES['C5'], 0.2),
    (NOTES['E5'], 0.2),
    (NOTES['G5'], 0.2),
    (NOTES['C6'], 0.4),
    (0, 0.1),  # 短暫停頓
    (NOTES['G5'], 0.2),
    (NOTES['C6'], 0.4)
]

SEQUENCE = [
    (NOTES['C5'], 0.2),  # 燈
    (NOTES['C5'], 0.2),  # 燈
    (NOTES['E5'], 0.2),  # 燈
    (NOTES['G5'], 0.4)   # 燈～
]

Question_Set = [
    [
        "What should you do if someone shows signs of a stroke?",
        "A. Wait and see if they get better",
        "B. Give them painkillers and let them rest",
        "C. Call an ambulance immediately",
        "D. Massage them to help relax"
    ],
    [
        "Which activity can help improve hand and arm movement after a stroke?",
        "A. Watching TV",
        "B. Playing simple games with hands",
        "C. Taking long naps",
        "D. Drinking cold water"
    ],
    [
        "If a stroke patient feels tired during rehab, what should they do?",
        "A. Stop all rehab forever",
        "B. Only do rehab once a week",
        "C. Rest a bit, then continue slowly",
        "D. Drink soda for energy"
    ],
    [
        "Which of the following is a healthy daily habit?",
        "A. Smoking after meals",
        "B. Watching TV all day",
        "C. Skipping breakfast",
        "D. Drinking plenty of water"
    ],
    [
        "What should you do if you feel dizzy or lightheaded?",
        "A. Sit or lie down and tell someone",
        "B. Ignore it and keep walking",
        "C. Drive a car quickly",
        "D. Start running"
    ],
    [
        "What is a good way to reduce stress?",
        "A. Yelling loudly",
        "B. Eating junk food",
        "C. Watching scary movies",
        "D. Deep breathing and staying calm"
    ],
    [
        "Which activity is best for keeping the body active?",
        "A. Lying down all afternoon",
        "B. Light walking every day",
        "C. Playing video games",
        "D. Eating more"
    ],
    [
        "Which of the following is a good goal during rehabilitation?",
        "A. To regain independence in daily tasks",
        "B. To lie in bed all day",
        "C. To stop taking medicine",
        "D. To avoid talking to others"
    ],
    [
        "What should you do if you feel dizzy during rehab?",
        "A. Keep walking quickly",
        "B. Sit or lie down and tell someone",
        "C. Drive a car right away",
        "D. Drink soda for energy"
    ],
    [
        "Which drink is best for daily brain health?",
        "A. Beer",
        "B. Soda",
        "C. Water",
        "D. Energy drinks"
    ],
    [
        "Why is regular exercise important after a stroke?",
        "A. It avoids all thinking",
        "B. It improves strength and balance",
        "C. It helps stop all medicine",
        "D. It lets you skip rehab"
    ],
    [
        "How much sleep do older adults need each night?",
        "A. 3 hours",
        "B. 5 hours",
        "C. 7 to 8 hours",
        "D. 12 hours"
    ],
    [
        "Why is taking medicine important after a stroke?",
        "A. To avoid talking to doctors",
        "B. To skip therapy",
        "C. To prevent another stroke",
        "D. To eat more sweets"
    ],
    [
        "What is a goal of physical therapy after stroke?",
        "A. To avoid movement",
        "B. To walk more safely",
        "C. To stop talking",
        "D. To sleep all day"
    ],
    [
        "Which of the following is true about recovery?",
        "A. Doing small things daily helps",
        "B. Never move the weak side",
        "C. Only rest in bed",
        "D. Avoid using your brain"
    ],
    [
        "Which of the following should be avoided after stroke?",
        "A. Healthy breakfast",
        "B. Smoking",
        "C. Taking walks",
        "D. Drinking water"
    ],
    [
        "How often should you take your stroke medicine?",
        "A. Only when you feel bad",
        "B. As the doctor tells you",
        "C. Once a week",
        "D. Whenever you remember"
    ],
    [
        "What is a benefit of regular walking?",
        "A. Better circulation and brain health",
        "B. Tiredness and weakness",
        "C. Poor sleep",
        "D. Slower thinking"
    ],
    [
        "What helps improve coordination?",
        "A. Sitting all day",
        "B. Never using the weak side",
        "C. Watching people exercise",
        "D. Doing slow and simple movements daily"
    ],
    [
        "What is the role of occupational therapy?",
        "A. To help with daily tasks like eating and dressing",
        "B. To watch movies",
        "C. To make people sleep more",
        "D. To avoid using arms"
    ],
    [
        "What should you do if you miss a dose of your medicine?",
        "A. Stop taking it completely",
        "B. Double the next dose",
        "C. Ignore it",
        "D. Call your doctor or pharmacist"
    ]
]


# ---- IMU 設定 ----
MPU6050_ADDR = 0x68
PWR_MGMT_1 = 0x6B
ACCEL_XOUT_H = 0x3B


def play_celebration():
    GPIO.setmode(GPIO.BOARD)
    GPIO.setup(BUZZER_PIN, GPIO.OUT)
    p = GPIO.PWM(BUZZER_PIN, 440)
    p.start(50)

    for freq, duration in MELODY:
        if freq == 0:
            p.ChangeDutyCycle(0)  # 靜音
        else:
            p.ChangeFrequency(freq)
            p.ChangeDutyCycle(50)  # 打開聲音
        time.sleep(duration)

    p.stop()
    GPIO.cleanup()


def play_dun_dun_dun():
    GPIO.setmode(GPIO.BOARD)
    GPIO.setup(BUZZER_PIN, GPIO.OUT)

    p = GPIO.PWM(BUZZER_PIN, 440)
    p.start(50)  # 50% duty cycle

    for freq, duration in SEQUENCE:
        p.ChangeFrequency(freq)
        time.sleep(duration)
        time.sleep(0.05)  # 間隔一下節奏感更棒

    p.stop()
    GPIO.cleanup()



def mpu6050_init(bus):
    bus.write_byte_data(MPU6050_ADDR, PWR_MGMT_1, 0)

def read_word_2c(bus, reg):
    high = bus.read_byte_data(MPU6050_ADDR, reg)
    low = bus.read_byte_data(MPU6050_ADDR, reg + 1)
    val = (high << 8) + low
    if val >= 0x8000:
        val = -((65535 - val) + 1)
    return val

def wrap_text(text, font, max_width):
    """將長字串自動斷行，使其在不超過max_width的情況下顯示"""
    words = text.split(' ')
    lines = []
    current_line = ""
    for word in words:
        test_line = current_line + (word + " ")
        if font.size(test_line)[0] <= max_width:
            current_line = test_line
        else:
            lines.append(current_line)
            current_line = word + " "
    lines.append(current_line)
    return lines



def read_accel_x(bus):
    raw_x = read_word_2c(bus, ACCEL_XOUT_H)
    return raw_x / 16384.0



class ShakeDetector(threading.Thread):
    def __init__(self, threshold=1.2):
        super().__init__()
        self.bus = smbus2.SMBus(1)
        mpu6050_init(self.bus)
        self.shake_count = 0
        self.threshold = threshold
        self.running = True
        self.lock = threading.Lock()
        self.ready = False
        self.shake_times = []  # 新增：記錄每次搖動的時間戳
        self.total_time = 0.0  # 新增：搖動5次的總時間
        self.average_speed = 0.0  # 新增：搖動的平均速率

    def run(self):
        state = 0
        while self.running:
            accel_x = read_accel_x(self.bus)
            if state == 0 and accel_x > self.threshold:
                state = 1
            elif state == 1 and accel_x < -self.threshold:
                with self.lock:
                    self.shake_count += 1
                    self.shake_times.append(time.time())  # 記錄當前時間戳
                    print(f"Shake #{self.shake_count}")
                    if self.shake_count >= 5:
                        # 計算總時間和平均速率
                        if len(self.shake_times) >= 2:
                            self.total_time = self.shake_times[-1] - self.shake_times[0]
                            self.average_speed = 5 / self.total_time  # 搖動5次的平均速率（次/秒）
                            print(f"Total time for 5 shakes: {self.total_time:.2f} seconds")
                            print(f"Average speed: {self.average_speed:.2f} shakes/second")
                        self.ready = True
                        self.running = False
                state = 0
            time.sleep(0.05)

    def stop(self):
        self.running = False
        self.bus.close()



class GPIOKeyReader(threading.Thread):
    def __init__(self, dev_path="/dev/gpio_keys"):
        super().__init__()
        self.dev_path = dev_path
        self.running = True
        self.last_key = None
        self.lock = threading.Lock()
        self.enabled = False

    def run(self):
        try:
            fd = os.open(self.dev_path, os.O_RDONLY)
        except Exception as e:
            print(f"開啟 {self.dev_path} 失敗：{e}")
            return

        while self.running:
            try:
                key = os.read(fd, 1)
                if key and self.enabled:
                    with self.lock:
                        self.last_key = key.decode().lower()
                        print(f"收到按鍵：{self.last_key}")
            except Exception as e:
                print(f"讀取錯誤：{e}")
                break

        os.close(fd)

    def stop(self):
        self.running = False

    def get_key(self):
        if not self.enabled:
            return None
        with self.lock:
            key = self.last_key
            self.last_key = None
            return key









def Show_Q(n):
    if n < 0 or n >= len(Question_Set):
        print(f"Invalid question number: {n}")
        return

    print(f"Question {Q_counter + 2}:")
    for i in range(5):
        print(Question_Set[n][i])


def Get_Keyboard_Value():
    """有輸入回傳輸入值, 沒輸入回傳-1"""
    ready, _, _ = select.select([sys.stdin], [], [], 0)
    if ready:
        buf = sys.stdin.readline()
        try:
            val = int(buf.strip())
            return val
        except:
            return -1
    return -1
def Send_Answer_Request_with_GUI(screen, Server_fd, score):
    global Q_counter, shake_speed_history
    detector = ShakeDetector()
    detector.start()
    msg = "Please shake the IMU 5 times..."
    while True:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit(0)

        with detector.lock:
            imu_count = detector.shake_count
        show_question(
            screen,
            "Ready to Buzz In",
            ["Shake the IMU five times"],
            status=msg,
            q_num=Q_counter,
            score=score,
            imu_count=imu_count
        )
        if Q_counter >= 0 and Q_finished[Q_counter]:
            detector.stop()
            show_question(screen, "This question has ended", [], status="Buzz-in phase ended", q_num=Q_counter, score=score)
            Server_fd.sendall(b"Q_finished 0")
            pygame.time.wait(1000)
            return
        if detector.ready:
            detector.stop()
            # 儲存搖動速率與時間戳
            shake_speed_history.append((time.time(), detector.average_speed))
            # 顯示搖動速率資訊
            show_question(
                screen,
                "Buzz-in Successful",
                [f"Total time: {detector.total_time:.2f}s", f"Average speed: {detector.average_speed:.2f}/s"],
                status="Waiting for server...",
                q_num=Q_counter,
                score=score
            )
            Server_fd.sendall(f"Request_to_Answer {detector.average_speed}".encode())
            pygame.time.wait(1000)
            return
        pygame.time.wait(50)




def safe_text(text):
    # 將傳入字串的 null 字元全部移除，且去除前後空白
    return text.replace('\x00', '').strip()





def TimeOut_handler(signum, frame):
    global TimeOut_flag
    TimeOut_flag = 1

def countdown_display_thread(timeout, stop_event):
    for i in range(timeout, 0, -1):
        if stop_event.is_set():
            break
        print(f"\rTime left: {i:2d} seconds ", end="", flush=True)
        time.sleep(1)
    print("\r                     \r", end="")  # 清空倒數顯示
    

def Answer_Phase_with_GUI(screen, Server_fd, question, options, score, q_num):
    global answer_value, TimeOut_flag
    answer_value = -1
    TimeOut_flag = 0
    timeout = 10
    reader = GPIOKeyReader("/dev/gpio_keys")
    reader.enabled = True
    reader.start()
    start_time = time.time()
    key_map = {'a': 1, 'b': 2, 'c': 3, 'd': 4}
    while True:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit(0)
        key = reader.get_key()
        sec_left = int(timeout - (time.time() - start_time))
        show_question(screen, question, options, status="Press a button to answer", countdown=sec_left, score=score, q_num=q_num)
        if key in key_map:
            answer_value = key_map[key]
            show_question(screen, question, options, status=f"You selected: {key.upper()}", countdown=sec_left, score=score, q_num=q_num)
            pygame.time.wait(1000)
            reader.stop()
            Server_fd.sendall(f"ANSWER {answer_value}".encode())
            return
        if time.time() - start_time >= timeout:
            TimeOut_flag = 1
            show_question(screen, question, options, status="Timeout! Please buzz in again.", countdown=0, score=score, q_num=q_num)
            pygame.time.wait(1000)
            reader.stop()
            Server_fd.sendall(b"ANSWER -1")
            return
        pygame.time.wait(50)


def Listen_Q_finished(Server_fd2):
    global Q_counter, Game_finished
    
    while not Game_finished:
        buffer = bytearray(BUFFER_SIZE)
        try:
            len_recv = Server_fd2.recv_into(buffer)
            if len_recv <= 0:
                continue
        except:
            continue

        try:
            parts = buffer.decode().split()
            if len(parts) >= 3 and parts[0] == "Q_finished":
                index = int(parts[1])
                who = int(parts[2])
                if 0 <= index < 5:
                    if index == Q_counter:
                        with Q_finished_lock:
                            Q_finished[index] = 1
                        print(f"\n[System] Player {who} get the point!")
                    else:
                        print(f"Something wrong! Q_counter = {Q_counter}, Q_finished index = {index}")
            else:
                print(f"[Warning] Unknown message from port2: {buffer.decode()}")
        except:
            print("[Warning] Error processing message from port2")

def init_gui():
    pygame.init()
    screen = pygame.display.set_mode((800, 600))
    pygame.display.set_caption("Buzz-In Quiz Game")
    return screen

def draw_text(screen, text, pos, color=(0,0,0), size=36, center=False):
    text = safe_text(text)
    font = pygame.font.SysFont("Noto Sans CJK TC", size, bold=True)
    surf = font.render(text, True, color)
    rect = surf.get_rect()
    if center:
        rect.center = pos
    else:
        rect.topleft = pos
    screen.blit(surf, rect)

def show_question(screen, question, options, status="", countdown=None, q_num=None, score=None, imu_count=None):
    if background_img:
        screen.blit(background_img, (0, 0))
    else:
        screen.fill((255,255,255))

    info_text = ""
    if q_num is not None:
        info_text += f"Question: {q_num+1}   "
    if score is not None:
        info_text += f"Score: {score}   "
    if imu_count is not None:
        info_text += f"IMU Shake: {imu_count}/5"
    if info_text:
        draw_text(screen, info_text, (20, 20), color=(0,0,180), size=26, center=False)

    # 題目長度自動斷行
    font = pygame.font.SysFont("Noto Sans CJK TC", 36, bold=True)
    wrapped = wrap_text(safe_text(question), font, 700)  # 700像素內自動換行
    for i, line in enumerate(wrapped):
        draw_text(screen, line, (400, 80 + i*38), size=36, center=True)

    # 選項都置中
    line_y = 200
    for opt in options:
        for line in safe_text(opt).splitlines():
            draw_text(screen, line, (400, line_y), color=(50,50,200), size=32, center=True)
            line_y += 44


    if countdown is not None:
        draw_text(screen, f"Countdown: {countdown}", (400, 50), color=(200,0,0), size=36, center=True)
    
    if status:
        lines = safe_text(status).split('\n')
        for idx, line in enumerate(lines):
            draw_text(screen, safe_text(status), (400, 500), color=(255, 0, 0), size=32, center=True)
    pygame.display.flip()

def wrap_text(text, font, max_width):
    words = text.split(' ')
    lines = []
    current_line = ""
    for word in words:
        test_line = current_line + (word + " ")
        if font.size(test_line)[0] <= max_width:
            current_line = test_line
        else:
            lines.append(current_line)
            current_line = word + " "
    lines.append(current_line)
    return lines



def main():
    global Server_fd, Q_counter, Q_finished, Game_finished
    my_score = 0
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <ip> <port1> <port2>")
        sys.exit(1)
    
    screen = init_gui()
    server_ip = sys.argv[1]
    server_port1 = int(sys.argv[2])
    server_port2 = int(sys.argv[3])
  
    # TCP part
    # Socket 1
    try:
        Server_fd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_addr = (server_ip, server_port1)
        Server_fd.connect(server_addr)
    except Exception as e:
        print(f"Socket 1 error: {e}")
        sys.exit(1)

    # Socket 2
    try:
        Server_fd2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_addr2 = (server_ip, server_port2)
        Server_fd2.connect(server_addr2)
    except Exception as e:
        print(f"Socket 2 error: {e}")
        Server_fd.close()
        sys.exit(1)

    port2_thread = threading.Thread(target=Listen_Q_finished, args=(Server_fd2,))
    port2_thread.daemon = True
    port2_thread.start()

    Game_finished = False
    num = 0
    show = bytearray(BUFFER_SIZE)
    command = bytearray(30)
    Q_counter = -1
    Q_finished = [0] * 5

    while not Game_finished:
        Receive_from_Server_buffer[:] = b'\0' * BUFFER_SIZE
        show[:] = b'\0' * BUFFER_SIZE
        command[:] = b'\0' * 30
        
        try:
            recv_len = Server_fd.recv_into(Receive_from_Server_buffer)
            if recv_len <= 0:
                print("error receive message from server")
                sys.exit(1)
        except Exception as e:
            print(f"Receive error: {e}")
            sys.exit(1)

        try:
            parts = Receive_from_Server_buffer.decode().split(maxsplit=2)
            if len(parts) >= 3:
                command_str = parts[0]
                num = int(parts[1])
                show_str = parts[2]
            elif len(parts) == 2:
                command_str = parts[0]
                num = int(parts[1])
                show_str = ""
            else:
                command_str = parts[0] if parts else ""
                num = 0
                show_str = ""
        except:
            command_str = ""
            num = 0
            show_str = ""

        if command_str == "Connect":
            # 可用 show_question 顯示登入成功
            show_question(screen, "Connection Successful", [show_str])
            pygame.time.wait(1000)
            continue
        
        elif command_str == "NewQ":
            question = Question_Set[num][0]
            options = Question_Set[num][1:5]
            show_question(screen, question, options, status="Waiting for buzz in")
            Q_counter += 1
            Send_Answer_Request_with_GUI(screen, Server_fd, my_score)
            continue
        
        elif command_str == "PleaseAnswer":
            question = Question_Set[num][0]
            options = Question_Set[num][1:5]
            Answer_Phase_with_GUI(screen, Server_fd, question, options, my_score, Q_counter)
            continue
        
        elif command_str == "PleaseWait":
            show_question(screen, "Waiting", [show_str])
            pygame.time.wait(1000)
            continue
        
        elif command_str == "AnswerTimeOut":
            # 可用 show_question 告知 timeout
            show_question(screen, "Timeout", [], status="Please buzz in again")
            pygame.time.wait(1000)
            Send_Answer_Request_with_GUI(screen, Server_fd, my_score)
            continue
        
        elif command_str == "AnswerCorrect":
            my_score += 1
            play_dun_dun_dun()
            show_question(screen, "Correct!", [], status="Congratulations!", q_num=Q_counter, score=my_score)
            pygame.time.wait(1500)
            continue
        
        elif command_str == "AnswerWrong":
            show_question(screen, "Incorrect!", [], status="Please buzz in again")
            pygame.time.wait(1500)
            Send_Answer_Request_with_GUI(screen, Server_fd, my_score)
            continue
        
        elif command_str == "Gamefinished":
            # 顯示最終成績
            show_question(screen, "Game Over", show_str.split('\n'), status="Thank you for playing!")
            if "Your final rank is: #1" in show_str:
                play_celebration()
            #  顯示搖動速率趨勢圖
            if shake_speed_history:
                import matplotlib.pyplot as plt
                times, speeds = zip(*shake_speed_history)
                base_time = times[0]
                relative_times = [t - base_time for t in times]

                plt.figure(figsize=(8, 4))
                plt.plot(relative_times, speeds, marker='o', linestyle='-')
                plt.title("Shake Speed Trend")
                plt.xlabel("Time (s)")
                plt.ylabel("Shake Speed (times/sec)")
                plt.grid(True)
                plt.tight_layout()
                plt.show()

            
            waiting = True
            while waiting:
                for event in pygame.event.get():
                    if event.type == pygame.KEYDOWN or event.type == pygame.QUIT:
                        waiting = False            
            Q_finished[Q_counter] = 1
            if Q_counter != 4:
                print("Q_counter should be 4! Something wrong!")
            Game_finished = True
            break

        
        else:
            show_question(screen, "Unknown command", [command_str], status="Please contact the developer")
            pygame.time.wait(1000)
            sys.exit(1)

        
        time.sleep(0.1)

    # 此局遊戲結束
    print("\nGame_finished!")

    Server_fd.close()
    Server_fd2.close()

if __name__ == "__main__":
    main()