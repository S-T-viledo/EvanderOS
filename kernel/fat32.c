/*
 * === EvanderOS - FAT32 File System (fat32.c) ===
 * EvanderOS - FAT32 文件系统 (fat32.c)
 *
 * Purpose/目的:
 *   - Implement FAT32 file system driver
 *     实现 FAT32 文件系统驱动程序
 *   - Provide file and directory operations (ls, mkdir, rm, touch, etc.)
 *     提供文件和目录操作 (ls, mkdir, rm, touch 等)
 *   - Support file reading and writing
 *     支持文件读取和写入
 *   - Manage cluster allocation in FAT (File Allocation Table)
 *     管理 FAT (文件分配表) 中的簇分配
 *
 * FAT32 Structure/FAT32 结构:
 *   - BPB (BIOS Parameter Block): Boot sector containing FS metadata
 *     BPB (BIOS参数块): 包含文件系统元数据的引导扇区
 *   - FAT (File Allocation Table): Chain of cluster numbers
 *     FAT (文件分配表): 簇号的链表
 *   - Root Directory: Starting at root_cluster
 *     根目录: 从 root_cluster 开始
 *   - Data Area: File/directory content clusters
 *     数据区: 文件/目录内容簇
 *
 * Key Variables/关键变量:
 *   - fat_start_sector: Starting sector of FAT table
 *   - data_start_sector: Starting sector of data area
 *   - sectors_per_cluster: Number of sectors per cluster
 *   - root_cluster: Cluster number of root directory (typically 2)
 */
#include "include.h"
#include "stdio.h"

unsigned int fat_start_sector;    // FAT 表的第一扇区 LBA - Starting sector of FAT table
unsigned int data_start_sector;   // 数据区的第一扇区 LBA - Starting sector of data area
unsigned int sectors_per_cluster; // 每簇扇区数 - Sectors per cluster
unsigned int root_cluster;        // 根目录起始簇号 - Root directory cluster number
unsigned int sectors_per_fat;     // 每个 FAT 表占用的扇区数 (用于遍历寻找空闲簇)
                                  // Sectors per FAT table (for traversing free clusters)

// 簇号转换为物理扇区号 
unsigned int cluster_to_sector(unsigned int cluster) {
    // FAT32 的簇号是从 2 开始的，簇 0 和 1 被保留了。
    // 所以 cluster 2 对应数据区的第 0 个簇。
    return data_start_sector + (cluster - 2) * sectors_per_cluster;
}

// partition_lba: FAT32 分区的起始扇区。
// 如果你是直接把虚拟盘 mkfs.fat -F 32 格式化的，这里就是 0。
// 如果你是用 fdisk 分区的，第一个分区通常是 2048。
void fat32_init(unsigned int partition_lba) {
    unsigned char boot_sector[512];
    
    // 读取分区第一个扇区 (BPB)
    read_disk_sectors(0,partition_lba, boot_sector, 1);
    
    struct fat32_bpb* bpb = (struct fat32_bpb*)boot_sector;

    // 简单校验一下是不是 FAT32
    if (bpb->boot_signature != 0x29) {
        printk("Not a valid FAT32 file system!\n");
        return;
    }

    // 1. 计算 FAT 表的起始物理扇区 = 分区起点 + 保留扇区数
    fat_start_sector = partition_lba + bpb->reserved_sector_count;

    // 2. 计算数据区的起始物理扇区 = FAT表起点 + (FAT表数量 * 每个FAT表的扇区数)
    data_start_sector = fat_start_sector + (bpb->table_count * bpb->table_size_32);

    // 3. 记录每簇的扇区数和根目录起始簇
    sectors_per_cluster = bpb->sectors_per_cluster;
    root_cluster = bpb->root_cluster;

    // 4：记录 FAT 表本身的大小
    sectors_per_fat = bpb->table_size_32;

    // 打印出来确认计算正确 (建议你在屏幕上看看这些值是否合理)
    printk("\nFAT32 Init Success:\n");
    printk("FAT Start Sector: %d\n", fat_start_sector);
    printk("Data Start Sector: %d\n", data_start_sector);
    printk("Sectors Per Cluster: %d\n", sectors_per_cluster);
    printk("Root Cluster: %d\n", root_cluster);
    printk("Sectors Per FAT: %d\n", sectors_per_fat);
}

