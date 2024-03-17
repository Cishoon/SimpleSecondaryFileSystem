#pragma once


#include <cstdint>

// 文件类型的枚举
// 0: 未分配 1: 文件 2: 目录
enum class FileType : uint32_t {
    NONE = 0,
    FILE = 1,
    DIRECTORY = 2,
};

