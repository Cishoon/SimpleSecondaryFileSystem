//
// Created by Cishoon on 2024/3/14.
//

#include "fs/FileSystem.hpp"

std::vector<std::string> FileSystem::parse_path(const std::string &path) {
    std::vector<std::string> dirs;
    std::string dir;
    for (char c : path) {
        if (c == '/') {
            if (!dir.empty()) {
                dirs.push_back(dir);
                dir.clear();
            }
        } else {
            dir.push_back(c);
        }
    }
    if (!dir.empty()) {
        dirs.push_back(dir);
    }
    return dirs;
}

Inode *FileSystem::open_dir(const std::string &path) {
    /*
     * 把文件夹的DiskInode节点放到内存中来，返回这个Inode节点的指针
     * 如果已经在内存Inode中了，快速检索返回。
     * 如果不在内存Inode中，要先从磁盘中读到内存中来。
     * 如果内存Inode没有空间了，
     */

    // 暂且强制要求路径以 / 开头
    if (path[0] != '/') {
        throw std::runtime_error("Invalid path: " + path + ", path must start with /");
    }

    auto inode = allocate_memory_inode(1);
    if (path == "/") {
        return inode;
    }

    auto dirs = parse_path(path);
    for (const auto &dir: dirs) {
        // 在inode的所有目录里，找到名字是xxx的目录
        bool found = false;
        for (uint32_t i = 0; i < inode->get_directory_num(); i++) {
            if (inode->file_type != FileType::DIRECTORY) {
                throw std::runtime_error("Not a directory: " + path);
            }

            auto buffer = allocate_buffer_cache(get_block_pointer(inode, i / 16));
            auto dir_entry = reinterpret_cast<DirectoryEntry *>(buffer->data + sizeof(DirectoryEntry) * (i % 16));
            if (dir_entry->inode_id != 0 && dir_entry->name == dir) {
                inode = allocate_memory_inode(dir_entry->inode_id);
                found = true;
                break;
            }
        }
        if (!found) {
            std::stringstream ss;
            ss << "Directory not found: " <<  dir << " in " << path;
            throw std::runtime_error(ss.str());
        }
    }
    return inode;
}

void FileSystem::format() {
    /**
     * 初始化文件系统
     * 1. 磁盘文件清空
     * 2. 初始化磁盘文件的SuperBlock
     * 3. 初始化磁盘根目录
     */
    disk_manager.format(); // 清空磁盘文件
    super_block.format();  // 初始化SuperBlock

    // 清空打开文件表
    for (auto &open_file: open_files) {
        open_file = nullptr;
    }

    // 初始化内存Inode
    device_m_inodes.clear();
    free_m_inodes.clear();
    for (auto &m_inode: m_inodes) {
        m_inode.clear();
        free_m_inodes.push_back(&m_inode);
    }

    // 清除高速缓存
    device_buffer_cache.clear();
    free_buffer_cache.clear();
    for (auto &cache_block: buffer_cache) {
        cache_block.clear();
        free_buffer_cache.push_back(&cache_block);
    }

    /*
     * 创建根目录：
     *  1. 分配磁盘Inode节点，位于第1个Inode（2#扇区的第1块）（每个扇区的Inode从0~7，物理上是第二块）
     *  2. DiskInode要指向一个内存空间，内存空间里面是目录，一个扇区可以存 512/32=16个目录，一开始只占用前两项。
     */
    DiskInode root_inode;
    root_inode.file_size = sizeof(DirectoryEntry) * 2;
    root_inode.file_type = FileType::DIRECTORY;
    root_inode.block_pointers[0] = super_block.get_free_block();
    // 将DiskInode写入磁盘
    std::vector<char> root_inode_data(BLOCK_SIZE);
    std::memcpy(root_inode_data.data() + sizeof(DiskInode), &root_inode, sizeof(DiskInode));
    disk_manager.write_block(2, root_inode_data);
    super_block.inode_bitmap.set(1);

    // 初始化根目录
    DirectoryEntry root_dir[2];
    root_dir[0].inode_id = 1;
    std::strcpy(root_dir[0].name, ".");
    root_dir[1].inode_id = 1;
    std::strcpy(root_dir[1].name, "..");
    // 将根目录写入磁盘
    std::vector<char> root_dir_data(BLOCK_SIZE);
    std::memcpy(root_dir_data.data(), root_dir, sizeof(root_dir));
    disk_manager.write_block(root_inode.block_pointers[0], root_dir_data);

    // 把superblock写回磁盘，superblock大小是2个block，可以直接写回
    std::vector<char> super_block_data(sizeof(SuperBlock));
    std::memcpy(super_block_data.data(), &super_block, sizeof(super_block));
    disk_manager.write_block(0, super_block_data);

    current_path = "/";
}

