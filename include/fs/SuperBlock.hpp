#pragma once

#include <cstdint>
#include <ctime>
#include <bitset>

// 磁盘Inode数量，#2~#59，共58个扇区，每个扇区包含8个Inode，共464个Inode
#define INODE_COUNT (464)
// 磁盘Block数量，#60~#2047，共1988个扇区
#define BLOCK_COUNT (1988)

#define INODE_START_INDEX (2)
#define BLOCK_START_INDEX (60)

class SuperBlock {
public:
    // Block的数量
    uint32_t block_count;
    // DiskInode的数量
    uint32_t inode_count;

    // 脏标志
    uint32_t dirty_flag;
    // 最后一次更新时间
    uint32_t last_update_time;

    std::bitset<INODE_COUNT> inode_bitmap; // 分配64字节
    std::bitset<BLOCK_COUNT> block_bitmap; // 分配256字节

    // 8 + 8 + 64 + 256 = 336 字节
    uint32_t padding[172]{}; // 填充到1024字节
public:
    SuperBlock() {
        block_count = BLOCK_COUNT;
        inode_count = INODE_COUNT;
        dirty_flag = 0;
        last_update_time = static_cast<uint32_t>(time(nullptr));
    }

    void format() {
        block_count = BLOCK_COUNT;
        inode_count = INODE_COUNT;
        last_update_time = static_cast<uint32_t>(time(nullptr));
        dirty_flag = 1; // 格式化后需要写回磁盘，所以设置脏标志

        inode_bitmap.reset();
        block_bitmap.reset();

    }

    // 获取空闲Inode
    uint32_t get_free_inode() {
        // 从位图中找到第一个空闲的Inode
        for (uint32_t i = 1; i < inode_count; i++) {
            if (!inode_bitmap.test(i)) {
                inode_bitmap.set(i);
                dirty_flag = 1;
                return i;
            }
        }
        // 如果没有空闲Inode
        throw std::runtime_error("No free inode");
    }

    // 获取空闲Block
    uint32_t get_free_block() {
        // 从位图中找到第一个空闲的Block
        for (uint32_t i = 0; i < block_count; i++) {
            if (!block_bitmap.test(i)) {
                block_bitmap.set(i);
                dirty_flag = 1;
                return i + BLOCK_START_INDEX;
            }
        }
        // 如果没有空闲Block
        throw std::runtime_error("No free block");
    }

    // 获取连续的空闲Block
    uint32_t get_free_blocks(uint32_t block_num) {
        // 从位图中找到连续的block_num个空闲的Block
        for (uint32_t i = 0; i < block_count - block_num; i++) {
            if (block_bitmap.test(i)) {
                continue;
            }
            uint32_t j = 1;
            for (; j < block_num; j++) {
                if (block_bitmap.test(i + j)) {
                    break;
                }
            }
            if (j == block_num) {
                for (uint32_t k = 0; k < block_num; k++) {
                    block_bitmap.set(i + k);
                }
                dirty_flag = 1;
                return i;
            }
        }
        // 如果没有连续的空闲Block
        throw std::runtime_error("No free blocks");
    }
};