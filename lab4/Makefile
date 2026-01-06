obj-m := mydev.o
all: x86_build #or RPI_build, you can specify only one build at a time
clean: x86_clean RPI_clean
x86_build: mydev.c
	sudo make -C /lib/modules/6.8.0-52-generic/build M=$(PWD) modules
x86_clean:
	sudo make -C /lib/modules/6.8.0-52-generic/build M=$(PWD) clean
RPI_build: mydev.c
	sudo make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C $(PWD)/../linux M=$(PWD) modules
RPI_clean:
	sudo make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C $(PWD)/../linux M=$(PWD) clean
.PHONY: all clean x86_build x86_clean RPI_build RPI_clean
