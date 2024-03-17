#include <gtest/gtest.h>
#include "fs/FileSystem.hpp"


// 测试格式化文件系统
TEST(FileSystemTest, FormatFileSystem) {
    FileSystem fs;
    fs.format();
    SUCCEED();
}


TEST(FileSystemTest, TestPathParser) {
    auto dirs = FileSystem::parse_path("/usr/local/bin");

    EXPECT_EQ(dirs[0], "usr");
    EXPECT_EQ(dirs[1], "local");
    EXPECT_EQ(dirs[2], "bin");

    SUCCEED();
}

TEST(FileSystemTest, Test_ls) {
    FileSystem fs;
    fs.format();
    auto entries = fs.ls();
    SUCCEED();
}

TEST(FileSystemTest, Test_mkdir) {
    FileSystem fs;
    fs.format();
    fs.mkdir("test");
    auto entries = fs.ls();
    // 检查是否有test目录
    for (auto &entry : entries) {
        if (entry == "test") {
            SUCCEED();
            return;
        }
    }
    FAIL();
}

// 创建同名文件夹，如果报错则说明成功
TEST(FileSystemTest, Test_mkdir_same_name) {
    FileSystem fs;
    fs.format();
    fs.mkdir("test");
    EXPECT_THROW(fs.mkdir("test"), std::runtime_error);
}

// 创建大量目录
TEST(FileSystemTest, Test_mkdir_many) {
    const int NUM = 463;
    FileSystem fs;
    fs.format();
    for (int i = 1; i <= NUM; i++) {
        fs.mkdir("test" + std::to_string(i));
    }
    auto entries = fs.ls();
    // 检查是否有test目录
    for (int i = 1; i <= NUM; i++) {
        bool found = false;
        for (auto &entry : entries) {
            if (entry == "test" + std::to_string(i)) {
                found = true;
                // std::cout << "Found test" + std::to_string(i) << std::endl;
                break;
            }
        }
        if (!found) {
            FAIL();
        }
    }
    SUCCEED();
}

// 创建超过463个目录，期望报错
TEST(FileSystemTest, Test_mkdir_many_error) {
    const int NUM = 463;
    FileSystem fs;
    fs.format();
    for (int i = 1; i <= NUM; i++) {
        fs.mkdir("test" + std::to_string(i));
    }
    EXPECT_THROW(fs.mkdir("test464"), std::runtime_error);
}