// 辅助函数：将 FAT32 的 8.3 文件名转换为正常字符串进行对比
// 比如 "KERNEL  BIN" -> "KERNEL.BIN"
void format_name(unsigned char* fat_name, char* dest) {
    int i, j = 0;
    for (i = 0; i < 8 && fat_name[i] != ' '; i++) dest[j++] = fat_name[i];
    if (fat_name[8] != ' ') {
        dest[j++] = '.';
        for (i = 8; i < 11 && fat_name[i] != ' '; i++) dest[j++] = fat_name[i];
    }
    dest[j] = '\0';
}

// 在根目录中寻找文件，返回其起始簇号
unsigned int fat32_find_file(const char* target_name) {
    unsigned char buffer[512];
    // 1. 读取根目录所在的第一个扇区
    read_disk_sectors(0, cluster_to_sector(root_cluster), buffer, 1);

    struct fat32_dir_entry* dir = (struct fat32_dir_entry*)buffer;

    // 2. 遍历扇区内的 16 个目录项 (512 / 32 = 16)
    for (int i = 0; i < 16; i++) {
        if (dir[i].name[0] == 0x00) break;   // 目录结束标志
        if (dir[i].name[0] == 0xE5) continue; // 已删除文件

        char name[13];
        format_name(dir[i].name, name);
        
        // 打印发现的文件 (调试用)
        // printk("Found: %s, Size: %d bytes\n", name, dir[i].file_size);

        // 如果名字匹配 (简单实现，不处理长文件名)
        // 注意：FAT32 默认是大写的
        if (memcmp(name, target_name, strlen(target_name)) == 0) {
            // 合并高位和低位簇号
            return (dir[i].cluster_high << 16) | dir[i].cluster_low;
        }
    }
    return 0; // 没找到
}

// 在 FAT 表中查询某簇的下一簇号
unsigned int get_next_cluster(unsigned int cluster) {
    unsigned char fat_buffer[512];
    // FAT32 中每个条目占 4 字节
    // 一个扇区可以存 512 / 4 = 128 个条目
    unsigned int fat_sector = fat_start_sector + (cluster / 128);
    unsigned int fat_offset = cluster % 128;

    read_disk_sectors(0, fat_sector, fat_buffer, 1);
    unsigned int* fat = (unsigned int*)fat_buffer;
    
    // 返回该条目的值 (掩码忽略高 4 位，那是保留位)
    return fat[fat_offset] & 0x0FFFFFFF;
}

// 将整个文件加载到内存
void fat32_read_file(unsigned int start_cluster, void* buffer) {
    unsigned int current_cluster = start_cluster;
    unsigned char* dest = (unsigned char*)buffer;

    while (current_cluster < 0x0FFFFFF8) { // 0x...F8 到 0x...FF 代表结束
        // 读取当前簇
        read_disk_sectors(0, cluster_to_sector(current_cluster), dest, sectors_per_cluster);
        
        // 移动内存指针
        dest += sectors_per_cluster * 512;
        
        // 查找下一簇
        current_cluster = get_next_cluster(current_cluster);
    }
}


// 辅助函数：从路径中提取下一段名字。例如 "/bin/ls" -> 提取出 "bin"，剩下 "/ls"
// 返回值：提取出的名字长度，如果到头了返回 0
int get_next_path_part(char **path, char *part) {
    while (**path == '/') (*path)++; // 跳过多余的 '/'
    if (**path == '\0') return 0;

    int i = 0;
    while (**path != '/' && **path != '\0' && i < 11) {
        part[i++] = **path;
        (*path)++;
    }
    part[i] = '\0';
    return i;
}


