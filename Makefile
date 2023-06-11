.DEFAULT_GOAL=all
.PHONY: all build-iso build-bin run debug clean

# ===== Config =====
# Using clang as it's already a cross-compiler
CC=clang --target=x86_64-elf
AS=$(CC)
LD=$(CC)

CFLAGS_WARNINGS=-Wall -Wextra -Wmissing-prototypes
CFLAGS_ABI=-mno-sse -mcmodel=kernel
CFLAGS=$(CFLAGS_WARNINGS) $(CFLAGS_ABI) -Isrc/include -Os -g3

LDFLAGS=-nostdlib -static -Xlinker -Map=bin/output.map

# Path to limine files (limine.sys, limine-*.bin)
LIMINE_DATA=/usr/share/limine

# ===== Object files =====
OFILES=obj/limine_reqs.o obj/kernel.o obj/serial.o \
	obj/string.o obj/stdio.o obj/vfprintf.o obj/errno.o \
	obj/fb.o obj/font/font.o obj/pmm.o obj/panic.o obj/string_x86.o \
	obj/vaddress.o
CRTI=obj/start.o

# ===== Build dirs (tmpfs optional) =====
dirs:
	mkdir -p obj/font
	mkdir -p isodir
	mkdir -p bin

dirs-tmpfs:
	mkdir -p /dev/shm/osdev/ukulele/obj
	ln -s /dev/shm/osdev/ukulele/obj obj
	mkdir -p /dev/shm/osdev/ukulele/bin
	ln -s /dev/shm/osdev/ukulele/bin bin

# ===== Recipes =====
obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

obj/%.o: src/%.s
	$(AS) -c -o $@ $<

obj/%.o: src/%.S
	$(AS) -c -o $@ $<

# ===== Output binaries =====
bin/os.elf: src/linker.ld $(CRTI) $(OFILES)
	size $(CRTI) $(OFILES)
	$(LD) $(LDFLAGS) -T src/linker.ld $(CRTI) $(OFILES) -o $@

bin/os.img: bin/os.elf src/limine.cfg
	mkdir -p isodir/boot/limine
	cp $(LIMINE_DATA)/limine.sys isodir/boot/limine/
	cp $(LIMINE_DATA)/limine-cd.bin isodir/boot/limine/
	cp $(LIMINE_DATA)/limine-cd-efi.bin isodir/boot/limine/
	cp src/limine.cfg isodir/boot/limine/limine.cfg
	cp bin/os.elf isodir/boot/os.elf
	xorriso -as mkisofs -b boot/limine/limine-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine/limine-cd-efi.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		isodir -o $@
	limine-deploy $@

# ===== Main targets =====
clean:
	rm -r bin/* obj/* isodir/*

build-bin: bin/os.elf
build-iso: bin/os.img

run: build-iso
	qemu-system-x86_64 -serial stdio -cdrom bin/os.img
debug: build-iso
	qemu-system-x86_64 -serial stdio -s -S -cdrom bin/os.img

all: build-bin build-iso
