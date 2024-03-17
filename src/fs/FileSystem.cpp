//
// Created by Cishoon on 2024/3/14.
//

#include "fs/FileSystem.hpp"

std::vector<std::string> FileSystem::parse_path(const std::string &path) {
    std::vector<std::string> dirs;
    std::string dir;
    for (char c: path) {
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

Inode *FileSystem::get_inode_by_path(const std::string &path) {
    auto _current_inode_id = current_inode_id;

    std::string file = path.substr(path.find_last_of('/') + 1);
    std::string parent_path = path.substr(0, path.size() - file.size());

    cd(parent_path);
    auto inode = allocate_memory_inode(current_inode_id);
    for (uint32_t i = 0; i < inode->get_directory_num(); i++) {
        auto dir_entry = get_directory_entry(inode, i);
        if (dir_entry->inode_id != 0 && std::string(dir_entry->name) == file) {
            current_inode_id = _current_inode_id;
            return allocate_memory_inode(dir_entry->inode_id);
        }
    }

    current_inode_id = _current_inode_id;
    throw std::runtime_error("File not found: " + path);
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
        open_file.clear();
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

    current_inode_id = 1;
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
        // return *reinterpret_cast<uint32_t *>(buffer->data + sizeof(uint32_t) * ((i - 5) % PTRS_PER_BLOCK));
        return *buffer->read<uint32_t>((i - 5) % PTRS_PER_BLOCK);
    } else if (i < 5 + 2 * PTRS_PER_BLOCK + 2 * PTRS_PER_BLOCK * PTRS_PER_BLOCK) {
        // 二次间接索引
        i -= 5 + 2 * PTRS_PER_BLOCK;
        uint32_t block_no = pInode->block_pointers[7 + i / (PTRS_PER_BLOCK * PTRS_PER_BLOCK)];
        auto first_level_buffer = allocate_buffer_cache(block_no);
        // block_no = *reinterpret_cast<uint32_t *>(first_level_buffer->data + sizeof(uint32_t) * ((i / PTRS_PER_BLOCK) % PTRS_PER_BLOCK));
        block_no = *first_level_buffer->read<uint32_t>((i / PTRS_PER_BLOCK) % PTRS_PER_BLOCK);
        auto second_level_buffer = allocate_buffer_cache(block_no);
        // return *reinterpret_cast<uint32_t *>(second_level_buffer->data + sizeof(uint32_t) * (i % PTRS_PER_BLOCK));
        return *second_level_buffer->read<uint32_t>(i % PTRS_PER_BLOCK);
    } else {
        // 三次间接索引
        i -= 5 + 2 * PTRS_PER_BLOCK + 2 * PTRS_PER_BLOCK * PTRS_PER_BLOCK;
        uint32_t block_no = pInode->block_pointers[9];
        auto first_level_buffer = allocate_buffer_cache(block_no);
        // block_no = *reinterpret_cast<uint32_t *>(first_level_buffer->data + sizeof(uint32_t) * ((i / (PTRS_PER_BLOCK * PTRS_PER_BLOCK)) % PTRS_PER_BLOCK));
        block_no = *first_level_buffer->read<uint32_t>((i / (PTRS_PER_BLOCK * PTRS_PER_BLOCK)) % PTRS_PER_BLOCK);
        auto second_level_buffer = allocate_buffer_cache(block_no);
        // block_no = *reinterpret_cast<uint32_t *>(second_level_buffer->data + sizeof(uint32_t) * ((i / PTRS_PER_BLOCK) % PTRS_PER_BLOCK));
        block_no = *second_level_buffer->read<uint32_t>((i / PTRS_PER_BLOCK) % PTRS_PER_BLOCK);
        auto third_level_buffer = allocate_buffer_cache(block_no);
        // return *reinterpret_cast<uint32_t *>(third_level_buffer->data + sizeof(uint32_t) * (i % PTRS_PER_BLOCK));
        return *third_level_buffer->read<uint32_t>(i % PTRS_PER_BLOCK);
    }
}

const DirectoryEntry *FileSystem::get_directory_entry(Inode *pInode, uint32_t i) {
    auto block_no = get_block_pointer(pInode, i / 16);
    if (block_no < BLOCK_START_INDEX) {
        // 未分配的内存空间
        throw std::runtime_error("Block not allocated: " + std::to_string(block_no));
    }
    auto buffer = allocate_buffer_cache(block_no);
    // auto dir_entry = reinterpret_cast<DirectoryEntry *>(buffer->data + sizeof(DirectoryEntry) * (i % 16));
    auto dir_entry = buffer->read<DirectoryEntry>(i % 16);
    return dir_entry;
}

FileSystem::~FileSystem() {
    save();
}

void FileSystem::write_back_inode(Inode *pInode) {
    auto disk_inode = Inode::to_disk_inode(*pInode);
    auto [block_no, _num] = inode_id_to_block_no(pInode->inode_id);
    auto buffer = allocate_buffer_cache(block_no);
    // std::memcpy(buffer->data + sizeof(DiskInode) * _num, &disk_inode, sizeof(DiskInode));
    // buffer->dirty = true;
    // buffer->write<DiskInode>(&disk_inode, _num);
    write_buffer(buffer, &disk_inode, _num);
}

void FileSystem::alloc_new_block(Inode *inode) {
    static const uint32_t PTRS_PER_BLOCK = BLOCK_SIZE / sizeof(uint32_t); // 每个块可以包含的指针数量

    // Inode 有一个 uint32_t block_pointers[10]
    // 直接索引
    for (int i = 0; i < 5; i++) {
        if (inode->block_pointers[i] == 0) {
            inode->block_pointers[i] = super_block.get_free_block();
            return;
        }
    }

    // 一次间接索引
    for (int i = 5; i < 7; i++) {
        if (inode->block_pointers[i] == 0) {
            inode->block_pointers[i] = super_block.get_free_block();
            auto buffer = allocate_buffer_cache(inode->block_pointers[i]);
            auto ptr = buffer->read<uint32_t>(0);
            for (int j = 0; j < PTRS_PER_BLOCK; j++) {
                if (ptr[j] == 0) {
                    auto id = super_block.get_free_block();
                    write_buffer(buffer, &id, j);
                    return;
                }
            }
        } else {
            auto buffer = allocate_buffer_cache(inode->block_pointers[i]);
            auto ptr = buffer->read<uint32_t>(0);
            for (int j = 0; j < PTRS_PER_BLOCK; j++) {
                if (ptr[j] == 0) {
                    auto id = super_block.get_free_block();
                    write_buffer(buffer, &id, j);
                    return;
                }
            }
        }
    }

    // 二次间接索引
    for (int i = 7; i < 9; i++) {
        if (inode->block_pointers[i] == 0) { // 如果二次间接索引还未分配
            inode->block_pointers[i] = super_block.get_free_block();
        }
        auto first_level_buffer = allocate_buffer_cache(inode->block_pointers[i]);
        auto first_level_ptr = first_level_buffer->read<uint32_t>(0);
        for (int j = 0; j < PTRS_PER_BLOCK; j++) {
            if (first_level_ptr[j] == 0) { // 如果一级间接索引块中的指针未分配
                auto id = super_block.get_free_block();
                write_buffer(first_level_buffer, &id, j);
                auto second_level_buffer = allocate_buffer_cache(first_level_ptr[j]);
                auto id2 = super_block.get_free_block();
                write_buffer(second_level_buffer, &id2, 0);
                return;
            } else {
                auto second_level_buffer = allocate_buffer_cache(first_level_ptr[j]);
                auto second_level_ptr = second_level_buffer->read<uint32_t>(0);
                for (int k = 0; k < PTRS_PER_BLOCK; k++) {
                    if (second_level_ptr[k] == 0) { // 如果二级间接索引块中的指针未分配
                        auto id = super_block.get_free_block();
                        write_buffer(second_level_buffer, &id, k);
                        return;
                    }
                }
            }
        }
    }

    // 三次间接索引
    if (inode->block_pointers[9] == 0) { // 如果三次间接索引还未分配
        inode->block_pointers[9] = super_block.get_free_block();
    }
    auto first_level_buffer = allocate_buffer_cache(inode->block_pointers[9]);
    auto first_level_ptr = first_level_buffer->read<uint32_t>(0);
    for (int i = 0; i < PTRS_PER_BLOCK; i++) {
        if (first_level_ptr[i] == 0) {
            auto id = super_block.get_free_block();
            write_buffer(first_level_buffer, &id, i);
        }
        auto second_level_buffer = allocate_buffer_cache(first_level_ptr[i]);
        auto second_level_ptr = second_level_buffer->read<uint32_t>(0);
        for (int j = 0; j < PTRS_PER_BLOCK; j++) {
            if (second_level_ptr[j] == 0) {
                auto id = super_block.get_free_block();
                write_buffer(second_level_buffer, &id, j);
                auto third_level_buffer = allocate_buffer_cache(second_level_ptr[j]);
                auto id2 = super_block.get_free_block();
                write_buffer(third_level_buffer, &id2, 0);
                return;
            } else {
                auto third_level_buffer = allocate_buffer_cache(second_level_ptr[j]);
                auto third_level_ptr = third_level_buffer->read<uint32_t>(0);
                for (int k = 0; k < PTRS_PER_BLOCK; k++) {
                    if (third_level_ptr[k] == 0) {
                        auto id = super_block.get_free_block();
                        write_buffer(third_level_buffer, &id, k);
                        return;
                    }
                }
            }
        }
    }
}

void FileSystem::write_back_cache_block(BufferCache *pCache) {
    auto p_start = pCache->read<char>(0);
    std::vector<char> data = std::vector<char>(p_start, p_start + BLOCK_SIZE);
    disk_manager.write_block(pCache->block_no, data);
    pCache->set_dirty(false);
}

void FileSystem::set_directory_entry(Inode *pInode, uint32_t num, uint32_t id, const std::string &basicString) {
    auto block_no = get_block_pointer(pInode, num / 16);
    if (block_no < BLOCK_START_INDEX) {
        throw std::runtime_error("Block not allocated: " + std::to_string(block_no));
    }
    auto buffer = allocate_buffer_cache(block_no);
    DirectoryEntry entry(id, basicString.c_str());
    write_buffer(buffer, &entry, num % 16);
}

std::string FileSystem::get_current_dir() {
    std::string current_path = pwd();
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
    auto inode = allocate_memory_inode(current_inode_id);
    std::vector<std::string> entries;
    for (uint32_t i = 0; i < inode->get_directory_num(); i++) {
        auto dir_entry = get_directory_entry(inode, i);
        if (dir_entry->inode_id != 0)
            entries.emplace_back(dir_entry->name);
    }
    return entries;
}

void FileSystem::mkdir(const std::string &dir_name) {
    // 目录名最长28字节
    if (dir_name.size() > 28) {
        throw std::runtime_error("Directory name too long: " + dir_name);
    }

    auto dir_inode = allocate_memory_inode(current_inode_id);
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
    DirectoryEntry entry1(new_dir_inode->inode_id, ".");
    DirectoryEntry entry2(dir_inode->inode_id, "..");
    write_buffer(buffer, &entry1, 0);
    write_buffer(buffer, &entry2, 1);

    /*
     * 更新父目录
     * 首先找有没有被删除的文件, 他的inode_id = 0，这个位置可以放子目录
     * 如果找不到，在后面开辟一项
     */
    for (uint32_t i = 0; i < dir_inode->get_directory_num(); i++) {
        auto entry = get_directory_entry(dir_inode, i);
        if (entry->inode_id == 0) {
            set_directory_entry(dir_inode, i, new_dir_inode->inode_id, dir_name);
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
    // 从当前目录开始，一直通过 .. 找到父目录，直到到根目录
    std::string path;
    auto inode = allocate_memory_inode(current_inode_id);

    while (inode->inode_id != 1) {
        auto parent_inode = allocate_memory_inode(get_parent_inode_id(inode));
        for (uint32_t i = 0; i < parent_inode->get_directory_num(); i++) {
            auto dir_entry = get_directory_entry(parent_inode, i);
            if (dir_entry->inode_id == inode->inode_id) {
                std::stringstream ss;
                ss << "/" << dir_entry->name << path;
                path = ss.str();
                break;
            }
        }
        inode = parent_inode;
    }

    if (path.empty()) {
        path = "/";
    }
    return path;
}

void FileSystem::read_from_disk_to_cache(const uint32_t &block_no, BufferCache *cache_block) {
    auto data = disk_manager.read_block(block_no, 1);
    cache_block->block_no = block_no;
    (void) cache_block->write<char>(data.data(), 0, data.size());
}

void FileSystem::write_cache_to_disk(BufferCache *cache_block) {
    auto ptr = cache_block->read<char>(0);
    auto data = std::vector<char>(ptr, ptr + BLOCK_SIZE);

    disk_manager.write_block(cache_block->block_no, data);
    cache_block->set_dirty(false);
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
        DiskInode disk_inode = *buffer->read<DiskInode>(_num);
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
        write_buffer(buffer, &disk_inode, _num);
    }
    // 从高速缓存块中读取相应的数据，写进来
    auto [block_no, _num] = inode_id_to_block_no(inode_id);
    auto buffer = allocate_buffer_cache(block_no);
    DiskInode disk_inode = *buffer->read<DiskInode>(_num);
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
        open_file.clear();
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
    current_inode_id = 1;

    if (exist("/root")) {
        cd("/root");
    }
}

std::pair<uint32_t, uint32_t> FileSystem::inode_id_to_block_no(const uint32_t &inode_id) {
    uint32_t block_no = inode_id / 8 + 2;
    uint32_t offset = inode_id % 8;
    return std::make_pair(block_no, offset);
}

template<typename T>
void FileSystem::write_buffer(BufferCache *pCache, const T *value, const uint32_t &index, uint32_t size, bool index_by_char) {
    bool write_to_bottom = pCache->write<T>(value, index, size, index_by_char);
    if (write_to_bottom) {
        write_cache_to_disk(pCache);
    }
}

void FileSystem::cd(const std::string &path) {
    if (path.empty()) {
        return;
    }

    Inode *current_inode = path[0] == '/' ? allocate_memory_inode(1) :
                           allocate_memory_inode(current_inode_id);

    auto dirs = parse_path(path);
    for (const auto &dir: dirs) {
        // 如果当前目录中找不到dir，抛出异常
        bool found = false;
        for (uint32_t i = 0; i < current_inode->get_directory_num(); i++) {
            auto dir_entry = get_directory_entry(current_inode, i);
            if (dir_entry->inode_id != 0 && dir_entry->name == dir) {
                current_inode = allocate_memory_inode(dir_entry->inode_id);
                found = true;
                break;
            }
        }
        if (!found) {
            std::stringstream ss;
            ss << "Directory not found: " << dir;
            throw std::runtime_error(ss.str());
        }
        if (!current_inode->is_directory()) {
            std::stringstream ss;
            ss << "Not directory: " << dir;
            throw std::runtime_error(ss.str());
        }
    }

    current_inode_id = current_inode->inode_id;
}

const uint32_t &FileSystem::get_parent_inode_id(Inode *pInode) {
    for (uint32_t i = 0; i < pInode->get_directory_num(); i++) {
        auto dir_entry = get_directory_entry(pInode, i);
        if (dir_entry->inode_id != 0 && std::string(dir_entry->name) == "..") {
            return dir_entry->inode_id;
        }
    }
    throw std::runtime_error("Parent directory not found");
}

void FileSystem::rm(const std::string &dir_name) {
    auto dir_inode = allocate_memory_inode(current_inode_id);
    for (uint32_t i = 0; i < dir_inode->get_directory_num(); i++) {
        auto entry = get_directory_entry(dir_inode, i);
        if (entry->inode_id != 0 && std::string(entry->name) == dir_name) {
            auto inode = allocate_memory_inode(entry->inode_id);
            if (inode->is_directory() && inode->get_directory_num() > 2) {
                throw std::runtime_error("Directory not empty: " + dir_name);
            }
            // set_directory_entry(dir_inode, i, 0, "");
            // 把当前文件夹下的最后一个目录覆盖到这个位置
            auto last_entry = get_directory_entry(dir_inode, dir_inode->get_directory_num() - 1);
            set_directory_entry(dir_inode, i, last_entry->inode_id, last_entry->name);
            set_directory_entry(dir_inode, dir_inode->get_directory_num() - 1, 0, ""); // TODO 可以删去这一行，保留为了测试的时候好看
            dir_inode->file_size -= sizeof(DirectoryEntry);
            free_memory_inode(inode);
            return;
        }
    }
    throw std::runtime_error("Directory not found: " + dir_name);
}

void FileSystem::init() {
    format();
    mkdir("root");
    mkdir("home");
    mkdir("etc");
    mkdir("bin");
    mkdir("usr");
    mkdir("dev");
    cd("/root");
}

bool FileSystem::exist(const std::string &path) {
    try {
        get_inode_by_path(path);
        return true;
    } catch (std::runtime_error &e) {
        return false;
    }
}

void FileSystem::touch(const std::string &file_name) {
    auto dir_inode = allocate_memory_inode(current_inode_id);
    auto entries = ls();
    if (std::find(entries.begin(), entries.end(), file_name) != entries.end()) {
        throw std::runtime_error("File already exists: " + file_name);
    }

    auto new_file_inode = allocate_memory_inode(super_block.get_free_inode());
    new_file_inode->file_type = FileType::FILE;
    new_file_inode->file_size = 0;

    for (uint32_t i = 0; i < dir_inode->get_directory_num(); i++) {
        auto entry = get_directory_entry(dir_inode, i);
        if (entry->inode_id == 0) {
            set_directory_entry(dir_inode, i, new_file_inode->inode_id, file_name);
            return;
        }
    }
    if (dir_inode->get_directory_num() % 16 == 0) {
        alloc_new_block(dir_inode);
    }
    set_directory_entry(dir_inode, dir_inode->get_directory_num(), new_file_inode->inode_id, file_name);
    dir_inode->file_size += sizeof(DirectoryEntry);
}

void FileSystem::free_memory_inode(Inode *pInode) {
    free_m_inodes.push_back(pInode);
    device_m_inodes.remove(pInode);

    // 释放Inode
    super_block.inode_bitmap.reset(pInode->inode_id);

    // 释放Inode指向的所有数据块
    for (int i = 0; i < (pInode->file_size / BLOCK_SIZE) + 1; i++) {
        auto block_no = get_block_pointer(pInode, i);
        if (block_no >= BLOCK_START_INDEX) {
            super_block.block_bitmap.reset(block_no - BLOCK_START_INDEX);
        }
    }
}

void FileSystem::save() {
    // 将superblock写回
    std::vector<char> super_block_data(sizeof(SuperBlock));
    std::memcpy(super_block_data.data(), &super_block, sizeof(super_block));
    disk_manager.write_block(0, super_block_data);

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

uint32_t FileSystem::fopen(const std::string &file_path) {
    auto dir_inode = get_inode_by_path(file_path);
    if (dir_inode->is_directory()) {
        throw std::runtime_error("Is a directory instead of file: " + file_path);
    }
    for (int fd = 0; fd < OPEN_FILE_NUM; fd++) {
        auto &open_file = open_files[fd];
        if (open_file.is_busy() && open_file.inode_id == dir_inode->inode_id) {
            // 抛出异常：文件+文件路径+已经被打开，编号是[fd]
            std::stringstream ss;
            ss << "File " << file_path << " already opened, fd=[" << fd << "]";
            throw std::runtime_error(ss.str());
        }
    }
    for (int fd = 0; fd < OPEN_FILE_NUM; fd++) {
        auto &open_file = open_files[fd];
        if (!open_file.is_busy()) {
            open_file.inode_id = dir_inode->inode_id;
            open_file.offset = 0;
            open_file.reference_count++;
            return fd;
        }
    }
    // 超过最大打开文件数量
    throw std::runtime_error("Exceeded maximum number of open files");
}

void FileSystem::fclose(const uint32_t &file_id) {
    auto &open_file = open_files[file_id];
    if (open_file.is_busy()) {
        open_file.reference_count--;
        if (open_file.reference_count == 0) {
            open_file.clear();
            return;
        }
    }
    // 抛异常，没有打开的文件编号是[fd]
    throw std::runtime_error("No open file, fd=[" + std::to_string(file_id) + "]");
}

void FileSystem::fwrite(const uint32_t &file_id, const char *data, const uint32_t &size) {
    auto &open_file = open_files[file_id];
    if (!open_file.is_busy()) {
        throw std::runtime_error("File not opened: " + std::to_string(file_id));
    }
    auto inode = allocate_memory_inode(open_file.inode_id);

    uint32_t offset = open_file.offset;
    uint32_t ptr = offset;
    while(ptr - offset < size) {
        // 获取、分配数据块
        if (inode->file_size % BLOCK_SIZE == 0) {
            alloc_new_block(inode);
        }
        auto block_no = get_block_pointer(inode, ptr / BLOCK_SIZE);

        // 写入数据块
        auto buffer = allocate_buffer_cache(block_no);
        uint32_t write_size = std::min(BLOCK_SIZE - ptr % BLOCK_SIZE, size - (ptr - offset));
        write_buffer(buffer, data + (ptr - offset), ptr % BLOCK_SIZE, write_size, true);
        ptr += write_size;

        // 更新数据
        inode->file_size = std::max(inode->file_size, ptr);
        open_file.offset = ptr;
    }
}

void FileSystem::fread(const uint32_t &file_id, char *data, const uint32_t &size) {
    auto &open_file = open_files[file_id];
    if (!open_file.is_busy()) {
        throw std::runtime_error("File not opened: " + std::to_string(file_id));
    }
    auto inode = allocate_memory_inode(open_file.inode_id);

    uint32_t offset = open_file.offset;
    uint32_t ptr = offset;
    while(ptr - offset < size && ptr < inode->file_size) {
        // 获取数据块
        auto block_no = get_block_pointer(inode, ptr / BLOCK_SIZE);
        if (block_no < BLOCK_START_INDEX) {
            throw std::runtime_error("Block not allocated: " + std::to_string(block_no));
        }

        // 读取数据块
        auto buffer = allocate_buffer_cache(block_no);
        uint32_t read_size = std::min(BLOCK_SIZE - ptr % BLOCK_SIZE, size - (ptr - offset));
        read_size = std::min(read_size, inode->file_size - ptr);

        auto ptr_data = buffer->read<char>(ptr % BLOCK_SIZE);
        std::memcpy(data + (ptr - offset), ptr_data, read_size);
        ptr += read_size;

        // 更新数据
        open_file.offset = ptr;
    }
}

void FileSystem::fseek(const uint32_t &file_id, const uint32_t &offset) {
    auto &open_file = open_files[file_id];
    if (!open_file.is_busy()) {
        throw std::runtime_error("File not opened: " + std::to_string(file_id));
    }
    open_file.offset = offset;
}

std::string FileSystem::cat(const std::string &file_name) {
    auto inode = get_inode_by_path(file_name);
    if (inode->is_directory()) {
        throw std::runtime_error("Is a directory instead of file: " + file_name);
    }
    auto file_id = fopen(file_name);
    auto offset = open_files[file_id].offset;
    fseek(file_id, 0);

    char buffer[inode->file_size];
    fread(file_id, buffer, inode->file_size);

    fseek(file_id, offset);
    fclose(file_id);
    return {buffer, inode->file_size};
}

