#include <gtest/gtest.h>

#include "fs/DirectoryEntry.hpp"

TEST(DirectoryEntry, size) {
    EXPECT_EQ(sizeof(DirectoryEntry), 32);
}