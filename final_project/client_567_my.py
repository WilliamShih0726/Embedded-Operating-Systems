import socket
import select
import sys
import threading
import time
import smbus2
import os

BUFFER_SIZE = 1024
Game_finished = False
Q_counter = -1
Q_finished = [0] * 5
TimeOut_flag = 0
answer_value = -1


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


def mpu6050_init(bus):
    bus.write_byte_data(MPU6050_ADDR, PWR_MGMT_1, 0)

def read_word_2c(bus, reg):
    high = bus.read_byte_data(MPU6050_ADDR, reg)
    low = bus.read_byte_data(MPU6050_ADDR, reg + 1)
    val = (high << 8) + low
    if val >= 0x8000:
        val = -((65535 - val) + 1)
    return val

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

    def run(self):
        state = 0
        while self.running:
            accel_x = read_accel_x(self.bus)
            if state == 0 and accel_x > self.threshold:
                state = 1
            elif state == 1 and accel_x < -self.threshold:
                with self.lock:
                    self.shake_count += 1
                    print(f"Shake #{self.shake_count}")
                    if self.shake_count >= 5:
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

def Show_Q(this_round_idx, question_bank_idx):
    # this_round_idx: 這是第幾題（第1~5題，0開始）
    # question_bank_idx: 題庫編號（0~20）
    if question_bank_idx < 0 or question_bank_idx >= len(Question_Set):
        print(f"Invalid question number: {question_bank_idx}")
        return
    print(f"Question {this_round_idx+2}:")  # 顯示第1~5題
    for line in Question_Set[question_bank_idx]:
        print(line)

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


def Send_Answer_Request(Server_fd):
    global Q_counter

    detector = ShakeDetector()
    detector.start()
    print("Please shake imu 5 times...")

    while True:
        # 檢查Q_finished[Q_counter]
        if Q_counter >= 0 and Q_finished[Q_counter]:
            detector.stop()
            print("Question over")
            # 通知Server Q_finished
            msg = f"Q_finished 0".encode()
            Server_fd.sendall(msg)
            return

        # 檢查搖動是否達標
        if detector.ready:
            detector.stop()
            print("Send Request_to_Answer。")
            msg = f"Request_to_Answer 0".encode()
            Server_fd.sendall(msg)
            return

        time.sleep(0.05)


def countdown_display_thread(timeout, stop_event):
    for i in range(timeout, 0, -1):
        if stop_event.is_set():
            break
        print(f"\rTime left: {i:2d} seconds ", end="", flush=True)
        time.sleep(1)
    print("\r                     \r", end="")  # 清空倒數顯示

def Answer_Phase(Server_fd):
    global answer_value, TimeOut_flag
    answer_value = -1
    TimeOut_flag = 0
    timeout = 10
    stop_event = threading.Event()
    t = threading.Thread(target=countdown_display_thread, args=(timeout, stop_event))
    t.start()

    # 啟動 GPIO 按鍵 reader
    reader = GPIOKeyReader("/dev/gpio_keys")
    reader.enabled = True
    reader.start()

    start_time = time.time()
    key_map = {'a': '1', 'b': '2', 'c': '3', 'd': '4'}
    while True:
        key = reader.get_key()
        if key in key_map:
            answer_value = key_map[key]
            stop_event.set()
            break
        if time.time() - start_time >= timeout:
            TimeOut_flag = 1
            stop_event.set()
            break
        time.sleep(0.05)
    t.join()
    reader.stop()

    if answer_value == -1:
        print("Time Out! You need to resend the request")
    else:
        print(f"Your answer is {answer_value}")

    msg = f"ANSWER {answer_value}".encode()
    Server_fd.sendall(msg)


