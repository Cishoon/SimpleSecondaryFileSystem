//
// Created by Cishoon on 2024/3/12.
//

#include "disk_manager/DiskManager.hpp"


DiskManager::DiskManager(const std::string &file_path, const uint32_t& file_size) : _file_path(file_path), _file_size(file_size) {
    if (!_disk_file.is_open()) {
        // 检查文件是否存在
        std::ifstream existing_file(file_path, std::ios::binary);
        if (!existing_file) {
            // 文件不存在，创建并设置为1G大小
            std::ofstream new_file(file_path, std::ios::binary | std::ios::out);
            if (!new_file) {
                throw std::runtime_error("Failed to create disk file.");
            }
            // new_file.seekp((1LL << 30) - 1); // 移动到1G-1位置
            new_file.seekp((file_size) - 1); // 移动到1G-1位置
            new_file.write("\0", 1); // 写入一个字节以扩展文件大小到1G
            new_file.close();
        }

        // 打开文件以供读写
        _disk_file.open(file_path, std::ios::in | std::ios::out | std::ios::binary);
        if (!_disk_file) {
            throw std::runtime_error("Failed to open disk file.");
        }
    }
}

void DiskManager::format() {
    // 关闭当前打开的文件
    _disk_file.close();

    // 以输出模式重新打开文件，这将清空文件内容
    _disk_file.open(_file_path, std::ios::out | std::ios::trunc | std::ios::binary);
    _disk_file.seekp(_file_size - 1);
    _disk_file.write("\0", 1);
    // 关闭并以读写模式重新打开文件
    _disk_file.close();
    _disk_file.open(_file_path, std::ios::in | std::ios::out | std::ios::binary);

    // 确保文件流处于良好状态
    if (!_disk_file.good()) {
        throw std::runtime_error("Failed to format the disk file.");
    }
}

DiskManager::~DiskManager() {
    if (_disk_file.is_open()) {
        _disk_file.close();
    }
}

std::vector<char> DiskManager::_read(const std::streamoff& position, const std::streamsize& length) {
    std::vector<char> buffer(length);
    _disk_file.seekg(position);
    _disk_file.read(buffer.data(), length);
    return buffer;
}

void DiskManager::_write(const std::streamoff& position, const std::vector<char>& data) {
    _disk_file.seekp(position);
    _disk_file.write(data.data(), static_cast<std::streamsize>(data.size()));
    _disk_file.flush();
}

std::vector<char> DiskManager::read_block(const uint32_t& block_id, const uint32_t& block_num) {
    return _read(block_id * BLOCK_SIZE, block_num * BLOCK_SIZE);
}

void DiskManager::write_block(const uint32_t& block_id, const std::vector<char>& data) {
    // data 必须是 BLOCK_SIZE 的整数倍
    if (data.size() % BLOCK_SIZE != 0) {
        throw std::runtime_error("Data size must be multiple of BLOCK_SIZE. Data size: " + std::to_string(data.size()));
    }
    _write(block_id * BLOCK_SIZE, data);
}