// 传入路径，传出该文件/文件夹的完整目录项
// 返回值：1 表示找到，0 表示未找到
int fat32_stat(char* filepath, struct fat32_dir_entry* out_entry) {
    unsigned int current_cluster = root_cluster;
    char* path_ptr = filepath;
    char current_part[12];
    unsigned char buffer[512]; 

    // 如果路径是根目录 "/"
    if (filepath[0] == '/' && filepath[1] == '\0') {
        // 伪造一个根目录的 entry
        memset(out_entry, 0, sizeof(struct fat32_dir_entry));
        out_entry->attr = FAT_ATTR_DIRECTORY;
        out_entry->cluster_low = root_cluster & 0xFFFF;
        out_entry->cluster_high = (root_cluster >> 16) & 0xFFFF;
        return 1;
    }

    while (get_next_path_part(&path_ptr, current_part) > 0) {
        // 读取当前目录簇
        read_disk_sectors(0, cluster_to_sector(current_cluster), buffer, 1);
        struct fat32_dir_entry* dir = (struct fat32_dir_entry*)buffer;
        
        int found = 0;
        for (int i = 0; i < 16; i++) {
            if (dir[i].name[0] == 0x00) break;   
            if (dir[i].name[0] == 0xE5 || dir[i].attr == FAT_ATTR_LONG_NAME) continue; 

            char formatted_name[13];
            format_name(dir[i].name, formatted_name); 

            if (memcmp(formatted_name, current_part, strlen(current_part)) == 0) {
                found = 1;
                current_cluster = (dir[i].cluster_high << 16) | dir[i].cluster_low;
                
                // 如果路径还没解析完，说明当前这个必须是文件夹
                if (*path_ptr != '\0' && !(dir[i].attr & FAT_ATTR_DIRECTORY)) {
                    return 0; // 路径错误：中间的某个节点不是文件夹
                }
                
                // 如果路径解析完了，说明找到了最终目标！
                if (*path_ptr == '\0') {
                    // 【核心】：把找到的完整 32 字节目录项拷贝给外面的指针！
                    if (out_entry) {
                        memcpy(out_entry, &dir[i], sizeof(struct fat32_dir_entry));
                    }
                    return 1;
                }
                break; 
            }
        }
        if (!found) return 0; 
    }
    return 0;
}


// 寻找并分配一个空闲簇，返回分配的簇号。0 表示磁盘已满。
unsigned int fat32_allocate_cluster() {
    unsigned int buffer[128]; // 一个扇区 512 字节，可以装 128 个 32位 的 FAT 表项
    
    // 假设 FAT 表占用 sectors_per_fat 个扇区
    for (unsigned int sec = 0; sec < sectors_per_fat; sec++) {
        // 读取 FAT 表的一个扇区
        read_disk_sectors(0, fat_start_sector+ sec, buffer, 1);        
        for (int i = 0; i < 128; i++) {
            // 0x00000000 表示该簇空闲
            if ((buffer[i] & 0x0FFFFFFF) == 0) {
                // 1. 计算出这是第几个簇
                unsigned int free_cluster = sec * 128 + i;
                
                // FAT32 的前两个簇 (0 和 1) 是保留的
                if (free_cluster < 2) continue;
                
                // 2. 将其标记为 EOF (已被占用且是文件的结尾)
                buffer[i] = 0x0FFFFFFF;
                
                // 3. 将修改后的 FAT 表写回磁盘！
                write_disk_sectors(0, fat_start_sector + sec, (unsigned char*)buffer, 1);
                
                // 4. 将新分配的这个簇的物理数据区清零！防止读到以前的垃圾数据
                unsigned char zero_buf[512];
                memset(zero_buf, 0, 512);
                unsigned int data_sector = cluster_to_sector(free_cluster);
                // 假设 sectors_per_cluster 为 1，如果是多扇区需要循环清空
                write_disk_sectors(0, data_sector, (unsigned char*)zero_buf, 1); 
                
                return free_cluster;
            }
        }
    }
    return 0; // 磁盘满了！
}

// 在 parent_cluster 对应的目录下，寻找一个空槽位，写入 new_entry
int fat32_add_dir_entry(unsigned int parent_cluster, struct fat32_dir_entry* new_entry) {
    unsigned char buffer[512];
    
    // 读取父目录的扇区
    read_disk_sectors(0, cluster_to_sector(parent_cluster), buffer, 1);
    struct fat32_dir_entry* dir = (struct fat32_dir_entry*)buffer;

    for (int i = 0; i < 16; i++) {
        // 寻找空位：0x00(从未用过) 或 0xE5(已被删除的废弃项)
        if (dir[i].name[0] == 0x00 || dir[i].name[0] == 0xE5) {
            
            // 将新目录项拷贝到这个空位
            memcpy(&dir[i], new_entry, sizeof(struct fat32_dir_entry));
            
            // 将修改后的目录扇区刷回磁盘！
            write_disk_sectors(0, cluster_to_sector(parent_cluster), (unsigned char*)buffer, 1);
            return 1; // 成功
        }
    }
    // 如果走到这里，说明这一个簇的 16 个位置都满了。
    // 在真正的 OS 中，这时候需要调用 fat32_allocate_cluster 给父目录扩容！
    // 目前为了简单，我们可以先返回失败。
    printk("Directory is full!\n");
    return 0; 
}


