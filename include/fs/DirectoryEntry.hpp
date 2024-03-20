#pragma once

#include <cstring>
#include <cstdint>

// DirectoryEntry是目录项，用于存储目录中的文件名和Inode编号
class DirectoryEntry {
public:
    uint32_t inode_id = 0; // Inode编号
    char name[28] {}; // 文件名

    DirectoryEntry() = default;
    DirectoryEntry(const uint32_t &inode_id, const char *name) : inode_id(inode_id) {
        strncpy(this->name, name, 28);
    }

};