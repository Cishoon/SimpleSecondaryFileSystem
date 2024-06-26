#pragma once

#include <cstdint>
#include <ctime>
#include <bitset>

#define INODE_COUNT (3968) // 几个inode块，用于bitset
#define BLOCK_COUNT (2097152) // 几个数据块，用于bitset

#define INODE_SIZE (INODE_COUNT / 8) // INODE扇区数量，8个inode块一个扇区

#define SUPER_BLOCK_SIZE ((16 + INODE_COUNT / 8 + BLOCK_COUNT / 8) / 512) // SUPER_BLOCK扇区数量

#define INODE_START_INDEX (SUPER_BLOCK_SIZE) // INODE起始扇区
#define BLOCK_START_INDEX (SUPER_BLOCK_SIZE + INODE_SIZE)   // 数据块起始扇区


class SuperBlock {
public:
    // Block的数量
    uint32_t block_count;
    // DiskInode的数量
    uint32_t inode_count;

    // 脏标志
    uint32_t dirty_flag;

    // 记录block_bitmap上次分配到哪个位置，加速get_free_block
    uint32_t last_i;

    std::bitset<INODE_COUNT> inode_bitmap;

    std::bitset<BLOCK_COUNT> block_bitmap;


public:
    SuperBlock() {
        block_count = BLOCK_COUNT;
        inode_count = INODE_COUNT;
        dirty_flag = 0;
        last_i = 0;
    }

    void format() {
        block_count = BLOCK_COUNT;
        inode_count = INODE_COUNT;
        dirty_flag = 1; // 格式化后需要写回磁盘，所以设置脏标志

        inode_bitmap.reset();
        block_bitmap.reset();
        last_i = 0;
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
        // for (uint32_t i = last_i; i < block_count; i++) {
        //     if (!block_bitmap.test(i)) {
        //         block_bitmap.set(i);
        //         dirty_flag = 1;
        //         last_i = (i + 1) % block_count;
        //         return i + BLOCK_START_INDEX;
        //     }
        //
        //     if (i + 1 == last_i) {
        //         break;
        //     }
        // }
        uint32_t i = last_i;
        do {
            if (i == 0) ++i;
            if (!block_bitmap.test(i)) {
                block_bitmap.set(i);
                dirty_flag = 1;
                last_i = (i + 1) % block_count;
                return i + BLOCK_START_INDEX;
            }
            i = (i + 1) % block_count;
        } while (i != last_i);

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
                return i + BLOCK_START_INDEX;
            }
        }
        // 如果没有连续的空闲Block
        throw std::runtime_error("No free blocks");
    }

    [[nodiscard]] inline bool check_block_bit(const uint32_t &p) const {
        return p != 0 && block_bitmap.test(p - BLOCK_START_INDEX);
    }
};