const uint32_t &FileSystem::get_block_pointer(Inode *pInode, uint32_t i) {
    static const uint32_t PTRS_PER_BLOCK = BLOCK_SIZE / sizeof(uint32_t); // 每个块可以包含的指针数量

    // 5个直接索引，2个一次间接索引，2个二次间接索引和1个三次间接索引。
    if (i < 5) {
        // 直接索引
        return pInode->block_pointers[i];
    } else if (i < 5 + 2 * PTRS_PER_BLOCK) {
        // 一次间接索引
        uint32_t block_no = pInode->block_pointers[5 + (i - 5) / PTRS_PER_BLOCK];
        auto buffer = allocate_buffer_cache(block_no);
        return *reinterpret_cast<uint32_t *>(buffer->data + sizeof(uint32_t) * ((i - 5) % PTRS_PER_BLOCK));
    } else if (i < 5 + 2 * PTRS_PER_BLOCK + 2 * PTRS_PER_BLOCK * PTRS_PER_BLOCK) {
        // 二次间接索引
        i -= 5 + 2 * PTRS_PER_BLOCK;
        uint32_t block_no = pInode->block_pointers[7 + i / (PTRS_PER_BLOCK * PTRS_PER_BLOCK)];
        auto first_level_buffer = allocate_buffer_cache(block_no);
        block_no = *reinterpret_cast<uint32_t *>(first_level_buffer->data + sizeof(uint32_t) * ((i / PTRS_PER_BLOCK) % PTRS_PER_BLOCK));
        auto second_level_buffer = allocate_buffer_cache(block_no);
        return *reinterpret_cast<uint32_t *>(second_level_buffer->data + sizeof(uint32_t) * (i % PTRS_PER_BLOCK));
    } else {
        // 三次间接索引
        i -= 5 + 2 * PTRS_PER_BLOCK + 2 * PTRS_PER_BLOCK * PTRS_PER_BLOCK;
        uint32_t block_no = pInode->block_pointers[9];
        auto first_level_buffer = allocate_buffer_cache(block_no);
        block_no = *reinterpret_cast<uint32_t *>(first_level_buffer->data + sizeof(uint32_t) * ((i / (PTRS_PER_BLOCK * PTRS_PER_BLOCK)) % PTRS_PER_BLOCK));
        auto second_level_buffer = allocate_buffer_cache(block_no);
        block_no = *reinterpret_cast<uint32_t *>(second_level_buffer->data + sizeof(uint32_t) * ((i / PTRS_PER_BLOCK) % PTRS_PER_BLOCK));
        auto third_level_buffer = allocate_buffer_cache(block_no);
        return *reinterpret_cast<uint32_t *>(third_level_buffer->data + sizeof(uint32_t) * (i % PTRS_PER_BLOCK));
    }
}

DirectoryEntry *FileSystem::get_directory_entry(Inode *pInode, uint32_t i) {
    auto block_no = get_block_pointer(pInode, i / 16);
    if (block_no < 60) {
        // 未分配的内存空间
        throw std::runtime_error("Block not allocated: " + std::to_string(block_no));
    }
    auto buffer = allocate_buffer_cache(block_no);
    auto dir_entry = reinterpret_cast<DirectoryEntry *>(buffer->data + sizeof(DirectoryEntry) * (i % 16));
    // auto dir_entry = buffer->get_data<DirectoryEntry*>(i % 16);
    return dir_entry;
}

FileSystem::~FileSystem() {
    // 将内存Inode和高速缓存写回磁盘
    for (auto &m_inode: m_inodes) {
        write_back_inode(&m_inode);
    }

    for (auto &cache_block: buffer_cache) {
        if (cache_block.is_dirty()) {
            write_back_cache_block(&cache_block);
        }
    }
}

void FileSystem::write_back_inode(Inode *pInode) {
    auto disk_inode = Inode::to_disk_inode(*pInode);
    auto [block_no, _num] = inode_id_to_block_no(pInode->inode_id);
    auto buffer = allocate_buffer_cache(block_no);
    std::memcpy(buffer->data + sizeof(DiskInode) * _num, &disk_inode, sizeof(DiskInode));
    buffer->dirty = true;
}

