KERNELDIR := /home/denghengli/linux/IMX6ULL/linux/nxptoalpha_linux

CURRENT_PATH := $(shell pwd)
obj-m := newcharled.o

build: kernel_modules

kernel_modules :
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean