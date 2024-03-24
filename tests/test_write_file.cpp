#include <gtest/gtest.h>
#include "fs/FileSystem.hpp"
#include "common/common.hpp"

TEST(FileSystemTest, Test_write_big_file) {
    auto callback = [](size_t current, size_t total) {
        const int barWidth = 50; // 进度条的宽度
        float progress = static_cast<float>(current) / (float) total; // 计算当前进度（0.0 - 1.0）
        int pos = static_cast<int>(barWidth * progress); // 计算进度条内已完成部分的长度

        std::cout << "\r["; // 回车符并开始进度条的输出
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::cout << "="; // 已完成部分
            else if (i == pos) std::cout << ">"; // 当前位置
            else std::cout << " "; // 未完成部分
        }
        std::cout << "] " << int(progress * 100.0) << "% " // 显示百分比
                  << COMMON::formatBytes(current) << " / " << COMMON::formatBytes(total); // 显示已完成和总大小
        std::cout << std::endl; // 立即刷新输出，确保显示更新
    };

    FileSystem fs;
    fs.format();
    fs.touch("a");

    std::string a(((1LL << 29) + (1LL << 28)), 'a');
    auto fd = fs.fopen("a");
    fs.fwrite(fd, a.data(), a.size(), callback);
    fs.fclose(fd);

    fs.rm("a");

    fs.touch("a");
    fd = fs.fopen("a");
    fs.fwrite(fd, a.data(), a.size(), callback);
    fs.fclose(fd);

    SUCCEED();
}