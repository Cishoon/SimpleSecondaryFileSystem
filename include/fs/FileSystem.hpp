#pragma once

#include <list>
#include <iostream>
#include <sstream>
#include "SuperBlock.hpp"
#include "DiskInode.hpp"
#include "disk_manager/DiskManager.hpp"
#include "Inode.hpp"
#include "File.hpp"
#include "DirectoryEntry.hpp"
#include "BufferCache.hpp"

#ifdef RUNNING_TESTS
#define DISK_PATH "disk_dev.img"
#define DISK_SIZE (1LL << 30)
#else
#define DISK_PATH "disk.img"
#define DISK_SIZE (1LL << 30)
#endif

#define MEMORY_INODE_NUM (100)  // 内存Inode数量
#define OPEN_FILE_NUM (15)      // 同时打开文件数量上限

#define CACHE_BLOCK_NUM (16)   // 高速缓存块数量

class FileSystem {
private:
    DiskManager disk_manager;

private:
    // SuperBlock
    SuperBlock super_block;

    // 打开文件表
    std::array<File *, OPEN_FILE_NUM> open_files;


    // 内存Inode
    std::array<Inode, MEMORY_INODE_NUM> m_inodes;
    // 约定：写入数据push_back，读取数据pop_front
    std::list<Inode *> device_m_inodes;
    std::list<Inode *> free_m_inodes;

    // 内存高速缓存
    std::array<BufferCache, CACHE_BLOCK_NUM> buffer_cache;
    // 约定：写入数据push_back，读取数据pop_front
    std::list<BufferCache *> device_buffer_cache; // 设备缓存队列，有哪些缓存已经被加载在内存中了
    std::list<BufferCache *> free_buffer_cache; // 空闲缓存队列

private:
    // 当前文件路径
    std::string current_path;

public:
    FileSystem();

    ~FileSystem();

    void format();

private:
    /**
     * 给一个盘块分配高速缓存
     * @param block_no 盘块号
     * @return BufferCache* 高速缓存块指针
     */
    BufferCache *allocate_buffer_cache(const uint32_t &block_no);

    Inode *allocate_memory_inode(const uint32_t &inode_id);

    /**
     * 从磁盘读取数据到高速缓存快
     * @param block_no 盘块号
     * @param cache_block 高速缓存块
     */
    void read_from_disk_to_cache(const uint32_t &block_no, BufferCache *cache_block);

    /**
     * 将高速缓存块的数据写入磁盘
     * @param cache_block 高速缓存块
     */
    void write_cache_to_disk(BufferCache *cache_block);

    /**
     * 转换Inode编号到实际盘块号、第几个
     * @param inode_id Inode 编号
     * @return std::pair<uint32_t, uint32_t> [第几个盘块(2~59)，第几个(0~7)]
     */
    static std::pair<uint32_t, uint32_t> inode_id_to_block_no(const uint32_t &inode_id);


    /**
     * 获取Inode的第i个数据块指针
     * @param pInode  Inode指针
     * @param i     第i个数据块
     * @return    第i个数据块指针
     */
    const uint32_t &get_block_pointer(Inode *pInode, uint32_t i);

    /**
     * 打开文件夹
     * @param path 文件夹路径
     * @return Inode* 文件夹Inode指针
     */
    Inode *open_dir(const std::string &path);

    /**
     * 给Inode分配一个新的盘块
     * @param inode
     */
    void alloc_new_block(Inode* inode);

    void write_back_inode(Inode *pInode);

    void write_back_cache_block(BufferCache *pCache);


public:
    static std::vector<std::string> parse_path(const std::string &path);

    std::string pwd();

    /**
     * 获取当前所在文件夹
     * @return 当前文件夹
     */
    std::string get_current_dir();


    void set_directory_entry(Inode *pInode, uint32_t num, uint32_t id, const std::string &basicString);

    /**
     * 在当前目录下，创建新的目录文件 mkdir
     * @param dir_name 目录名
     */
    void mkdir(const std::string &dir_name);

    /**
     * 获取这个Inode节点的第i个目录项
     * @param pInode  Inode指针
     * @param i     第i个目录项
     * @return 目录项指针
     */
    DirectoryEntry *get_directory_entry(Inode *pInode, uint32_t i);

    /**
     * 列出当前目录下所有文件 ls
     */
    std::vector<std::string> ls();

private:



};