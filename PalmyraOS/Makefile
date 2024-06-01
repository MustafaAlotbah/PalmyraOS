
# 32-Bit output
GPP_PARAMS = -m32 -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-leading-underscore
AS_PARAMS = -g -f elf
LD_PARAMS = -melf_i386


SUBDIRS := source/ source/core/ source/boot/ source/libs/

# Source files
SRC_CPP_FILES := $(wildcard $(addsuffix /*.cpp, $(SUBDIRS)))
SRC_ASM_FILES := $(wildcard $(addsuffix /*.asm, $(SUBDIRS)))

# remove preceding source/ and replace .cpp/.asm with .o
OBJ_CPP_FILES := $(patsubst source/%,%, $(SRC_CPP_FILES:.cpp=.o))
OBJ_ASM_FILES := $(patsubst source/%,%, $(SRC_ASM_FILES:.asm=.o))
objects := $(OBJ_CPP_FILES) $(OBJ_ASM_FILES)

objects:
	echo $(objects)

# Compile .cpp files into .o files
bin/%.o: source/%.cpp
	@mkdir -p $(@D)
	g++ $(GPP_PARAMS) -Iinclude -o $@ -c $<

# Compile .asm files into .o files
bin/%.o: source/%.asm
	@mkdir -p $(@D)
	nasm $(AS_PARAMS) -o $@ $<

# compile linker.ld DEPENDING on $(objects) using the following command into kernel.bin
kernel.bin: linker.ld $(addprefix bin/, $(objects))
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