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
OFILES=obj/errno.o obj/fb.o obj/font/font.o obj/kernel.o obj/limine_reqs.o \
	obj/panic.o obj/pmm.o obj/serial.o obj/stdio.o obj/string.o \
	obj/string_x86.o obj/vaddress.o obj/vfprintf.o obj/allocator.o \
	obj/arena_allocator.o

CRTI=obj/start.o

ISOFILES=isodir/boot/os.elf \
	isodir/boot/limine/limine.cfg    isodir/boot/limine/limine.sys \
	isodir/boot/limine/limine-cd.bin isodir/boot/limine/limine-cd-efi.bin

# ===== Build dirs (tmpfs optional) =====
dirs-tmpfs:
	mkdir -p /dev/shm/osdev/ukulele/obj
	ln -s /dev/shm/osdev/ukulele/obj obj
	mkdir -p /dev/shm/osdev/ukulele/bin
	ln -s /dev/shm/osdev/ukulele/bin bin

dirs:
	mkdir -p obj/font
	mkdir -p isodir/boot/limine
	mkdir -p bin

# ===== Recipes =====
obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

obj/%.o: src/%.s
	$(AS) -c -o $@ $<

obj/%.o: src/%.S
	$(AS) -c -o $@ $<

# ISO recipes
isodir/boot/limine/% :
	cp $(LIMINE_DATA)/$(@F) isodir/boot/limine

isodir/boot/limine/limine.cfg: src/limine.cfg
	cp $< $@

isodir/boot/os.elf: bin/os.elf
	cp $< $@

# ===== Output binaries =====
bin/os.elf: src/linker.ld $(CRTI) $(OFILES)
	size $(CRTI) $(OFILES)
	$(LD) $(LDFLAGS) -T src/linker.ld $(CRTI) $(OFILES) -o $@

bin/os.img: $(ISOFILES)
	xorriso -as mkisofs -b boot/limine/limine-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine/limine-cd-efi.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		isodir -o $@
	limine-deploy $@

# ===== Main targets =====
clean:
	find bin/ obj/ isodir/ -type f -delete

build-bin: bin/os.elf
build-iso: bin/os.img

run: build-iso
	qemu-system-x86_64 -serial stdio -cdrom bin/os.img
debug: build-iso
	qemu-system-x86_64 -serial stdio -s -S -cdrom bin/os.img

all: build-bin build-iso