def Listen_Q_finished(Server_fd2):
    global Q_finished, Q_counter
    while True:
        buffer = Server_fd2.recv(BUFFER_SIZE)
        if not buffer:
            print("error receive message from server (port2)")
            sys.exit(1)
        parts = buffer.decode().strip().split()
        if len(parts) >= 2 and parts[0] == "Q_finished":
            try:
                index = int(parts[1])
                if 0 <= index < 5:
                    if index == Q_counter:
                        Q_finished[index] = 1
                        print(f"[System] Marked Q_finished[{index}] = 1 from port2")
                    else:
                        print(f"Something wrong! Q_counter = {Q_counter}, Q_finished index = {index}")
            except:
                print(f"[Warning] Failed to parse Q_finished: {buffer.decode().strip()}")
        else:
            print(f"[Warning] Unknown message from port2: {buffer.decode().strip()}")

def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <ip> <port1> <port2>")
        sys.exit(1)

    server_ip = sys.argv[1]
    server_port1 = int(sys.argv[2])
    server_port2 = int(sys.argv[3])

    Server_fd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    Server_fd.connect((server_ip, server_port1))

    Server_fd2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    Server_fd2.connect((server_ip, server_port2))

    # 啟動 Q_finished 監聽執行緒
    t_port2 = threading.Thread(target=Listen_Q_finished, args=(Server_fd2,), daemon=True)
    t_port2.start()

    global Game_finished, Q_counter, Q_finished
    while True:  # new game
        Game_finished = False
        num = 0
        show = ""
        command = ""
        Q_counter = -1
        Q_finished = [0] * 5

        while not Game_finished:
            buffer = Server_fd.recv(BUFFER_SIZE)
            if not buffer:
                print("error receive message from server")
                sys.exit(1)
            s = buffer.decode()
            #print("[DEBUG] 收到:", repr(s))
            # 多行判斷，每行都處理
            for line in s.strip().splitlines():
                #print("[DEBUG] 單行訊息:", repr(line))
                parts = line.strip().split(maxsplit=2)
                if not parts:
                    continue
                command = parts[0]
                # 只在 NewQ 指令時解析 num，其他不用
                if len(parts) >= 2 and command == "NewQ":
                    try:
                        num = int(parts[1])
                    except:
                        num = 0
                show = parts[2] if len(parts) > 2 else ""
                if command == "Connect":
                    print(show, end="")
                    continue
                elif command == "NewQ":
                    Show_Q(Q_counter, num)
                    if Q_counter < 0:
                        Q_counter += 1
                    elif Q_counter >= 5:
                        print("Something error about NewQ")
                    else:
                        Q_finished[Q_counter] = 1
                        Q_counter += 1
                    Send_Answer_Request(Server_fd)
                elif command == "PleaseAnswer":
                    print(show, end="")
                    Answer_Phase(Server_fd)
                elif command == "PleaseWait":
                    print(show, end="")
                    continue
                elif command == "AnswerTimeOut":
                    Send_Answer_Request(Server_fd)
                elif command == "AnswerCorrect":
                    print(show, end="")
                    print("You correct!")
                    continue
                elif command == "AnswerWrong":
                    print(show, end="")
                    print("You wrong!")
                    Send_Answer_Request(Server_fd)
                elif command == "Gamefinished":
                    # show 可能有多行 (排名與分數)
                    for line2 in show.splitlines():
                        print(line2)
                    Q_finished[Q_counter] = 1
                    if Q_counter != 4:
                        print("Q_counter should be 4! Something wrong!")
                    Game_finished = True
                    break
                elif command == "Ranking:":
                    print(line)  # 或直接 continue
                    continue
                elif command == "Rank":
                    print(line)
                    continue
                else:
                    print("There's something unexpected receive from Server:", repr(line))
                    sys.exit(1)
            time.sleep(0.1)



        # 遊戲結束時，持續接收並印出所有 ranking 資訊
        s = b''
        while True:
            chunk = Server_fd.recv(BUFFER_SIZE)
            if not chunk:
                break
            s += chunk
            if len(chunk) < BUFFER_SIZE:
                break
        for line in s.decode(errors='ignore').splitlines():
            print(line)



if __name__ == "__main__":
    main()