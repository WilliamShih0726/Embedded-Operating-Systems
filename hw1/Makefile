KDIR := ../linux                             # Raspberry Pi 的 kernel source 路徑
PWD  := $(shell pwd)
CROSS = aarch64-linux-gnu-

obj-m := standalong_delivery_driver.o

all: kernel user

# 用交叉編譯器編譯 kernel module
kernel:
	$(MAKE) ARCH=arm64 CROSS_COMPILE=$(CROSS) -C $(KDIR) M=$(PWD) modules

# 用交叉編譯器編譯 hw1 和 writer
user:
	$(CROSS)gcc -o hw1 hw1.c
	$(CROSS)gcc -o standalong_delivery_writer standalong_delivery_writer.c

clean:
	$(MAKE) ARCH=arm64 CROSS_COMPILE=$(CROSS) -C $(KDIR) M=$(PWD) clean
	rm -f hw1 standalong_delivery_writer

.PHONY: all kernel user clean
