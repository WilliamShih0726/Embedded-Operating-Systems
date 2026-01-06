import tkinter as tk
from tkinter import messagebox

class FullOfflineQuizClientGUI(tk.Tk):
    def __init__(self):
        super().__init__()
        self.bind("<Key>", self.on_key_press)
        self.title("Quiz Client (Full Offline)")
        self.geometry("500x450")
        self.Game_finished = False
        self.Q_counter = -1
        self.imu_threshold = 5
        self.shake_counter = 0
        self.timer_count = 10
        self.answer_var = tk.IntVar(value=-1)
        self.status_label = tk.Label(self, text="等待題目...", font=("Arial", 16))
        self.status_label.pack(pady=10)
        self.question_label = tk.Label(self, text="", font=("Arial", 15), wraplength=480, justify="left")
        self.question_label.pack(pady=8)
        self.options = []
        for i in range(4):
            rb = tk.Radiobutton(self, text="", variable=self.answer_var, value=i+1, font=("Arial", 13), state="disabled")
            rb.pack(anchor="w")
            self.options.append(rb)
        self.imu_btn = tk.Button(self, text="搖動裝置(點擊5次模擬)", command=self.imu_shake)
        self.imu_btn.pack(pady=10)
        self.answer_btn = tk.Button(self, text="提交答案", command=self.submit_answer, state="disabled")
        self.answer_btn.pack(pady=8)
        self.timer_label = tk.Label(self, text="", font=("Arial", 13))
        self.timer_label.pack(pady=5)
        # 模擬題庫(正確答案索引)
        self.question_set = [
            ["What should you do if someone shows signs of a stroke你媽?",
             "1. Wait and see if they get better",
             "2. Give them painkillers and let them rest",
             "3. Call an ambulance immediately",
             "4. Massage them to help relax", 3],
            ["Which activity can help improve hand and arm movement after a stroke?",
             "1. Watching TV",
             "2. Playing simple games with hands",
             "3. Taking long naps",
             "4. Drinking cold water", 2],
            ["If a stroke patient feels tired during rehab, what should they do?",
             "1. Stop all rehab forever",
             "2. Only do rehab once a week",
             "3. Rest a bit, then continue slowly",
             "4. Drink soda for energy", 3],
            ["Which of the following is a healthy daily habit?",
             "1. Smoking after meals",
             "2. Watching TV all day",
             "3. Skipping breakfast",
             "4. Drinking plenty of water", 4],
            ["What should you do if you feel dizzy or lightheaded?",
             "1. Sit or lie down and tell someone",
             "2. Ignore it and keep walking",
             "3. Drive a car quickly",
             "4. Start running", 1],
        ]
        self.total_questions = len(self.question_set)
        self.Q_finished = [0] * self.total_questions
        # 啟動第一題
        self.next_question(first=True)

    def next_question(self, first=False):
        if not first:
            self.Q_counter += 1
        else:
            self.Q_counter = 0

        if self.Q_counter >= self.total_questions:
            self.status_label.config(text="遊戲結束，恭喜！")
            self.imu_btn.config(state="disabled")
            self.answer_btn.config(state="disabled")
            for i in range(4):
                self.options[i].config(state="disabled")
            self.timer_label.config(text="")
            return

        self.shake_counter = 0
        self.Q_finished[self.Q_counter] = 0
        q = self.question_set[self.Q_counter]
        self.question_label.config(text=q[0])
        for i in range(4):
            self.options[i].config(text=q[i+1], state="disabled")
        self.answer_var.set(-1)
        self.imu_btn.config(state="normal")
        self.answer_btn.config(state="disabled")
        self.status_label.config(text=f"請搖動裝置搶答")
        self.timer_label.config(text="")

    def imu_shake(self):
        if self.Q_finished[self.Q_counter]:
            self.imu_btn.config(state="disabled")
            self.status_label.config(text="此題已結束，請等待下一題")
            return
        self.shake_counter += 1
        self.status_label.config(text=f"搖動次數: {self.shake_counter}/{self.imu_threshold}")
        if self.shake_counter >= self.imu_threshold:
            self.imu_btn.config(state="disabled")
            self.start_answer_phase()

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
