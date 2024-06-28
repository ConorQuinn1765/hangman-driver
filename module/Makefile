obj-m += hangman.o

.PHONY: build clean load unload test

build:
	make -C /lib/modules/$(shell uname -r)/build modules M=$(PWD)

clean:
	make -C /lib/modules/$(shell uname -r)/build clean M=$(PWD)

load:
	sudo insmod hangman.ko
unload:
	-sudo rmmod hangman
