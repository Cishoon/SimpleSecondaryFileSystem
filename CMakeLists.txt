cmake_minimum_required(VERSION 3.20)
project(FileSystem)

set(CMAKE_CXX_STANDARD 17)

include_directories(src include)

add_executable(FileSystem src/main.cpp
        src/shell/Shell.cpp
        include/shell/Shell.hpp
        src/disk_manager/DiskManager.cpp
        include/disk_manager/DiskManager.hpp
        src/fs/FileSystem.cpp
        include/fs/FileSystem.hpp
        include/fs/SuperBlock.hpp
        include/fs/DiskInode.hpp
        include/fs/Inode.hpp
        include/fs/File.hpp
        include/fs/FileType.hpp
        include/fs/DirectoryEntry.hpp
        include/fs/BufferCache.hpp
        include/common/common.hpp
        include/fs/CacheManager.hpp
)

# Google Test 集成
add_subdirectory(external/googletest)

# 定义测试可执行文件
add_executable(Tests
        tests/test_SuperBlock.cpp
        tests/test_DiskInode.cpp
        tests/test_DirectoryEntry.cpp
        tests/test_FileSystem.cpp
        src/disk_manager/DiskManager.cpp
        src/fs/FileSystem.cpp
        include/fs/FileSystem.hpp
)

# 使用更现代的方式设置包含目录
target_include_directories(Tests PRIVATE ${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

# 为你的目标添加编译和链接标志（可选）
#option(ENABLE_PROFILING "Enable profiling with gprof" ON)
#if(ENABLE_PROFILING)
    target_compile_options(Tests PRIVATE -pg)
    target_link_options(Tests PRIVATE -pg)
#endif()

# 添加宏定义，以便在编译测试代码时定义RUNNING_TESTS
target_compile_definitions(Tests PRIVATE RUNNING_TESTS)

# 链接Google Test库到测试可执行文件
target_link_libraries(Tests gtest gtest_main)

enable_testing()
add_test(NAME AllTests COMMAND Tests)