#pragma once

#include <cstdint>
#include "FileType.hpp"

class DiskInode {
public:
    FileType file_type = FileType::NONE; // 0: 未分配 1: 文件 2: 目录
    uint32_t file_size = 0;
    uint32_t block_pointers[10] {}; // 存的值是盘块号
    uint32_t padding[4] {};

    DiskInode() = default;


};
// 4 + 4 + 40 + 16 = 64