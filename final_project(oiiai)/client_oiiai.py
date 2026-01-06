import threading
import time
import smbus2
import pygame
import sys
import os
import socket

BUFFER_SIZE = 1024
Game_finished = False
Q_counter = -1
Q_finished = [0] * 5
TimeOut_flag = 0
answer_value = -1

Question_Set = [
    [
        "What should you do if someone shows signs of a stroke?",
        "1. Wait and see if they get better",
        "2. Give them painkillers and let them rest",
        "3. Call an ambulance immediately",
        "4. Massage them to help relax"
    ],
    [
        "Which activity can help improve hand and arm movement after a stroke?",
        "1. Watching TV",
        "2. Playing simple games with hands",
        "3. Taking long naps",
        "4. Drinking cold water"
    ],
    [
        "If a stroke patient feels tired during rehab, what should they do?",
        "1. Stop all rehab forever",
        "2. Only do rehab once a week",
        "3. Rest a bit, then continue slowly",
        "4. Drink soda for energy"
    ],
    [
        "Which of the following is a healthy daily habit?",
        "1. Smoking after meals",
        "2. Watching TV all day",
        "3. Skipping breakfast",
        "4. Drinking plenty of water"
    ],
    [
        "What should you do if you feel dizzy or lightheaded?",
        "1. Sit or lie down and tell someone",
        "2. Ignore it and keep walking",
        "3. Drive a car quickly",
        "4. Start running"
    ]
]



# MPU6050 設定
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

# IMU 偵測搖動
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
                    if self.shake_count >= 5:
                        self.ready = True
                        self.running = False
                state = 0
            time.sleep(0.05)

    def stop(self):
        self.running = False
        self.bus.close()

# GPIO 驅動按鍵 reader（從 /dev/gpio_keys）
class GPIOKeyReader(threading.Thread):
    def __init__(self, dev_path="/dev/gpio_keys"):
        super().__init__()
        self.dev_path = dev_path
        self.running = True
        self.last_key = None
        self.lock = threading.Lock()
        self.enabled = False  # 是否允許讀取按鍵

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

def run_game(server_ip, port1, port2):
    pygame.init()
    screen = pygame.display.set_mode((800, 480))
    pygame.display.set_caption("快速搶答遊戲")
    font = pygame.font.Font(None, 36)
    clock = pygame.time.Clock()

    background = None
    if os.path.exists("background.jpg"):
        background = pygame.image.load("background.jpg").convert()
        background = pygame.transform.scale(background, (800, 480))

    # 搶答 client socket
    Server_fd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    Server_fd.connect((server_ip, port1))
    Server_fd2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    Server_fd2.connect((server_ip, port2))
    Server_fd.settimeout(0.5)

    # 啟動 IMU 偵測
    detector = ShakeDetector()
    detector.start()

    # 啟動 GPIO 按鍵讀取
    reader = GPIOKeyReader("/dev/gpio_keys")
    reader.start()

    state = "WAIT_SHAKE"  # 狀態：等待搖動、答題、送答案、等結果等
    answer = None
    question_text = ""
    qidx = 0
    msg_display = ""
    running = True

    try:
        while running:
            # 畫面
            if background:
                screen.blit(background, (0, 0))
            else:
                screen.fill((30, 30, 30))
            shake_display = font.render(f"Shake: {detector.shake_count}/5", True, (0, 255, 255))
            screen.blit(shake_display, (600, 20))
            state_display = font.render(f"狀態: {state}", True, (200, 255, 128))
            screen.blit(state_display, (10, 10))
            msg_disp = font.render(msg_display, True, (255, 255, 255))
            screen.blit(msg_disp, (10, 400))

            if state == "WAIT_SHAKE":
                prompt = font.render("請搖動裝置5次以搶答...", True, (255, 255, 0))
                screen.blit(prompt, (100, 220))
                with detector.lock:
                    if detector.ready:
                        state = "WAIT_SERVER"
                        msg_display = "等待伺服器回應..."
                        Server_fd.sendall(b"Request_to_Answer 0")
                        reader.enabled = False  # 搶答前不允許答題
            elif state == "WAIT_SERVER":
                try:
                    data = Server_fd.recv(1024)
                except socket.timeout:
                    data = b""
                if data:
                    msg = data.decode().strip()
                    if msg.startswith("PleaseAnswer"):
                        state = "ANSWER"
                        msg_display = "請作答 (A/B/C/D)"
                        reader.enabled = True
                        answer = None
                        # 顯示題目 (可依格式自動取得)
                        qidx = int(msg.split()[1])
                        question_text = f"Q{qidx+1}: "  # 你可根據 server 傳送格式加上題目內容
                # 其他狀態，暫時不處理
            elif state == "ANSWER":
                # 顯示題目
                q_surface = font.render(question_text, True, (255, 255, 0))
                screen.blit(q_surface, (50, 100))
                key = reader.get_key()
                if key in "abcd" and answer is None:
                    answer = key
                    Server_fd.sendall(f"ANSWER {['a','b','c','d'].index(key)+1}".encode())
                    msg_display = f"已選擇: {key.upper()}, 等待結果..."
                    reader.enabled = False
                    state = "WAIT_RESULT"
            elif state == "WAIT_RESULT":
                try:
                    data = Server_fd.recv(1024)
                except socket.timeout:
                    data = b""
                if data:
                    msg = data.decode().strip()
                    if msg.startswith("AnsweerCorrect"):
                        msg_display = "答對了！自動進入下一題"
                        state = "WAIT_SHAKE"
                        detector = ShakeDetector()
                        detector.start()
                    elif msg.startswith("AnswerWrong"):
                        msg_display = "答錯，請重新搖動搶答"
                        state = "WAIT_SHAKE"
                        detector = ShakeDetector()
                        detector.start()
                    elif msg.startswith("AnswerTimeOut"):
                        msg_display = "超時，請重新搖動搶答"
                        state = "WAIT_SHAKE"
                        detector = ShakeDetector()
                        detector.start()
                    elif msg.startswith("Gamefinished"):
                        msg_display = "遊戲結束，謝謝參與！"
                        state = "END"
            elif state == "END":
                end_msg = font.render("按下 ESC 鍵離開", True, (255, 50, 50))
                screen.blit(end_msg, (250, 250))

            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        running = False

            pygame.display.flip()
            clock.tick(30)

    except KeyboardInterrupt:
        print("\ngameover,清理資源...")
    finally:
        detector.stop()
        reader.stop()
        pygame.quit()
        sys.exit()

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print(f"Usage: python3 {sys.argv[0]} <server_ip> <port1> <port2>")
        sys.exit(1)
    run_game(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]))
