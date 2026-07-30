KDIR := /lib/modules/$(shell uname -r)/build
SRC := $(PWD)/src
BUILD := $(PWD)/build

all: $(BUILD)/shepherd.c $(BUILD)/Kbuild
	$(MAKE) -C $(KDIR) M=$(BUILD) modules

$(BUILD)/shepherd.c: $(SRC)/shepherd.c | $(BUILD)
	cp $< $@

$(BUILD)/Kbuild: | $(BUILD)
	echo 'obj-m := shepherd.o' > $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

insmod:
	sudo insmod $(BUILD)/shepherd.ko

rmmod:
	sudo rmmod shepherd

reload: rmmod insmod

.PHONY: all clean insmod rmmod reload