// 将普通字符串 "TEST" 转换为 FAT32 目录项的 11 字节格式 "TEST       "
void make_fat_name(char* src, unsigned char* dest) {
    // 先全部填充为空格 (0x20)
    for (int i = 0; i < 11; i++) dest[i] = ' ';
    
    // 【核心修复】：特判 "." 和 ".."
    if (src[0] == '.' && src[1] == '\0') {
        dest[0] = '.';
        return;
    }
    if (src[0] == '.' && src[1] == '.' && src[2] == '\0') {
        dest[0] = '.'; 
        dest[1] = '.';
        return;
    }

    int i = 0, j = 0;
    // 处理主文件名 (最多 8 字节)
    while (src[i] != '\0' && src[i] != '.' && j < 8) {
        // 如果是小写字母，转为大写
        if (src[i] >= 'a' && src[i] <= 'z') {
            dest[j] = src[i] - 32;
        } else {
            dest[j] = src[i];
        }
        i++; j++;
    }
    
    // 如果有扩展名，处理扩展名
    if (src[i] == '.') {
        i++;
        j = 8; // 扩展名固定从第 8 个字节开始
        while (src[i] != '\0' && j < 11) {
            if (src[i] >= 'a' && src[i] <= 'z') {
                dest[j] = src[i] - 32;
            } else {
                dest[j] = src[i];
            }
            i++; j++;
        }
    }
}



void fat32_ls(char* path) {
    unsigned int target_cluster = root_cluster; // 默认是根目录

    // 1. 如果用户传了具体的路径 (并且不是根目录 "/")
    if (path != 0 && path[0] != '\0' && !(path[0] == '/' && path[1] == '\0')) {
        struct fat32_dir_entry entry;
        
        // 调用 stat 查找该路径的目录项
        if (fat32_stat(path, &entry) == 0) {
            printk(" ls: cannot access '%s': No such file or directory\n", path);
            return; // 找不到，直接返回
        }
        
        // 检查它的属性是不是文件夹 (0x10)
        if (!(entry.attr & 0x10)) {
            printk(" ls: '%s' is a file, not a directory\n", path);
            return; // 如果是个普通文件，不打印它的内容
        }
        
        // 既然是文件夹，取出它的起始簇号！
        target_cluster = (entry.cluster_high << 16) | entry.cluster_low;
    }

    // 2. 读取目标簇的扇区，并遍历打印 (与你原来的逻辑一样)
    unsigned char buffer[512];
    read_disk_sectors(0, cluster_to_sector(target_cluster), buffer, 1);
    struct fat32_dir_entry* dir = (struct fat32_dir_entry*)buffer;

    printk("\n NAME       EXT    SIZE(Byte)  ATTR\n");
    printk(" ------------------------------\n");

    for (int i = 0; i < 16; i++) {
        if (dir[i].name[0] == 0x00) break;   
        if (dir[i].name[0] == 0xE5 || dir[i].attr == 0x0F) continue; 

        char name[9] = {0};
        char ext[4] = {0};
        for(int j=0; j<8; j++) name[j] = dir[i].name[j];
        for(int j=0; j<3; j++) ext[j] = dir[i].name[8+j];

        char* attr_str = (dir[i].attr & 0x10) ? "<DIR>" : "FILE ";
        printk(" %s   %s    %d         %s\n", name, ext, dir[i].file_size, attr_str);
    }
}


