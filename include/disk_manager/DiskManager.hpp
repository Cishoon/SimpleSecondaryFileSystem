#pragma once

#include <fstream>

#define BLOCK_SIZE (512) // 磁盘块大小

class DiskManager {
public:

    explicit DiskManager(const std::string &file_path, const uint32_t& file_size);

    ~DiskManager();

    // 格式化磁盘文件（全部清空）
    void format();

    // 以盘块为单位读取磁盘
    std::vector<char> read_block(const uint32_t& block_id, const uint32_t& block_num);

    // 以盘块为单位写入磁盘
    void write_block(const uint32_t& block_id, const std::vector<char>& data);

private:
    // 读取磁盘文件的特定部分
    std::vector<char> _read(const std::streamoff& position, const std::streamsize& length);

    // 写入数据到磁盘文件的特定位置
    void _write(const std::streamoff& position, const std::vector<char>& data);


private:
    std::string _file_path; // 磁盘文件路径
    std::fstream _disk_file; // 文件流，用于读写操作
    uint32_t _file_size; // 文件大小
};