void FileSystem::alloc_new_block(Inode *inode) {
    static const uint32_t PTRS_PER_BLOCK = BLOCK_SIZE / sizeof(uint32_t); // 每个块可以包含的指针数量

    // Inode 有一个 uint32_t block_pointers[10]
    // 直接索引
    for (int i = 0; i < 5; i++) {
        if(inode->block_pointers[i] == 0) {
            inode->block_pointers[i] = super_block.get_free_block();
            return;
        }
    }

    // 一次间接索引
    for (int i = 5; i < 7; i++) {
        if (inode->block_pointers[i] == 0) {
            inode->block_pointers[i] = super_block.get_free_block();
            auto buffer= allocate_buffer_cache(inode->block_pointers[i]);
            // buffer->clear();
            auto ptr = reinterpret_cast<uint32_t *>(buffer->data);
            for (int j = 0; j < PTRS_PER_BLOCK; j++) {
                if (ptr[j] == 0) {
                    ptr[j] = super_block.get_free_block();
                    return;
                }
            }
        } else {
            auto buffer = allocate_buffer_cache(inode->block_pointers[i]);
            auto ptr = reinterpret_cast<uint32_t *>(buffer->data);
            for (int j = 0; j < PTRS_PER_BLOCK; j++) {
                if (ptr[j] == 0) {
                    ptr[j] = super_block.get_free_block();
                    return;
                }
            }
        }
    }

    // 二次间接索引
    for (int i = 7; i < 9; i++) {
        if (inode->block_pointers[i] == 0) { // 如果二次间接索引还未分配
            inode->block_pointers[i] = super_block.get_free_block();
            auto first_level_buffer = allocate_buffer_cache(inode->block_pointers[i]);
            // first_level_buffer->clear();
        }
        auto first_level_buffer = allocate_buffer_cache(inode->block_pointers[i]);
        auto first_level_ptr = reinterpret_cast<uint32_t*>(first_level_buffer->data);
        for (int j = 0; j < PTRS_PER_BLOCK; j++) {
            if (first_level_ptr[j] == 0) { // 如果一级间接索引块中的指针未分配
                first_level_ptr[j] = super_block.get_free_block(); // 分配一级间接索引块
                auto second_level_buffer = allocate_buffer_cache(first_level_ptr[j]);
                // second_level_buffer->clear();
                auto second_level_ptr = reinterpret_cast<uint32_t*>(second_level_buffer->data);
                second_level_ptr[0] = super_block.get_free_block(); // 分配实际的数据块
                return;
            } else {
                auto second_level_buffer = allocate_buffer_cache(first_level_ptr[j]);
                auto second_level_ptr = reinterpret_cast<uint32_t*>(second_level_buffer->data);
                for (int k = 0; k < PTRS_PER_BLOCK; k++) {
                    if (second_level_ptr[k] == 0) { // 如果二级间接索引块中的指针未分配
                        second_level_ptr[k] = super_block.get_free_block(); // 分配实际的数据块
                        return;
                    }
                }
            }
        }
    }

    // 三次间接索引
    if (inode->block_pointers[9] == 0) { // 如果三次间接索引还未分配
        inode->block_pointers[9] = super_block.get_free_block();
        auto first_level_buffer = allocate_buffer_cache(inode->block_pointers[9]);
        // first_level_buffer->clear();
    }
    auto first_level_buffer = allocate_buffer_cache(inode->block_pointers[9]);
    auto first_level_ptr = reinterpret_cast<uint32_t*>(first_level_buffer->data);
    for (int i = 0; i < PTRS_PER_BLOCK; i++) {
        if (first_level_ptr[i] == 0) {
            first_level_ptr[i] = super_block.get_free_block();
            auto second_level_buffer = allocate_buffer_cache(first_level_ptr[i]);
            // second_level_buffer->clear();
        }
        auto second_level_buffer = allocate_buffer_cache(first_level_ptr[i]);
        auto second_level_ptr = reinterpret_cast<uint32_t*>(second_level_buffer->data);
        for (int j = 0; j < PTRS_PER_BLOCK; j++) {
            if (second_level_ptr[j] == 0) {
                second_level_ptr[j] = super_block.get_free_block();
                auto third_level_buffer = allocate_buffer_cache(second_level_ptr[j]);
                // third_level_buffer->clear();
                auto third_level_ptr = reinterpret_cast<uint32_t*>(third_level_buffer->data);
                third_level_ptr[0] = super_block.get_free_block(); // 分配实际的数据块
                return;
            } else {
                auto third_level_buffer = allocate_buffer_cache(second_level_ptr[j]);
                auto third_level_ptr = reinterpret_cast<uint32_t*>(third_level_buffer->data);
                for (int k = 0; k < PTRS_PER_BLOCK; k++) {
                    if (third_level_ptr[k] == 0) {
                        third_level_ptr[k] = super_block.get_free_block();
                        return;
                    }
                }
            }
        }
    }
}

