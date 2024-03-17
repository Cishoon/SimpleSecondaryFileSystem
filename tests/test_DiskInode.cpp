#include <gtest/gtest.h>
#include "fs/DiskInode.hpp"

// 测试DiskInode的大小是否正确，即是否为64字节
TEST(DiskInodeTest, TestSize) {
    DiskInode disk_inode;
    EXPECT_EQ(sizeof(disk_inode), 64);
}

