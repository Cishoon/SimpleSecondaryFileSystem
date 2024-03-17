#pragma once


#include <cstdint>
#include "Inode.hpp"

/**
- 引用计数：File有几个入边（只有父进程打开文件的时候fork出子进程，才会大于1），这次好像不会发生这个情况
    当两个进程打开同一个文件、或一个进程打开两次同一文件的时候，会创建两个不同的File结构，这两个File结构的Inode指针会指向同一个内存Inode，Inode的引用计数大于1.
- 指向分配给这个内存的Inode指针
- 文件读写指针：offset
 */
class File {
public:
    uint32_t reference_count = 0;
    uint32_t offset = 0;
    Inode* inode_ptr = nullptr;
};