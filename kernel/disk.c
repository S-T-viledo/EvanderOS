// disk.c - Disk I/O operations / 磁盘 I/O 操作
#include "include.h"
#include "stdio.h"

void init_disk_controller();

// Read sectors from disk using LBA addressing / 使用 LBA 寻址从磁盘读取扇区
// Parameters / 参数：
//   drive: 0 for master(kernel disk), 1 for slave(FAT32 disk) / 0=主盘(内核), 1=从盘(FAT32)
//   sector_start: starting sector in LBA mode / 起始扇区号(LBA)
//   buffer: memory address to load data to / 加载目标内存地址
//   sector_count: number of sectors to read / 要读取的扇区数
void read_disk_sectors(unsigned char drive, unsigned int sector_start, void* buffer, unsigned int sector_count)
{
    init_disk_controller();
    unsigned short* dest = (unsigned short*)buffer;
    
    // Select drive: 0xE0 for master, 0xF0 for slave / 根据 drive 参数选择驱动器
    // If drive == 0, result is 0xE0. If drive == 1, result is 0xF0. / 若 drive == 0 结果是 0xE0，若 drive == 1 结果是 0xF0
    unsigned char drive_selector = 0xE0 | (drive << 4);

    for (unsigned int i = 0; i < sector_count; i++) {
        unsigned int current_sector = sector_start + i;
        
        // Wait for disk idle (BSY bit clear) / 等待磁盘空闲(BSY位清零)
        while (io_in8(0x1f7) & 0x80);
        // Send sector count / 发送扇区数
        io_out8(0x1F2, 1);
        
        // Send LBA address bits 0-7 / 发送 LBA 地址第 0-7 位
        io_out8(0x1F3, current_sector & 0xFF);
        // Send LBA address bits 8-15 / 发送 LBA 地址第 8-15 位
        io_out8(0x1F4, (current_sector >> 8) & 0xFF);
        // Send LBA address bits 16-23 / 发送 LBA 地址第 16-23 位
        io_out8(0x1F5, (current_sector >> 16) & 0xFF);
        
        // Send drive selector and LBA bits 24-27 using dynamic drive selector / 发送驱动器选择器和 LBA 第 24-27 位
        io_out8(0x1F6, drive_selector | ((current_sector >> 24) & 0x0F));
        
        // Send read command (0x20) / 发送读命令(0x20)
        io_out8(0x1F7, 0x20);

        // Wait for DRQ bit (data ready) / 等待 DRQ 位(数据准备好)
        while ((io_in8(0x1F7) & 0x88) != 0x08);
        
        // Read 256 words (512 bytes) from data port / 从数据端口读取 256 个字(512 字节)
        for (int j = 0; j < 256; j++) {
            unsigned short data = io_in16(0x1f0);
            *dest = data;
            dest++; // Note: dest is short*, so dest++ advances by 2 bytes / 注意：dest被强转成short*,自动前进2字节
        }
    }
}


// Write sectors to disk / 写入扇区到磁盘
void write_disk_sectors(unsigned char drive, unsigned int lba, unsigned char* buffer, unsigned int count) {
    // 1. Wait for disk idle (check BSY bit) / 等待磁盘空闲(检查 BSY 位)
    while ((io_in8(0x1F7) & 0x80) != 0);

    // 2. Send parameters / 发送参数
    io_out8(0x1F2, count);
    io_out8(0x1F3, (unsigned char)lba);
    io_out8(0x1F4, (unsigned char)(lba >> 8));
    io_out8(0x1F5, (unsigned char)(lba >> 16));
    // Send LBA bits 24-27 and set LBA mode and master/slave drive / 发送 LBA 最高 4 位并设置 LBA 模式和驱动器
    io_out8(0x1F6, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));

    // 3. Send write command (0x30) / 发送写命令(0x30)
    io_out8(0x1F7, 0x30);

    unsigned short* ptr = (unsigned short*)buffer;
    for (unsigned int i = 0; i < count; i++) {
        // 4. Wait for disk ready (check DRQ bit) to receive data / 等待磁盘准备好接收数据(检查 DRQ 位)
        while ((io_in8(0x1F7) & 0x08) == 0);

        // 5. Write 256 words (512 bytes) / 写入 256 个字(512 字节)
        for (int j = 0; j < 256; j++) {
            io_out16(0x1F0, *ptr);
            ptr++;
        }
    }
}

// Initialize disk controller and select master drive / 初始化磁盘控制器并选择主盘
void init_disk_controller() {
    // Select master drive on device register / 在设备寄存器中选择主盘
    io_out8(0x1F6, 0xE0);

    // Wait for disk controller to be ready / 等待磁盘控制器就绪
    for (int i = 0; i < 1000; i++) {
        io_in8(0x1F7);
    }
}
