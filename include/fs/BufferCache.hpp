#pragma once

#include <cstdint>
#include "disk_manager/DiskManager.hpp"


class BufferCache {
private:
public:
    char data[BLOCK_SIZE]{}; // 数据
    bool dirty = false;  // 是否脏块

    uint32_t block_no = 0;  // 块号
    BufferCache() = default;

    [[nodiscard]] bool is_dirty() const {
        return dirty;
    }

    void clear() {
        block_no = 0;
        dirty = false;
        std::memset(data, 0, BLOCK_SIZE);
    }

};