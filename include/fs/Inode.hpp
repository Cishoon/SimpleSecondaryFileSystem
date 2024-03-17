#pragma once

#include <cstdint>
#include "DiskInode.hpp"
#include "DirectoryEntry.hpp"

/**
 * 内存Inode，用于缓存磁盘Inode的数据
1. 文件大小
2. 文件的数据块指针，混合索引树
3. 文件的引用计数：文件的引用计数是指文件被打开的次数，每打开一次，引用计数加1，每关闭一次，引用计数减1。当引用计数为0时，表示文件没有被打开，可以删除。
 */
class Inode {
public:
    Inode() = default;

    FileType file_type = FileType::NONE; // 0: 未分配 1: 文件 2: 目录
    uint32_t file_size = 0;
    uint32_t block_pointers[10] {};
    uint32_t reference_count = 0; // 引用计数，为0时可以写回内存
    uint32_t inode_id = 0; // Inode编号
    uint32_t padding[2] {};

    // 判断Inode是否还未被分配
    [[nodiscard]] bool is_available() const {
        return file_type == FileType::NONE;
    }
    // 判断Inode是否空闲
    [[nodiscard]] bool is_free() const {
        return reference_count == 0;
    }

    [[nodiscard]] bool is_file() const {
        return file_type == FileType::FILE;
    }

    [[nodiscard]] bool is_directory() const {
        return file_type == FileType::DIRECTORY;
    }

    void clear() {
        file_type = FileType::NONE;
        file_size = 0;
        reference_count = 0;
        inode_id = 0;
        for (auto &block_pointer : block_pointers) {
            block_pointer = 0;
        }
    }

    static Inode to_inode(const DiskInode& inode, const uint32_t& inode_id) {
        Inode m_inode;
        m_inode.file_type = inode.file_type;
        m_inode.inode_id = inode_id;
        m_inode.reference_count = 0;
        m_inode.file_size = inode.file_size;
        std::memcpy(m_inode.block_pointers, inode.block_pointers, sizeof inode.block_pointers);
        return m_inode;
    }

    static DiskInode to_disk_inode(const Inode& inode) {
        DiskInode disk_inode;
        disk_inode.file_type = inode.file_type;
        disk_inode.file_size = inode.file_size;
        std::memcpy(disk_inode.block_pointers, inode.block_pointers, sizeof inode.block_pointers);
        return disk_inode;
    }

    [[nodiscard]] uint32_t get_directory_num() const {
        if (file_type != FileType::DIRECTORY) {
            throw std::runtime_error("Inode::get_directory_num: Not a directory: " + std::to_string(inode_id));
        }
        return file_size / sizeof(DirectoryEntry);
    }
};