void FileSystem::write_back_cache_block(BufferCache *pCache) {
    std::vector<char> data = std::vector<char>(pCache->data, pCache->data + BLOCK_SIZE);
    disk_manager.write_block(pCache->block_no, data);
    pCache->dirty = false;
}

void FileSystem::set_directory_entry(Inode *pInode, uint32_t num, uint32_t id, const std::string &basicString) {
    auto block_no = get_block_pointer(pInode, num / 16);
    if (block_no < 60) {
        throw std::runtime_error("Block not allocated: " + std::to_string(block_no));
    }
    auto buffer = allocate_buffer_cache(block_no);
    auto dir_entry = reinterpret_cast<DirectoryEntry *>(buffer->data + sizeof(DirectoryEntry) * (num % 16));
    dir_entry->inode_id = id;
    std::strcpy(dir_entry->name, basicString.c_str());
    buffer->dirty = true;
}

std::string FileSystem::get_current_dir() {
    // 解析路径最后一个 / 后的内容
    if (current_path == "/") {
        return "/";
    }
    auto pos = current_path.rfind('/');
    if (pos == std::string::npos) {
        throw std::runtime_error("Invalid current path: " + current_path);
    }
    return current_path.substr(pos + 1);
}

std::vector<std::string> FileSystem::ls() {
    auto inode = open_dir(current_path);
    std::vector<std::string> entries;
    for (uint32_t i = 0; i < inode->get_directory_num(); i++) {
        auto dir_entry = get_directory_entry(inode, i);
        if (dir_entry->inode_id != 0)
            entries.emplace_back(dir_entry->name);
    }
    return entries;
}

void FileSystem::mkdir(const std::string &dir_name) {
    auto dir_inode = open_dir(current_path);
    auto entries = ls();
    if (std::find(entries.begin(), entries.end(), dir_name) != entries.end()) {
        throw std::runtime_error("Directory already exists: " + dir_name);
    }

    // 创建新的目录文件
    auto new_dir_inode = allocate_memory_inode(super_block.get_free_inode());
    new_dir_inode->file_type = FileType::DIRECTORY;
    new_dir_inode->file_size = 2 * sizeof(DirectoryEntry);
    new_dir_inode->block_pointers[0] = super_block.get_free_block();


    // 更新当前目录
    auto buffer = allocate_buffer_cache(new_dir_inode->block_pointers[0]);
    auto dir_entry = reinterpret_cast<DirectoryEntry *>(buffer->data);
    dir_entry->inode_id = new_dir_inode->inode_id;
    std::strcpy(dir_entry->name, ".");
    dir_entry++;
    dir_entry->inode_id = dir_inode->inode_id;
    std::strcpy(dir_entry->name, "..");
    buffer->dirty = true;

    /*
     * 更新父目录
     * 首先找有没有被删除的文件, 他的inode_id = 0，这个位置可以放子目录
     * 如果找不到，在后面开辟一项
     */
    for (uint32_t i = 0; i < dir_inode->get_directory_num(); i++) {
        auto entry = get_directory_entry(dir_inode, i);
        if (entry->inode_id == 0) {
            entry->inode_id = new_dir_inode->inode_id;
            std::strcpy(entry->name, dir_name.c_str());
            return;
        }
    }
    // 需要开辟新的内存空间
    /*
     * 分两种情况：1.当前盘块还没满，直接在后面添加
     * 2. 当前盘块已经满了，增加一块，再添加在这个新的块里
     */
    if (dir_inode->get_directory_num() % 16 == 0) {
        alloc_new_block(dir_inode);
    }
    set_directory_entry(dir_inode, dir_inode->get_directory_num(), new_dir_inode->inode_id, dir_name);
    dir_inode->file_size += sizeof(DirectoryEntry);
}

std::string FileSystem::pwd() {
    return current_path;
}

void FileSystem::read_from_disk_to_cache(const uint32_t &block_no, BufferCache *cache_block) {
    auto data = disk_manager.read_block(block_no, 1);
    cache_block->block_no = block_no;
    std::move(data.begin(), data.end(), cache_block->data);
}

