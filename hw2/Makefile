# 可同時支援本機編譯與交叉編譯給 Raspberry Pi

# 目標檔案名稱
HOST_TARGET := hw2
RPI_TARGET  := hw2_rpi

# 原始碼
SRC := hw2.c

# 編譯選項
CFLAGS := -Wall -Wextra -std=c11 -O2

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