// path: 例如 "/BIN/MYDIR"
int fat32_mkdir(char* path) {
    // 1. 切割路径：找到父目录和新文件夹的名字
    char parent_path[256];
    memset(parent_path, 0, 256);
    
    char new_dir_name[16];
    memset(new_dir_name, 0, 16);
    
    int len = strlen(path);
    int last_slash = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (path[i] == '/') { last_slash = i; break; }
    }
    
    if (last_slash == -1) return -1; // 路径不合法

    if (last_slash == 0) {
        strcpy(parent_path, "/"); // 父目录是根目录
    } else {
        memcpy(parent_path, path, last_slash);
    }
    strcpy(new_dir_name, path + last_slash + 1);

    // 2. 获取父目录的簇号
    unsigned int parent_cluster;
    if (strcmp(parent_path, "/") == 0) {
        parent_cluster = root_cluster;
    } else {
        struct fat32_dir_entry parent_entry;
        if (!fat32_stat(parent_path, &parent_entry)) return -2; // 父目录不存在
        if (!(parent_entry.attr & 0x10)) return -3; // 父节点不是文件夹
        parent_cluster = (parent_entry.cluster_high << 16) | parent_entry.cluster_low;
    }

    // 3. 向 FAT 表申请一个新的空闲簇
    unsigned int new_cluster = fat32_allocate_cluster();
    if (new_cluster == 0) return -4; // 磁盘已满

    // 4. 构造新文件夹在父目录中的 "户口本"
    struct fat32_dir_entry new_entry;
    memset(&new_entry, 0, sizeof(struct fat32_dir_entry));
    make_fat_name(new_dir_name, new_entry.name);
    new_entry.attr = 0x10; // 0x10 代表这是一个目录 (Directory)
    new_entry.cluster_high = (new_cluster >> 16) & 0xFFFF;
    new_entry.cluster_low = new_cluster & 0xFFFF;
    new_entry.file_size = 0; // 文件夹的大小通常为 0

    // 5. 将新户口本登记到父目录中
    if (!fat32_add_dir_entry(parent_cluster, &new_entry)) {
        return -5; // 父目录已满 (目前暂未实现父目录跨簇自动扩容)
    }

    // 6. 在新房子里，放置 . 和 .. 两个隐藏传送门
    unsigned char buffer[512];
    memset(buffer, 0, 512); // 一定要清零！
    struct fat32_dir_entry* new_dir_sectors = (struct fat32_dir_entry*)buffer;

    // 构建 "." (指向自己)
    make_fat_name(".", new_dir_sectors[0].name);
    new_dir_sectors[0].attr = 0x10;
    new_dir_sectors[0].cluster_high = (new_cluster >> 16) & 0xFFFF;
    new_dir_sectors[0].cluster_low = new_cluster & 0xFFFF;

    // 构建 ".." (指向父目录)
    make_fat_name("..", new_dir_sectors[1].name);
    new_dir_sectors[1].attr = 0x10;
    // 如果父目录是根目录，FAT32 规定 .. 的簇号必须写 0
    unsigned int dotdot_cluster = (parent_cluster == root_cluster) ? 0 : parent_cluster;
    new_dir_sectors[1].cluster_high = (dotdot_cluster >> 16) & 0xFFFF;
    new_dir_sectors[1].cluster_low = dotdot_cluster & 0xFFFF;

    // 将这两个传送门刷入新簇对应的物理扇区！
    write_disk_sectors(0, cluster_to_sector(new_cluster), (unsigned char*)buffer, 1);

    return 0; 
}


// 创建一个空文件
int fat32_touch(char* path) {
    char parent_path[256];
    memset(parent_path, 0, 256);
    char new_file_name[16];
    memset(new_file_name, 0, 16);
    
    // 1. 切割路径
    int len = strlen(path);
    int last_slash = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (path[i] == '/') { last_slash = i; break; }
    }
    if (last_slash == -1) return -1; 

    if (last_slash == 0) {
        strcpy(parent_path, "/");
    } else {
        memcpy(parent_path, path, last_slash);
    }
    strcpy(new_file_name, path + last_slash + 1);

    struct fat32_dir_entry check_entry;
    if (fat32_stat(path, &check_entry) == 1) {
        // 如果该文件或文件夹已经存在，直接返回成功
        return 0; 
    }
    // 2. 获取父目录簇号
    unsigned int parent_cluster;
    if (strcmp(parent_path, "/") == 0) {
        parent_cluster = root_cluster;
    } else {
        struct fat32_dir_entry parent_entry;
        if (!fat32_stat(parent_path, &parent_entry)) return -2; 
        if (!(parent_entry.attr & 0x10)) return -3; 
        parent_cluster = (parent_entry.cluster_high << 16) | parent_entry.cluster_low;
    }

    // 3. 构造文件的 "户口本"
    struct fat32_dir_entry new_entry;
    memset(&new_entry, 0, sizeof(struct fat32_dir_entry));
    make_fat_name(new_file_name, new_entry.name);
    
    // 普通文件的属性是 0x20，且初始不分配簇 (簇号为0)，大小为0
    new_entry.attr = 0x20; 
    new_entry.cluster_high = 0;
    new_entry.cluster_low = 0;
    new_entry.file_size = 0; 

    // 4. 登记到父目录
    if (!fat32_add_dir_entry(parent_cluster, &new_entry)) {
        return -5; 
    }

    return 0; 
}



