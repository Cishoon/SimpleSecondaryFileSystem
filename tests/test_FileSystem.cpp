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

    dirs = FileSystem::parse_path("usr/local/bin");
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
    const int NUM = 500;
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

// 创建超过INODE_COUNT - 2个目录，期望报错，耗时太久，删了
// TEST(FileSystemTest, Test_mkdir_many_error) {
//     const int NUM = INODE_COUNT - 2;
//     FileSystem fs;
//     fs.format();
//     for (int i = 1; i <= NUM; i++) {
//         fs.mkdir("test" + std::to_string(i));
//     }
//     EXPECT_THROW(fs.mkdir("test_overflow"), std::runtime_error);
// }

// 多次打开关闭文件系统
TEST(FileSystemTest, Test_open_close) {
    const int NUM = 300;
    {
        FileSystem fs;
        fs.format();
        for (int i = 1; i <= NUM; i++) {
            fs.mkdir("test" + std::to_string(i));
        }
    }
    {
        FileSystem fs;
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
    }
    SUCCEED();
}

// 测试cd
TEST(FileSystemTest, Test_cd) {
    FileSystem fs;
    fs.format();
    fs.mkdir("test");
    fs.cd("test");
    EXPECT_EQ(fs.pwd(), "/test");
    SUCCEED();
}

TEST(FileSystemTest, ComplexDirectoryNavigation) {
    FileSystem fs;
    fs.format();

    // 创建多个目录
    fs.mkdir("dir1");
    fs.mkdir("dir2");
    fs.cd("dir1");
    fs.mkdir("subdir1");

    // 测试当前工作目录
    EXPECT_EQ(fs.pwd(), "/dir1");

    // 尝试进入一个不存在的目录
    EXPECT_THROW(fs.cd("nonexistent"), std::runtime_error);

    // 确保当前工作目录未改变
    EXPECT_EQ(fs.pwd(), "/dir1");

    // 进入一个子目录然后返回父目录
    fs.cd("subdir1");
    EXPECT_EQ(fs.pwd(), "/dir1/subdir1");
    fs.cd("..");
    EXPECT_EQ(fs.pwd(), "/dir1");

    // 使用绝对路径改变目录
    fs.cd("/dir2");
    EXPECT_EQ(fs.pwd(), "/dir2");

    // 在dir2中创建一个新目录并进入
    fs.mkdir("subdir2");
    fs.cd("subdir2");
    EXPECT_EQ(fs.pwd(), "/dir2/subdir2");

    SUCCEED(); // 如果测试到达这里，则认为成功
}

// 测试删除文件夹
TEST(FileSystemTest, Test_rmdir) {
    FileSystem fs;
    fs.format();
    fs.mkdir("test");
    fs.rm("test");
    auto entries = fs.ls();
    // 检查是否有test目录
    for (auto &entry : entries) {
        if (entry == "test") {
            FAIL();
            return;
        }
    }
    SUCCEED();
}

TEST(FileSystemTest, ComprehensiveRmdirTest) {
    FileSystem fs;
    fs.format();

    // 测试1: 删除空目录
    fs.mkdir("emptyDir");
    fs.rm("emptyDir");
    auto entries = fs.ls();
    if (std::find(entries.begin(), entries.end(), "emptyDir") == entries.end()) {
        SUCCEED();
    } else {
        FAIL() << "Directory not removed from parent directory list.";
    }

    // 测试2: 尝试删除包含文件的非空目录
    fs.mkdir("nonEmptyDir");
    fs.cd("nonEmptyDir");
    fs.mkdir("subdir");
    fs.cd("..");
    try {
        fs.rm("nonEmptyDir");
        FAIL() << "Expected std::runtime_error for non-empty directory.";
    } catch (std::runtime_error const & err) {
        EXPECT_EQ(err.what(), std::string("Directory not empty: nonEmptyDir"));
    } catch (...) {
        FAIL() << "Expected std::runtime_error for non-empty directory.";
    }

    // 测试3: 尝试删除不存在的目录
    try {
        fs.rm("nonexistentDir");
        FAIL() << "Expected std::runtime_error for nonexistent directory.";
    } catch (std::runtime_error const & err) {
        EXPECT_EQ(err.what(), std::string("Directory not found: nonexistentDir"));
    } catch (...) {
        FAIL() << "Expected std::runtime_error for nonexistent directory.";
    }

    // 测试4: 删除目录后，确保目录从父目录列表中移除
    fs.mkdir("toBeDeleted");
    fs.rm("toBeDeleted");
    entries = fs.ls();
    if (std::find(entries.begin(), entries.end(), "toBeDeleted") == entries.end()) {
        SUCCEED();
    } else {
        FAIL() << "Directory not removed from parent directory list.";
    }

}

