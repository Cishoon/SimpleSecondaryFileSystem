#include <gtest/gtest.h>
#include "fs/SuperBlock.hpp"

// 测试SuperBlock的大小是否正确，即是否为1024字节
TEST(SuperBlockTest, TestSize) {
    SuperBlock sb;
    EXPECT_EQ(sizeof(sb), SUPER_BLOCK_SIZE * 512);
}

// 测试SuperBlock的初始化
TEST(SuperBlockTest, TestInit) {
    SuperBlock sb;
    EXPECT_EQ(sb.block_count, BLOCK_COUNT);
    EXPECT_EQ(sb.inode_count, INODE_COUNT);
    EXPECT_EQ(sb.dirty_flag, 0);
    EXPECT_EQ(sb.inode_bitmap.count(), 0);
    EXPECT_EQ(sb.block_bitmap.count(), 0);
}