// 把 FAT 表里的占用标记全部清零
void fat32_free_chain(unsigned int start_cluster) {
    // 簇号 0 和 1 是保留的，如果是空文件(簇号0)，直接返回
    if (start_cluster < 2) return; 

    unsigned int curr = start_cluster;
    unsigned int buffer[128]; // 用于读取 FAT 扇区

    while (curr >= 2 && curr < 0x0FFFFFF8) {
        unsigned int fat_sec = fat_start_sector + (curr / 128);
        unsigned int fat_idx = curr % 128;

        // 读取 FAT 扇区
        read_disk_sectors(0, fat_sec, (unsigned char*)buffer, 1);
        
        // 记录下一个簇号
        unsigned int next = buffer[fat_idx] & 0x0FFFFFFF;
        
        // 将当前簇在 FAT 表中标记为 0 (空闲)
        buffer[fat_idx] = 0;
        
        // 写回 FAT 扇区
        write_disk_sectors(0, fat_sec, (unsigned char*)buffer, 1);

        curr = next; 
    }
}


// 删除文件或空文件夹
int fat32_rm(char* path) {
    char parent_path[256];
    memset(parent_path, 0, 256);
    char target_name[16];
    memset(target_name, 0, 16);
    
    // 1. 切割路径
    int len = strlen(path);
    int last_slash = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (path[i] == '/') { last_slash = i; break; }
    }
    if (last_slash == -1 || (last_slash == 0 && path[1] == '\0')) return -1; // 不能删除根目录 "/"

    if (last_slash == 0) {
        strcpy(parent_path, "/");
    } else {
        memcpy(parent_path, path, last_slash);
    }
    strcpy(target_name, path + last_slash + 1);

    // 2. 获取父目录簇号
    unsigned int parent_cluster;
    if (strcmp(parent_path, "/") == 0) {
        parent_cluster = root_cluster;
    } else {
        struct fat32_dir_entry parent_entry;
        if (!fat32_stat(parent_path, &parent_entry)) return -2; 
        parent_cluster = (parent_entry.cluster_high << 16) | parent_entry.cluster_low;
    }

    // 3. 格式化目标名字 (用于比对)
    unsigned char formatted_target[11];
    make_fat_name(target_name, formatted_target);

    // 4. 读取父目录，寻找并击杀目标！
    unsigned char buffer[512];
    read_disk_sectors(0, cluster_to_sector(parent_cluster), buffer, 1);
    struct fat32_dir_entry* dir = (struct fat32_dir_entry*)buffer;

    for (int i = 0; i < 16; i++) {
        if (dir[i].name[0] == 0x00) break; 
        if (dir[i].name[0] == 0xE5 || dir[i].attr == 0x0F) continue;

        if (memcmp(dir[i].name, formatted_target, 11) == 0) {
            
            // 如果是文件夹，拒绝删除！
            if (dir[i].attr & 0x10) {
                return -6; // 错误码 -6 代表 "Is a directory"
            }
            // 普通文件
            unsigned int target_cluster = (dir[i].cluster_high << 16) | dir[i].cluster_low;
            
            // A. 释放它占用的物理簇 (如果有的话)
            if (target_cluster >= 2) {
                fat32_free_chain(target_cluster);
            }

            // B. 盖上墓碑：将名字的第一个字节改为 0xE5
            dir[i].name[0] = 0xE5;

            // C. 刷回磁盘
            write_disk_sectors(0, cluster_to_sector(parent_cluster), buffer, 1);
            
            return 0; // 删除成功
        }
    }
    return -4; // 没找到该文件
}


