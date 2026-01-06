# 目標檔案名稱
HOST_TARGET := hw3
RPI_TARGET  := hw3_rpi

# 原始碼
SRC := hw3.c

# 編譯選項
CFLAGS := -Wall -Wextra -std=c11 -O2 -pthread

.PHONY: all clean

# 預設目標：同時編譯本機與 Raspberry Pi 版本
all: $(HOST_TARGET) $(RPI_TARGET)

# 本機編譯
$(HOST_TARGET): $(SRC)
	gcc $(CFLAGS) -o $@ $^

# RPi 交叉編譯
$(RPI_TARGET): $(SRC)
	@echo "Cross-compiling for Raspberry Pi..."
	aarch64-linux-gnu-gcc $(CFLAGS) -o $@ $^

# 清除所有產物
clean:
	rm -f $(HOST_TARGET) $(RPI_TARGET)
