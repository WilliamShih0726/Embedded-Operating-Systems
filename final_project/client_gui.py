import tkinter as tk
from tkinter import messagebox
import threading
import time
import smbus2
import os

# ---- IMU Shake Detector Thread ----
class ShakeDetector(threading.Thread):
    def __init__(self, threshold=1.2, shakes_needed=5):
        super().__init__()
        self.bus = smbus2.SMBus(1)
        self.MPU6050_ADDR = 0x68
        self.PWR_MGMT_1 = 0x6B
        self.ACCEL_XOUT_H = 0x3B
        self.shake_count = 0
        self.threshold = threshold
        self.running = True
        self.ready = False
        self.lock = threading.Lock()
        self.shakes_needed = shakes_needed
        self.init_sensor()

    def init_sensor(self):
        self.bus.write_byte_data(self.MPU6050_ADDR, self.PWR_MGMT_1, 0)

    def read_word_2c(self, reg):
        high = self.bus.read_byte_data(self.MPU6050_ADDR, reg)
        low = self.bus.read_byte_data(self.MPU6050_ADDR, reg + 1)
        val = (high << 8) + low
        if val >= 0x8000:
            val = -((65535 - val) + 1)
        return val

    def read_accel_x(self):
        raw_x = self.read_word_2c(self.ACCEL_XOUT_H)
        return raw_x / 16384.0

    def run(self):
        state = 0
        while self.running:
            accel_x = self.read_accel_x()
            if state == 0 and accel_x > self.threshold:
                state = 1
            elif state == 1 and accel_x < -self.threshold:
                with self.lock:
                    self.shake_count += 1
                    if self.shake_count >= self.shakes_needed:
                        self.ready = True
                        self.running = False
                state = 0
            time.sleep(0.05)

    def stop(self):
        self.running = False
        self.bus.close()

    def get_count_and_ready(self):
        with self.lock:
            return self.shake_count, self.ready

# ---- GPIO Key Reader Thread ----
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
            except Exception as e:
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

# ---- Tkinter GUI 主體 ----
class FullOfflineQuizClientGUI(tk.Tk):
    def __init__(self):
        super().__init__()
        # ... (原本的屬性初始化、題庫等不變) ...

        # --------- 新增 IMU / GPIO 實體支援 -----------
        self.use_imu = True     # 若不用硬體搖動可關閉
        self.use_gpio = True    # 若不用實體鍵盤可關閉

        if self.use_imu:
            self.imu_detector = ShakeDetector(threshold=1.2, shakes_needed=5)
            self.imu_detector.start()
        else:
            self.imu_detector = None

        if self.use_gpio:
            self.gpio_reader = GPIOKeyReader("/dev/gpio_keys")
            self.gpio_reader.start()
        else:
            self.gpio_reader = None

        # ... (原本的 GUI 建立) ...
        self.after(100, self.poll_hardware)   # 定期查詢硬體事件

    def poll_hardware(self):
        # IMU: 自動搖動到5次時啟用答題
        if self.use_imu and self.imu_detector:
            shakes, ready = self.imu_detector.get_count_and_ready()
            if not self.Q_finished[self.Q_counter]:
                self.status_label.config(text=f"搖動次數: {shakes}/5")
            if ready and self.imu_btn['state'] == "normal":
                self.imu_btn.config(state="disabled")
                self.start_answer_phase()

        # GPIO: 如果已進入答題階段、且有讀到key則自動送出
        if self.use_gpio and self.gpio_reader and self.answer_btn['state'] == "normal":
            key = self.gpio_reader.get_key()
            key_map = {'a': 1, 'b': 2, 'c': 3, 'd': 4}
            if key in key_map:
                self.answer_var.set(key_map[key])
                self.submit_answer()

        # 持續輪詢
        self.after(100, self.poll_hardware)

    def destroy(self):
        # 關閉 thread
        if self.use_imu and self.imu_detector:
            self.imu_detector.stop()
        if self.use_gpio and self.gpio_reader:
            self.gpio_reader.stop()
        super().destroy()




        

    def on_key_press(self, event):
        if self.answer_btn['state'] == "normal":
            if event.char in "1234":
                self.answer_var.set(int(event.char))
                self.submit_answer()

    def start_answer_phase(self):
        if self.Q_finished[self.Q_counter]:
            self.status_label.config(text="此題已結束，請等待下一題")
            return
        for i in range(4):
            self.options[i].config(state="normal")
        self.answer_btn.config(state="normal")
        self.status_label.config(text="請於10秒內選擇並送出答案")
        self.timer_count = 10
        self.timer_running = True
        self.update_timer()

    def update_timer(self):
        if self.answer_btn['state'] == "disabled" or not self.timer_running:
            self.timer_label.config(text="")
            return
        self.timer_label.config(text=f"剩餘時間: {self.timer_count} 秒")
        if self.timer_count > 0:
            self.timer_count -= 1
            self.after(1000, self.update_timer)
        else:
            self.timer_label.config(text="時間到！")
            self.answer_btn.config(state="disabled")
            for i in range(4):
                self.options[i].config(state="disabled")
            self.status_label.config(text="時間到，請重新搶答")
            self.timer_running = False
            self.imu_btn.config(state="normal")
            self.shake_counter = 0

    def submit_answer(self):
        if self.Q_finished[self.Q_counter]:
            self.status_label.config(text="此題已結束，請等待下一題")
            self.answer_btn.config(state="disabled")
            return
        ans = self.answer_var.get()
        if ans == -1:
            messagebox.showwarning("未選擇", "請選擇答案")
            return
        self.answer_btn.config(state="disabled")
        for i in range(4):
            self.options[i].config(state="disabled")
        self.timer_running = False
        correct = self.question_set[self.Q_counter][5]
        if ans == correct:
            self.status_label.config(text="答對了！自動進入下一題")
            self.Q_finished[self.Q_counter] = 1
            self.after(1500, self.next_question)
        else:
            self.status_label.config(text="答錯了，請重新搶答")
            self.imu_btn.config(state="normal")
            self.shake_counter = 0
            self.answer_var.set(-1)

if __name__ == "__main__":
    app = FullOfflineQuizClientGUI()
    app.mainloop()