// 递归摧毁某个目录簇链下的所有文件和子目录
void fat32_rmall_cluster(unsigned int start_cluster) {
    if (start_cluster < 2) return;
    unsigned int curr = start_cluster;
    
    // 【核心防御】：使用 kmalloc 在堆上申请 512 字节，避免 4KB 内核栈溢出！
    unsigned char* buffer = (unsigned char*)kmalloc(512);
    if (!buffer) return; 

    while (curr >= 2 && curr < 0x0FFFFFF8) {
        // 遍历这个簇里的每一个扇区
        for (unsigned int sec = 0; sec < sectors_per_cluster; sec++) {
            read_disk_sectors(0, cluster_to_sector(curr) + sec, buffer, 1);
            struct fat32_dir_entry* dir = (struct fat32_dir_entry*)buffer;

            for (int i = 0; i < 16; i++) {
                if (dir[i].name[0] == 0x00) goto end_dir; // 0x00 代表该目录彻底空了，直接跳出
                if (dir[i].name[0] == 0xE5 || dir[i].attr == 0x0F) continue; // 跳过已删除和长文件名

                // 【关键】：绝对不能删除 "." 和 ".."，否则会陷入死循环向上删把整个磁盘删空！
                if (memcmp(dir[i].name, ".          ", 11) == 0 ||
                    memcmp(dir[i].name, "..         ", 11) == 0) {
                    continue;
                }

                unsigned int child_cluster = (dir[i].cluster_high << 16) | dir[i].cluster_low;
                // 【紧急制动阀】：如果子簇号指向自己，或者指向0，绝对不许递归！
                if (child_cluster == start_cluster || child_cluster == 0) {
                    continue;
                }
                
                if (dir[i].attr & 0x10) {
                    // 这是一个子文件夹，递归杀进去！
                    if (child_cluster >= 2) fat32_rmall_cluster(child_cluster);
                } else {
                    // 这是一个普通文件，释放它的簇
                    if (child_cluster >= 2) fat32_free_chain(child_cluster);
                }
            }
        }
        curr = get_next_cluster(curr); // 读取目录的下一个簇
    }

end_dir:
    kfree(buffer); // 用完释放堆内存
    fat32_free_chain(start_cluster); // 最后，把这个目录本身的簇也释放掉
}


// 递归删除文件或文件夹
int fat32_rmall(char* path) {
    char parent_path[256];
    memset(parent_path, 0, 256);
    char target_name[16];
    memset(target_name, 0, 16);
    
    int len = strlen(path);
    int last_slash = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (path[i] == '/') { last_slash = i; break; }
    }
    if (last_slash == -1 || (last_slash == 0 && path[1] == '\0')) return -1; // 严禁删除根目录！

    if (last_slash == 0) strcpy(parent_path, "/");
    else memcpy(parent_path, path, last_slash);
    
    strcpy(target_name, path + last_slash + 1);

    unsigned int parent_cluster;
    if (strcmp(parent_path, "/") == 0) parent_cluster = root_cluster;
    else {
        struct fat32_dir_entry parent_entry;
        if (!fat32_stat(parent_path, &parent_entry)) return -2; 
        parent_cluster = (parent_entry.cluster_high << 16) | parent_entry.cluster_low;
    }

    unsigned char formatted_target[11];
    make_fat_name(target_name, formatted_target);

    unsigned char buffer[512];
    read_disk_sectors(0, cluster_to_sector(parent_cluster), buffer, 1);
    struct fat32_dir_entry* dir = (struct fat32_dir_entry*)buffer;

    for (int i = 0; i < 16; i++) {
        if (dir[i].name[0] == 0x00) break; 
        if (dir[i].name[0] == 0xE5 || dir[i].attr == 0x0F) continue;

        if (memcmp(dir[i].name, formatted_target, 11) == 0) {
            unsigned int target_cluster = (dir[i].cluster_high << 16) | dir[i].cluster_low;
            
            // 根据类型选择删除方式
            if (dir[i].attr & 0x10) {
                // 如果是文件夹，调用递归
                if (target_cluster >= 2) fat32_rmall_cluster(target_cluster);
            } else {
                // 如果是文件，普通释放
                if (target_cluster >= 2) fat32_free_chain(target_cluster);
            }

            // 并刷回磁盘
            dir[i].name[0] = 0xE5;
            write_disk_sectors(0, cluster_to_sector(parent_cluster), buffer, 1);
            return 0; 
        }
    }
    return -4; 
}


// 修改 FAT 表中的簇指针
void fat32_set_fat_entry(unsigned int cluster, unsigned int value) {
    unsigned int fat_sec = fat_start_sector + (cluster / 128);
    unsigned int fat_idx = cluster % 128;
    unsigned int buffer[128];
    
    read_disk_sectors(0, fat_sec, (unsigned char*)buffer, 1);
    
    // FAT32 的簇项最高 4 位是保留位，绝不能修改！
    buffer[fat_idx] = (buffer[fat_idx] & 0xF0000000) | (value & 0x0FFFFFFF);
    
    write_disk_sectors(0, fat_sec, (unsigned char*)buffer, 1);
}