// 创建462个目录，然后删除，然后再创建
TEST(FileSystemTest, Test_rmdir_many) {
    const int NUM = 462;
    FileSystem fs;
    fs.format();
    for (int i = 1; i <= NUM; i++) {
        fs.mkdir("test" + std::to_string(i));
    }
    for (int i = 1; i <= NUM; i++) {
        fs.rm("test" + std::to_string(i));
    }
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

// 创建大量文件
TEST(FileSystemTest, Test_touch) {
    const int NUM = 462;
    FileSystem fs;
    fs.format();
    for (int i = 1; i <= NUM; i++) {
        fs.touch("test" + std::to_string(i));
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

// 创建大量文件，然后删除，然后再创建
TEST(FileSystemTest, Test_touch_many) {
    const int NUM = 462;
    FileSystem fs;
    fs.format();
    for (int i = 1; i <= NUM; i++) {
        fs.touch("test" + std::to_string(i));
    }
    for (int i = 1; i <= NUM; i++) {
        fs.rm("test" + std::to_string(i));
    }
    for (int i = 1; i <= NUM; i++) {
        fs.touch("test" + std::to_string(i));
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

// fclose 和 fopen
TEST(FileSystemTest, Test_fopen_fclose) {
    FileSystem fs;
    fs.format();
    fs.touch("test");
    auto fd = fs.fopen("test");
    fs.fclose(fd);
    SUCCEED();
}

// 复杂的文件打开关闭
TEST(FileSystemTest, Test_fopen_fclose_complex) {
    FileSystem fs;
    fs.format();
    fs.touch("test");
    auto fd = fs.fopen("test");
    EXPECT_THROW(fs.fopen("test"), std::runtime_error);

    fs.fclose(fd);

    // 期望会抛异常
    EXPECT_THROW(fs.fclose(fd), std::runtime_error);
    SUCCEED();
}

// 测试文件写入
TEST(FileSystemTest, Test_write) {
    FileSystem fs;
    fs.format();
    fs.touch("test");
    auto fd = fs.fopen("test");
    fs.fwrite(fd, "Hello, World!", 14);
    fs.fclose(fd);
    SUCCEED();
}

// 测试文件读写
TEST(FileSystemTest, Test_read) {
    FileSystem fs;
    fs.format();

    fs.touch("test");
    fs.save();

    auto fd = fs.fopen("test");
    fs.fwrite(fd, "Hello, World!", 14);
    fs.fclose(fd);

    fd = fs.fopen("test");
    char buffer[14];
    fs.fread(fd, buffer, 14);
    fs.fclose(fd);
    EXPECT_STREQ(buffer, "Hello, World!");
    SUCCEED();
}

// 写入超长文本
TEST(FileSystemTest, Test_write_long) {
    FileSystem fs;
    fs.format();
    fs.touch("test");
    auto fd = fs.fopen("test");
    std::string long_text(99999, 'a');
    fs.fwrite(fd, long_text.c_str(), 100000);
    fs.fclose(fd);

    // 读取文本
    fd = fs.fopen("test");
    char buffer[100000];
    fs.fread(fd, buffer, 100000);
    EXPECT_STREQ(buffer, long_text.c_str());

    fs.fclose(fd);
    SUCCEED();
}

// 测试fseek
TEST(FileSystemTest, Test_fseek) {
    FileSystem fs;
    fs.format();
    fs.touch("test");
    auto fd = fs.fopen("test");
    std::string long_text(99999, 'a');
    fs.fwrite(fd, long_text.c_str(), 100000);
    fs.fseek(fd, 0);
    char buffer[100000];
    fs.fread(fd, buffer, 100000);
    EXPECT_STREQ(buffer, long_text.c_str());
    fs.fseek(fd, 100);
    fs.fread(fd, buffer, 100000);
    EXPECT_STREQ(buffer, long_text.substr(100).c_str());
    fs.fclose(fd);
    SUCCEED();
}

// 测试fseek 2
TEST(FileSystemTest, Test_fseek_2) {
    FileSystem fs;
    fs.format();
    fs.touch("test");
    auto fd = fs.fopen("test");
    fs.fseek(fd, 31);
    fs.fwrite(fd, "Hello, World!", 14);
    fs.fclose(fd);
    SUCCEED();
}

// 测试：通过命令行方式测试:
// • 新建文件/test/Jerry，打开该文件，任意写入800个字节;
// • 将文件读写指针定位到第500字节，读出500个字节到字符串abc。 • 将abc写回文件。
TEST(FileSystemTest, Test_fseek_3) {
    FileSystem fs;
    fs.format();
    fs.touch("test");
    auto fd = fs.fopen("test");
    fs.fseek(fd, 0);
    std::string long_text(800, 'a');
    fs.fwrite(fd, long_text.c_str(), 800);
    fs.fseek(fd, 500);
    char buffer[500] {0};
    fs.fread(fd, buffer, 500);
    // buffer 此时的长度应该是300
    EXPECT_EQ(strlen(buffer), 300);

    fs.fwrite(fd, buffer, strlen(buffer));
    // 总文件大小是800 + 300
    fs.fseek(fd, 0);
    char buffer2[1101] {0};
    fs.fread(fd, buffer2, 1100);
    EXPECT_EQ(strlen(buffer2), 1100);
    fs.fclose(fd);
    SUCCEED();
}

// 上传1MB文件
TEST(FileSystemTest, Test_write_1MB) {
    const int FILE_SIZE = 512;
    const int len = 2048 * 1;
    // const int len = 2;
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
    fs.fwrite(fd, long_text.c_str(), FILE_SIZE * len);
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