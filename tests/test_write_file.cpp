#include <gtest/gtest.h>
#include "fs/FileSystem.hpp"
#include "common/common.hpp"

// 上传1MB文件
TEST(FileSystemTest, Test_write_1MB) {
    const int FILE_SIZE = 512;
    const int len = 2048 * 32;

    FileSystem fs;
    fs.format();
    fs.touch("test");
    auto fd = fs.fopen("test");
    std::string long_text(FILE_SIZE * len, 'a');
    // for (int i = 0; i < len; i++) {
    //     fs.fwrite(fd, long_text.c_str(), FILE_SIZE);
    //     if (i % 2048 == 0) {
    //         std::cout << "Writing " << i / 2048 << "MB..." << std::endl;
    //     }
    // }
    FileSystem::ProgressCallback callback = [](size_t current, size_t total) {
        // 显示写入的进度，包括百分比和字节数
        // 字节数可以自动转换成KB、MB、GB等单位
        std::cout << "\r"
                  << "Writing " << COMMON::formatBytes(current) << " / " << COMMON::formatBytes(total) << " bytes ("
                  << (current * 100 / total) << "%)";
        std::cout.flush();
    };

    fs.fwrite(fd, long_text.c_str(), FILE_SIZE * len, callback);
    fs.fclose(fd);

    // 读取文本
    fd = fs.fopen("test");
    std::vector<char> buffer(FILE_SIZE * len + 1);
    fs.fread(fd, buffer.data(), FILE_SIZE * len);
    fs.fclose(fd);

    // std::string long_text2(buffer.begin(), buffer.end());
    // EXPECT_STREQ(long_text2.c_str(), long_text.c_str());

    // 把buffer中的内容写到文件里
    std::ofstream out("test.txt", std::ios::binary);
    out.write(buffer.data(), FILE_SIZE * len);
    out.close();

    // 统计test.txt里有几个a
    std::ifstream in("test.txt", std::ios::binary);
    in.seekg(0, std::ios::end);
    int size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<char> buffer2(size);
    in.read(buffer2.data(), size);
    in.close();
    int count = 0;
    for (int i = 0; i < size; i++) {
        if (buffer2[i] == 'a') {
            count++;
        }
    }
    EXPECT_EQ(count, FILE_SIZE * len);

    SUCCEED();
}