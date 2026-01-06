import socket
import threading
import time

BUFFER_SIZE = 1024

# 題庫（和 client_mix.py 一致）
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
    ]
]

# 答案索引（和 client 保持一致，1-based）
answer_list = ['C', 'B', 'C', 'D', 'A']

clients = []
clients_qfinished_conn = []

def handle_client(conn, addr, qfinished_conn):
    print(f"Client connected from {addr}")
    conn.sendall(b"Connect 0 Welcome to the Quiz Game!\n")
    time.sleep(1)
    for i, q in enumerate(Question_Set):
        conn.sendall(f"NewQ {i} \n".encode())
        time.sleep(0.5)
        q_finished = False
        while not q_finished:
            req = conn.recv(BUFFER_SIZE).decode().strip()
            if req.startswith("Request_to_Answer"):
                conn.sendall(f"PleaseAnswer {i} Please answer the question: (按 A/B/C/D)\n".encode())
                ans_cmd = conn.recv(BUFFER_SIZE).decode().strip()
                if ans_cmd.startswith("ANSWER"):
                    try:
                        user_ans = ans_cmd.split()[1]
                        print(f"[Server] Client answered: {user_ans}, Correct: {answer_list[i]}")
                    except:
                        user_ans = ""
                    if user_ans.upper() == answer_list[i]:
                        print("[Server] Answer correct.")
                        conn.sendall(f"AnsweerCorrect {i} Correct!\n".encode())
                        qfinished_conn.sendall(f"Q_finished {i}".encode())
                        q_finished = True
                    elif user_ans == "-1" or user_ans == "":
                        print("[Server] Answer timeout or blank.")
                        conn.sendall(f"AnswerTimeOut {i} Time out! Please shake and answer again\n".encode())
                    else:
                        print("[Server] Answer wrong.")
                        conn.sendall(f"AnswerWrong {i} Wrong answer. Please shake and try again\n".encode())
            elif req.startswith("Q_finished"):
                qfinished_conn.sendall(f"Q_finished {i}".encode())
                q_finished = True
            else:
                pass
    conn.sendall(f"Gamefinished 0 Game Finished! Thanks for playing.\n".encode())
    time.sleep(0.5)
    conn.sendall(f"0 Rank: 1st\n".encode())
    print(f"Client {addr} finished game.")
    conn.close()
    qfinished_conn.close()


def main():
    HOST = '0.0.0.0'
    PORT1 = 1234  # 主要通訊
    PORT2 = 1235  # Q_finished

    # 主要 port
    s1 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s1.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s1.bind((HOST, PORT1))
    s1.listen(5)
    print(f"Server main port listening on {PORT1}")

    # Q_finished port
    s2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s2.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s2.bind((HOST, PORT2))
    s2.listen(5)
    print(f"Server Q_finished port listening on {PORT2}")

    while True:
        print("Waiting for client connection...")
        conn1, addr1 = s1.accept()
        conn2, addr2 = s2.accept()
        print("Both main and Q_finished port connected.")
        t = threading.Thread(target=handle_client, args=(conn1, addr1, conn2))
        t.start()

if __name__ == "__main__":
    main()
