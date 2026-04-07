# ==============================================================================
# EvanderOS Build Script
# ==============================================================================

# --- 1. compile options ---
CFLAGS = -m32 -c -fno-builtin -ffreestanding -fno-stack-protector -fno-pie -fno-pic -g -std=c99 -Wall -Wextra -mno-sse -mno-mmx -mgeneral-regs-only

# --- 2. directory configuration ---
BOOT_DIR   = boot
KERNEL_DIR = kernel
USER_DIR   = user
APPS_DIR   = apps
BIN_DIR    = bin

IMAGE = EvanderOS.img

# --- 3. target files collection ---

KERNEL_OBJS = $(BIN_DIR)/start.o       \
              $(BIN_DIR)/kernel.o      \
              $(BIN_DIR)/screen.o      \
              $(BIN_DIR)/func.o        \
              $(BIN_DIR)/disk.o        \
              $(BIN_DIR)/int_handler.o \
              $(BIN_DIR)/interrupt.o   \
              $(BIN_DIR)/stdio.o       \
              $(BIN_DIR)/keyboard.o    \
              $(BIN_DIR)/mem.o         \
              $(BIN_DIR)/task.o        \
              $(BIN_DIR)/timer.o       \
              $(BIN_DIR)/gdt.o         \
              $(BIN_DIR)/syscall.o     \
              $(BIN_DIR)/fat32.o

# list of user programs (without .c suffix)
USER_PROGS = shell ls mkdir touch rm rmall echo cp evim easm

USER_BINS  = $(patsubst %, $(BIN_DIR)/%.bin, $(USER_PROGS))

# ==============================================================================
# core build rules
# ==============================================================================

.PHONY: all clean qemu debug dirs

all: dirs $(IMAGE)


dirs:
	@mkdir -p $(BIN_DIR)


$(IMAGE): $(BIN_DIR)/os.bin $(BIN_DIR)/bootloader.bin $(USER_BINS)
	@echo "=> Building final FAT32 image..."
	rm -f $(IMAGE)
	# 1. create a 64MB empty image file
	dd if=/dev/zero of=$(IMAGE) bs=1M count=64 status=none
	# 2. format as FAT32, reserve 8 sectors for Bootloader and Kernel, 256 sectors for FAT32 file system
	mformat -F -R 256 -i $(IMAGE) ::
	# 3. insert bootloader and kernel into the reserved sectors
	dd if=$(BIN_DIR)/bootloader.bin of=$(IMAGE) bs=1 count=3 conv=notrunc status=none
	dd if=$(BIN_DIR)/bootloader.bin of=$(IMAGE) bs=1 skip=90 seek=90 count=422 conv=notrunc status=none
	
	dd if=$(BIN_DIR)/os.bin of=$(IMAGE) bs=512 seek=8 conv=notrunc status=none
	
	# 4. copy user programs into the FAT32 file system
	mmd -i $(IMAGE) ::/BIN || true
	mcopy -o -i $(IMAGE) $(BIN_DIR)/shell.bin ::/SHELL.BIN
	mcopy -o -i $(IMAGE) $(BIN_DIR)/ls.bin ::/BIN/LS.BIN
	mcopy -o -i $(IMAGE) $(BIN_DIR)/mkdir.bin ::/BIN/MKDIR.BIN
	mcopy -o -i $(IMAGE) $(BIN_DIR)/touch.bin ::/BIN/TOUCH.BIN
	mcopy -o -i $(IMAGE) $(BIN_DIR)/rm.bin ::/BIN/RM.BIN
	mcopy -o -i $(IMAGE) $(BIN_DIR)/rmall.bin ::/BIN/RMALL.BIN
	mcopy -o -i $(IMAGE) $(BIN_DIR)/echo.bin ::/BIN/ECHO.BIN
	mcopy -o -i $(IMAGE) $(BIN_DIR)/cp.bin ::/BIN/CP.BIN
	mcopy -o -i $(IMAGE) $(BIN_DIR)/evim.bin ::/BIN/EVIM.BIN
	mcopy -o -i $(IMAGE) $(BIN_DIR)/easm.bin ::/BIN/EASM.BIN
	
	# 5. copy example assembly files for testing
	mcopy -o -i $(IMAGE) $(APPS_DIR)/example.asm ::/EXAMPLE.ASM
	mcopy -o -i $(IMAGE) $(APPS_DIR)/snake.asm ::/S.ASM
	@echo "=> Image build complete!"

# ==============================================================================
# compilation rules for bootloader, kernel, and user programs
# ==============================================================================

# --- Bootloader compilation ---
$(BIN_DIR)/bootloader.bin: $(BOOT_DIR)/bootloader.s
	nasm -f bin $< -o $@

# --- Kernel compilation (using % wildcard to automatically handle all C and S files) ---
$(BIN_DIR)/%.o: $(KERNEL_DIR)/%.c
	gcc $(CFLAGS) $< -o $@

$(BIN_DIR)/%.o: $(KERNEL_DIR)/%.s
	nasm -f elf32 $< -o $@

$(BIN_DIR)/os.bin: $(KERNEL_OBJS)
	ld -m elf_i386 -static $^ -o $(BIN_DIR)/os.elf -T linker.ld -Map=map.txt
	objcopy -O binary $(BIN_DIR)/os.elf $@

# --- Userlib ---
$(BIN_DIR)/userlib.o: $(USER_DIR)/userlib.c
	gcc $(CFLAGS) $< -o $@

$(BIN_DIR)/entry.o: $(USER_DIR)/entry.s
	nasm -f elf32 $< -o $@


$(BIN_DIR)/%.bin: $(USER_DIR)/%.c $(BIN_DIR)/entry.o $(BIN_DIR)/userlib.o
	gcc $(CFLAGS) $< -o $(BIN_DIR)/$*.o
	ld -m elf_i386 -T user_linker.ld -e main --oformat binary $(BIN_DIR)/entry.o $(BIN_DIR)/$*.o $(BIN_DIR)/userlib.o -o $@

# ==============================================================================
# run and debug rules
# ==============================================================================
qemu: $(IMAGE)
	qemu-system-i386 $(IMAGE)

debug: $(IMAGE)
	qemu-system-i386 $(IMAGE) -s -S

clean:
	rm -rf $(BIN_DIR)/*.o $(BIN_DIR)/*.bin $(BIN_DIR)/*.elf $(IMAGE) map.txt