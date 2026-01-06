import threading
import time
import smbus2
import pygame
import sys
import os

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
                    print(f"Shake #{self.shake_count}")
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

# 顯示題目與作答
def run_game():
    pygame.init()
    screen = pygame.display.set_mode((800, 480))
    pygame.display.set_caption("快速搶答遊戲")
    font = pygame.font.Font(None, 36)
    clock = pygame.time.Clock()

    background = None
    if os.path.exists("background.jpg"):
        background = pygame.image.load("background.jpg").convert()
        background = pygame.transform.scale(background, (800, 480))

    # 啟動 IMU 偵測
    detector = ShakeDetector()
    detector.start()

    # 啟動 GPIO 按鍵讀取
    reader = GPIOKeyReader("/dev/gpio_keys")
    reader.start()

    waiting_text = font.render("shake 5 times", True, (255, 255, 255))
    question_text = font.render("Q1: (A) aaa (B) bbb (C) ccc (D) ddd", True, (255, 255, 0))
    answer_text = None
    answered = False

    try:
        while True:
            if background:
                screen.blit(background, (0, 0))
            else:
                screen.fill((0, 0, 0))

            with detector.lock:
                count = detector.shake_count
                if detector.ready:
                    reader.enabled = True  # 啟用按鍵讀取

            shake_display = font.render(f"Shake:{count}", True, (0, 255, 255))
            screen.blit(shake_display, (600, 20))

            if not detector.ready:
                screen.blit(waiting_text, (50, 220))
            else:
                screen.blit(question_text, (30, 100))
                if answer_text:
                    screen.blit(answer_text, (50, 200))

                key = reader.get_key()
                if key and not answered:
                    key_map = {
                        'a': 'A',
                        'b': 'B',
                        'c': 'C',
                        'd': 'D'
                    }
                    if key in key_map:
                        answer = key_map[key]
                        answer_text = font.render(f"your answer{answer}", True, (0, 255, 0))
                        answered = True

            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    raise KeyboardInterrupt

            pygame.display.flip()
            clock.tick(30)

    except KeyboardInterrupt:
        print("\ngameover,清理資源...")
    finally:
        detector.stop()
        reader.stop()
        pygame.quit()
        sys.exit()

# 主程式
if __name__ == "__main__":
    run_game()
