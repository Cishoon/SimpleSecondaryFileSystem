#pragma once

#include <cstdint>
#include "disk_manager/DiskManager.hpp"


class BufferCache {
private:
    char data[BLOCK_SIZE]{}; // 数据
    bool dirty = false;  // 是否脏块
public:

    uint32_t block_no = 0;  // 块号
    BufferCache() = default;

    [[nodiscard]] bool is_dirty() const {
        return dirty;
    }

    void set_dirty(const bool& d) {
        this->dirty = d;
    }

    void clear() {
        block_no = 0;
        dirty = false;
        std::memset(data, 0, BLOCK_SIZE);
    }

    void clear_data() {
        std::memset(data, 0, BLOCK_SIZE);
    }

    /**
     * 读取数据
     * @tparam T
     * @param index
     * @return
     *  使用范例
     *  const uint32_t *block_pointers = cache_block->read<uint32_t>(0); // 读取第一个uint32_t指针
     */
    template<typename T>
    const T* read(const uint32_t& index) const {
        // 检查是否越界
        if (index * sizeof(T) + sizeof(T) > BLOCK_SIZE) {
            throw std::out_of_range("BufferCache::read out of range");
        }
        return reinterpret_cast<const T *>(data + index * sizeof(T));
    }

    /**
     * 写入数据
     * @tparam T
     * @param index
     * @param value
     *  使用范例
     *  cache_block->write<uint32_t>(0, block_pointers); // 写入第一个uint32_t指针
     */
    template<typename T>
    [[nodiscard]] bool write(const T *value, const uint32_t& index, uint32_t size = 0, bool index_by_char = false) {
        if (size == 0) size = sizeof(T);
        uint32_t offset = index_by_char ? index : index * size;

        // 检查是否越界
        if (offset + size > BLOCK_SIZE) {
            throw std::out_of_range("BufferCache::write out of range");
        }
        const auto& p = reinterpret_cast<const char*>(value);
        std::copy(p, p + size, data + offset);

        dirty = true;

        // 如果写到末尾了，返回true
        return offset + size == BLOCK_SIZE;
    }

};