// 覆盖写入文件：如果不存在会返回错误（需要先用 touch 创建）
int fat32_write_file(char* path, char* buffer, unsigned int size) {
    // 1. 路径解析 (跟 rm 类似，严禁 SSE 优化)
    char parent_path[256]; memset(parent_path, 0, 256);
    char target_name[16]; memset(target_name, 0, 16);
    int len = strlen(path), last_slash = -1;
    for (int i = len - 1; i >= 0; i--) if (path[i] == '/') { last_slash = i; break; }
    if (last_slash == -1) return -1;
    if (last_slash == 0) strcpy(parent_path, "/");
    else memcpy(parent_path, path, last_slash);
    strcpy(target_name, path + last_slash + 1);

    // 2. 获取父目录和目标文件信息
    unsigned int parent_cluster;
    if (strcmp(parent_path, "/") == 0) parent_cluster = root_cluster;
    else {
        struct fat32_dir_entry parent_entry;
        if (!fat32_stat(parent_path, &parent_entry)) return -2;
        parent_cluster = (parent_entry.cluster_high << 16) | parent_entry.cluster_low;
    }
    unsigned char formatted_target[11];
    make_fat_name(target_name, formatted_target);

    unsigned char dir_buf[512];
    read_disk_sectors(0, cluster_to_sector(parent_cluster), dir_buf, 1);
    struct fat32_dir_entry* dir = (struct fat32_dir_entry*)dir_buf;

    int target_idx = -1;
    for (int i = 0; i < 16; i++) {
        if (dir[i].name[0] == 0x00) break;
        if (dir[i].name[0] == 0xE5 || dir[i].attr == 0x0F) continue;
        if (memcmp(dir[i].name, formatted_target, 11) == 0) {
            if (dir[i].attr & 0x10) return -6; // 严禁覆盖写入文件夹！
            target_idx = i;
            break;
        }
    }
    if (target_idx == -1) return -4; // 文件未找到

    // 3. 释放旧的簇链（清空内容）
    unsigned int old_cluster = (dir[target_idx].cluster_high << 16) | dir[target_idx].cluster_low;
    if (old_cluster >= 2) fat32_free_chain(old_cluster);

    // 4. 分配新簇链并写入数据
    unsigned int first_cluster = 0;
    if (size > 0) {
        unsigned int bytes_per_cluster = sectors_per_cluster * 512;
        unsigned int clusters_needed = (size + bytes_per_cluster - 1) / bytes_per_cluster;
        unsigned int prev_cluster = 0;
        unsigned int bytes_written = 0;
        unsigned char* src_ptr = (unsigned char*)buffer;

        for (unsigned int k = 0; k < clusters_needed; k++) {
            unsigned int new_c = fat32_allocate_cluster();
            if (new_c == 0) return -7; // 磁盘已满！

            // 链接上一个簇到当前簇
            if (k == 0) first_cluster = new_c;
            else fat32_set_fat_entry(prev_cluster, new_c);
            prev_cluster = new_c;

            // 写入实际数据
            unsigned int chunk = size - bytes_written;
            if (chunk >= bytes_per_cluster) {
                write_disk_sectors(0, cluster_to_sector(new_c), src_ptr, sectors_per_cluster);
                src_ptr += bytes_per_cluster;
                bytes_written += bytes_per_cluster;
            } else {
                // 【核心防御】：最后一块数据可能不满一个簇，用 kmalloc 申请临时缓冲垫底，防内核栈溢出！
                unsigned char* bounce = (unsigned char*)kmalloc(bytes_per_cluster);
                memset(bounce, 0, bytes_per_cluster);
                memcpy(bounce, src_ptr, chunk);
                write_disk_sectors(0, cluster_to_sector(new_c), bounce, sectors_per_cluster);
                kfree(bounce);
                bytes_written += chunk;
            }
        }
    }
    // 5. 更新文件的 "户口本"
    dir[target_idx].cluster_high = (first_cluster >> 16) & 0xFFFF;
    dir[target_idx].cluster_low = first_cluster & 0xFFFF;
    dir[target_idx].file_size = size;
    write_disk_sectors(0, cluster_to_sector(parent_cluster), dir_buf, 1);

    return 0; // 写入成功！
}