void FileSystem::write_cache_to_disk(BufferCache *cache_block) {
    disk_manager.write_block(cache_block->block_no,
                             std::vector<char>(cache_block->data, cache_block->data + BLOCK_SIZE));
    cache_block->dirty = false;
}

Inode *FileSystem::allocate_memory_inode(const uint32_t &inode_id) {
    // 如果inode_id对应的Inode已经在内存Inode中了，直接返回
    for (auto &m_inode: m_inodes) {
        if (m_inode.inode_id == inode_id) {
            // 将这个Inode插回队列的末尾
            device_m_inodes.remove(&m_inode);
            device_m_inodes.push_back(&m_inode);
            return &m_inode;
        }
    }

    // 如果有空闲的Inode
    if (!free_m_inodes.empty()) {
        auto m_inode = free_m_inodes.front();
        free_m_inodes.pop_front();

        device_m_inodes.push_back(m_inode);
        // 从高速缓存块中读取相应的数据，写进来
        auto [block_no, _num] = inode_id_to_block_no(inode_id);
        auto buffer = allocate_buffer_cache(block_no);
        DiskInode disk_inode = *reinterpret_cast<DiskInode *>(buffer->data + sizeof(DiskInode) * _num);
        *m_inode = Inode::to_inode(disk_inode, inode_id);
        return m_inode;
    }

    auto m_inode = device_m_inodes.front();
    device_m_inodes.pop_front();
    // 将这个被换出的m_inode写入对应的缓存块
    {
        auto [block_no, _num] = inode_id_to_block_no(m_inode->inode_id);
        auto buffer = allocate_buffer_cache(block_no);
        DiskInode disk_inode = Inode::to_disk_inode(*m_inode);
        std::memcpy(buffer->data + sizeof(DiskInode) * _num, reinterpret_cast<char *>(&disk_inode),
                    sizeof(DiskInode));
        buffer->dirty = true;
    }
    // 从高速缓存块中读取相应的数据，写进来
    auto [block_no, _num] = inode_id_to_block_no(inode_id);
    auto buffer = allocate_buffer_cache(block_no);
    DiskInode disk_inode = *reinterpret_cast<DiskInode *>(buffer->data + sizeof(DiskInode) * _num);
    *m_inode = Inode::to_inode(disk_inode, inode_id);
    device_m_inodes.push_back(m_inode);
    return m_inode;
}

BufferCache *FileSystem::allocate_buffer_cache(const uint32_t &block_no) {
    // 如果盘块号已经在高速缓存中，直接返回
    for (auto &cache_block: buffer_cache) {
        if (cache_block.block_no == block_no) {
            // 将这个块插回队列的末尾
            device_buffer_cache.remove(&cache_block);
            device_buffer_cache.push_back(&cache_block);
            return &cache_block;
        }
    }

    // 如果有空闲缓存块，直接分配
    if (!free_buffer_cache.empty()) {
        auto cache_block = free_buffer_cache.front();
        free_buffer_cache.pop_front();

        device_buffer_cache.push_back(cache_block);
        read_from_disk_to_cache(block_no, cache_block);

        return cache_block;
    }

    // 如果没有空闲缓存块，需要替换
    auto cache_block = device_buffer_cache.front();
    device_buffer_cache.pop_front();
    if (cache_block->is_dirty()) {
        write_cache_to_disk(cache_block);
    }
    read_from_disk_to_cache(block_no, cache_block);
    device_buffer_cache.push_back(cache_block);
    return cache_block;
}

FileSystem::FileSystem() : disk_manager(DISK_PATH, DISK_SIZE), open_files() {
    // 读取磁盘文件的SuperBlock
    auto super_block_data = disk_manager.read_block(0, 2);
    super_block = *reinterpret_cast<SuperBlock *>(super_block_data.data());

    // 初始化打开文件表, 全部置空
    for (auto &open_file: open_files) {
        open_file = nullptr;
    }

    // 初始化内存Inode
    for (auto &m_inode: m_inodes) {
        m_inode.clear();
        free_m_inodes.push_back(&m_inode);
    }
    device_m_inodes.clear();

    // 清除高速缓存
    for (auto &cache_block: buffer_cache) {
        cache_block.clear();
        free_buffer_cache.push_back(&cache_block);
    }
    device_buffer_cache.clear();

    // 打开根目录
    current_path = "/";
}

std::pair<uint32_t, uint32_t> FileSystem::inode_id_to_block_no(const uint32_t &inode_id) {
    uint32_t block_no = inode_id / 8 + 2;
    uint32_t offset = inode_id % 8;
    return std::make_pair(block_no, offset);
}

