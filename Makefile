# make        - build chessos.img (and chessos.iso)
# make run    - build and start it in QEMU
# make clean  - delete everything that was built

CXX = g++

# The kernel runs without an operating system below it, so there is no
# standard library and none of the C++ features that need one.
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter \
           -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector \
           -fno-pic -no-pie -mno-red-zone -fno-asynchronous-unwind-tables \
           -fno-threadsafe-statics -nostdlib -Ikernel
LDFLAGS  = -nostdlib -static -T kernel/linker.ld --no-warn-rwx-segments

KERNEL_SOURCES = $(wildcard kernel/*.cpp)
KERNEL_HEADERS = $(wildcard kernel/*.h)
KERNEL_OBJECTS = $(patsubst kernel/%.cpp, build/%.o, $(KERNEL_SOURCES)) build/exceptions.o
BOOT_SOURCES   = boot/stage1.asm boot/stage2.asm boot/layout.inc boot/disk_read.inc
ASSET_FILES    = $(shell find assets midi -type f 2>/dev/null)

all: chessos.img

#Every object file depends on every header: simple, and nothing is ever stale.
build/%.o: kernel/%.cpp $(KERNEL_HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/exceptions.o: kernel/exceptions.asm
	@mkdir -p build
	nasm -f elf64 $< -o $@

# Link the kernel, then strip it down to the plain bytes the loader copies to 1 MB.
build/kernel.bin: $(KERNEL_OBJECTS) kernel/linker.ld
	ld $(LDFLAGS) $(KERNEL_OBJECTS) -o build/kernel.elf
	objcopy -O binary build/kernel.elf build/kernel.bin

build/assets.tar: $(ASSET_FILES) tools/pack_assets.py
	python3 tools/pack_assets.py

#make_image.py assembles the boot loader and puts everything in one disk image.
chessos.img: build/kernel.bin build/assets.tar $(BOOT_SOURCES) tools/make_image.py
	python3 tools/make_image.py

run: chessos.img
	qemu-system-x86_64 -m 256M -drive file=chessos.img,format=raw \
		-audiodev pa,id=sound -device AC97,audiodev=sound -serial stdio

clean:
	rm -rf build chessos.img chessos.iso

.PHONY: all run clean
