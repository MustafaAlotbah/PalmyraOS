
# 32-Bit output
GPP_PARAMS = -m32 -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-leading-underscore
AS_PARAMS = -g -f elf
LD_PARAMS = -melf_i386

# sources
SRC_CPP_FILES := $(wildcard source/*.cpp)
SRC_ASM_FILES := $(wildcard source/*.asm)

# remove preceding source/ and replace .cpp/.asm with .o
OBJ_CPP_FILES := $(patsubst source/%,%, $(SRC_CPP_FILES:.cpp=.o))
OBJ_ASM_FILES := $(patsubst source/%,%, $(SRC_ASM_FILES:.asm=.o))
objects := $(OBJ_CPP_FILES) $(OBJ_ASM_FILES)

objects:
	echo $(objects)

# compile .cpp files into .o files using the command:
%.o: source/%.cpp
	g++ $(GPP_PARAMS) -Iinclude -o bin/$@ -c $<

# compile .s files into .o files using the command:
%.o: source/%.asm
	nasm $(AS_PARAMS) -o bin/$@ $<

# compile linker.ld DEPENDING on $(objects) using the following command into kernel.bin
kernel.bin: linker.ld $(objects)
	ld $(LD_PARAMS) -T $< -o bin/$@ $(addprefix bin/, $(objects))

# run 'install' task depending on my kernel.bin using the following command
build: kernel.bin

iso: kernel.bin
	mkdir -p bin/iso
	mkdir -p bin/iso/boot
	mkdir -p bin/iso/boot/grub
	cp bin/kernel.bin bin/iso/boot
	cp source/boot/grub.cfg bin/iso/boot/grub/grub.cfg
	grub-mkrescue --output bin/kernel.iso bin/iso
# 	rm -rf bin/iso
	rm bin/*.o