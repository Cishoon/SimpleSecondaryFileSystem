//
// Created by Cishoon on 2024/3/12.
//

#include "shell/Shell.hpp"
#include "common/common.hpp"

Shell::Shell() {
    commands["help"] = {[this](const std::vector<std::string> &args = {}) { this->help(); },
                        "Display available commands",
                        "help"};
    commands["exit"] = {[this](const std::vector<std::string> &args = {}) { this->exit_shell(); },
                        "Exit the shell",
                        "exit"};
    commands["format"] = {[this](const std::vector<std::string> &args = {}) { this->format(); },
                          "Format the disk",
                          "format"};
    commands["ls"] = {[this](const std::vector<std::string> &args = {}) { this->ls(); },
                      "List directory contents",
                      "ls"};
    commands["pwd"] = {[this](const std::vector<std::string> &args = {}) { std::cout << fs.pwd() << std::endl; },
                       "Print working directory",
                       "pwd"};
    commands["echo"] = {[](const std::vector<std::string> &args) { Shell::echo(args); },
                        "Print the message to the console",
                        "echo <message> [count]"};
    commands["mkdir"] = {[this](const std::vector<std::string> &args) { this->mkdir(args); },
                         "Create a new directory",
                         "mkdir <dir_name>"};
    commands["cd"] = {[this](const std::vector<std::string> &args = {}) { this->cd(args); },
                      "Change the current directory",
                      "cd <dir>"};
    commands["init"] = {[this](const std::vector<std::string> &args = {}) { this->init(); },
                        "Initialize the file system (create root directory and root inode)",
                        "init"};
    commands["touch"] = {[this](const std::vector<std::string> &args) { this->touch(args); },
                         "Create a new file",
                         "touch <file_name>"};
    commands["rm"] = {[this](const std::vector<std::string> &args) { this->rm(args); },
                      "Remove a file or directory",
                      "rm <file_name>"};
    commands["save"] = {[this](const std::vector<std::string> &args = {}) { fs.save(); },
                        "Save the file system to disk",
                        "save"};
    commands["fopen"] = {[this](const std::vector<std::string> &args) { this->fopen(args); },
                         "Open a file",
                         "fopen <file_name>"};
    commands["fclose"] = {[this](const std::vector<std::string> &args) { this->fclose(args); },
                          "Close a file",
                          "fclose <file_id>"};
    commands["fseek"] = {[this](const std::vector<std::string> &args) {this->fseek(args);},
                         "Move the file pointer",
                         "fseek <file_id> <offset>"};
    commands["fwrite"] = {[this](const std::vector<std::string> &args) {this->fwrite(args);},
                            "Write something to a file multiple times",
                            "fwrite <file_id> <data> [times]"};
    commands["cat"] = {[this](const std::vector<std::string> &args) {this->cat(args);},
                            "Read the content of a file",
                            "cat <file_name>"};
    commands["flist"]= {[this](const std::vector<std::string> &args) {this->flist();},
                            "List all opened files",
                            "flist"};
    commands["upload"] = {[this](const std::vector<std::string> &args) {this->upload(args);},
                            "Upload a real_file to the file system",
                            "upload <path_in_system> <real_file_path>"};
    commands["download"] = {[this](const std::vector<std::string> &args) {this->download(args);},
                            "Download a real_file from the file system",
                            "download <path_in_system> <real_file_path>"};

}

void Shell::run() {
    std::string input;
    while (is_active) {
        std::string current_dir = fs.get_current_dir();
        current_dir = current_dir == "root" ? "~" : current_dir;
        std::cout << "[" << current_dir << "]# "; // 显示提示符
        std::getline(std::cin, input); // 获取用户输入
        process_command(input); // 处理命令
    }
}

void Shell::process_command(const std::string &input) {
    std::istringstream iss(input);
    std::string command;
    std::vector<std::string> args;

    iss >> command; // 读取命令
    std::string arg;
    while (iss >> arg) { // 读取所有参数
        args.push_back(arg);
    }

    auto it = commands.find(command);
    if (it != commands.end()) {
        try {
            it->second.action(args); // 使用参数列表执行命令
        } catch (const std::exception &e) {
            // std::cerr << "Error: " << e.what() << std::endl;
            std::cout << red << "Error: " << e.what() << reset << std::endl;
        }
    } else {
        std::cout << "Unknown command: " << command << std::endl;
    }
}

void Shell::help() {
    std::cout << "Available commands:\n";
    for (const auto &cmd: commands) {
        // 命令名以蓝色显示
        std::cout << "- " << blue << cmd.first << reset;
        // 介绍文本以默认颜色显示
        std::cout << ": " << cmd.second.description << "\n";
        // 使用方法以绿色显示
        std::cout << "  Usage: " << green << cmd.second.usage << reset << std::endl;
    }
}


void Shell::exit_shell() {
    is_active = false;
}

void Shell::format() {
    std::cout << "Formatting disk..." << std::endl;
    fs.format();
    std::cout << "Disk formatted." << std::endl;
}

void Shell::ls() {
    auto entries = fs.ls();
    size_t terminal_width, terminal_height;
    COMMON::get_terminal_size(&terminal_width, &terminal_height);

    // 找到最长的文件名
    int max_length = 0;
    for (const auto &entry: entries) {
        max_length = std::max(max_length, (int) entry.size());
    }

    // 确保有空间至少放置一个文件名加上间隔
    max_length += 2; // 假设两个空格作为间隔

    // 计算每行的文件名数量
    auto entries_per_line = terminal_width / max_length;

    int count = 0;
    for (const auto &entry: entries) {
        std::cout << std::left << std::setw(max_length) << entry;
        if (++count % entries_per_line == 0)
            std::cout << std::endl;
    }
    // 如果最后一行没有填满，也需要换行
    if (count % entries_per_line != 0)
        std::cout << std::endl;
}

void Shell::echo(const std::vector<std::string> &args) {
    // std::string message, int count
    std::string message;
    int count;
    if (args.empty()) {
        std::cout << "Usage: echo <message> [count]" << std::endl;
        return;
    } else if (args.size() == 1) {
        message = args[0];
        count = 1;
    } else {
        message = args[0];
        count = std::stoi(args[1]);
    }

    for (int i = 0; i < count; ++i) {
        std::cout << message << std::endl;
    }
}

void Shell::mkdir(const std::vector<std::string> &args) {
    if (args.empty()) {
        std::cout << "Usage: mkdir <dir_name>" << std::endl;
        return;
    }
    fs.mkdir(args[0]);
}

void Shell::cd(const std::vector<std::string> &vector) {
    if (vector.empty()) {
        std::cout << "Usage: cd <dir>" << std::endl;
        return;
    }
    fs.cd(vector[0]);
}

void Shell::init() {
    fs.init();
}

void Shell::touch(const std::vector<std::string> &vector) {
    if (vector.empty()) {
        std::cout << "Usage: touch <file_name>" << std::endl;
        return;
    }
    fs.touch(vector[0]);
}

void Shell::rm(const std::vector<std::string> &vector) {
    if (vector.empty()) {
        std::cout << "Usage: rm <file_name>" << std::endl;
        return;
    }
    fs.rm(vector[0]);
}

void Shell::fopen(const std::vector<std::string> &vector) {
    if (vector.empty()) {
        std::cout << "Usage: fopen <file_name>" << std::endl;
        return;
    }
    auto fd = fs.fopen(vector[0]);
    std::cout << "[" << blue << fd << reset << "]" << std::endl;
}

void Shell::fclose(const std::vector<std::string> &vector) {
    if (vector.empty()) {
        std::cout << "Usage: fclose <file_id>" << std::endl;
        return;
    }
    uint32_t fd;
    try{
        fd = std::stoi(vector[0]);
    } catch (...) {
        throw std::runtime_error("Invalid file id");
    }
    fs.fclose(fd);
}

void Shell::fseek(const std::vector<std::string> &vector) {
    if (vector.size() < 2) {
        std::cout << "Usage: fseek <file_id> <offset>" << std::endl;
        return;
    }
    uint32_t fd, offset;
    try{
        fd = std::stoi(vector[0]);
        offset = std::stoi(vector[1]);
    } catch (...) {
        throw std::runtime_error("Invalid file id or offset");
    }
    fs.fseek(fd, offset);
}

void Shell::fwrite(const std::vector<std::string> &vector) {
    if (vector.size() < 2) {
        std::cout << "Usage: fwrite <file_id> <data> [times]" << std::endl;
        return;
    }
    uint32_t fd, times;
    try{
        fd = std::stoi(vector[0]);
        times = vector.size() == 3 ? std::stoi(vector[2]) : 1;
    } catch (...) {
        throw std::runtime_error("Invalid file id or size");
    }
    // fs.fwrite(fd, vector[1].c_str(), size);
    // 写入的数据是times个vector[1]
    std::stringstream ss;
    for (int i = 0; i < times; i++) {
        ss << vector[1];
    }
    fs.fwrite(fd, ss.str().c_str(), ss.str().size());
}

void Shell::cat(const std::vector<std::string> &vector) {
    if (vector.empty()) {
        std::cout << "Usage: cat <file_name>" << std::endl;
        return;
    }
    std::string content = fs.cat(vector[0]);
    std::cout << content << std::endl;
}

void Shell::flist() {
    auto files = fs.flist();
    for (const auto &file: files) {
        std::cout << "[" << blue << file.first << reset << "] " << file.second << std::endl;
    }
}

void Shell::upload(const std::vector<std::string> &vector) {
    if (vector.size() < 2) {
        std::cout << "Usage: upload <path_in_system> <real_file_path>" << std::endl;
        return;
    }
    const std::string& path_in_system = vector[0];
    const std::string& real_file_path = vector[1];
    // 使用fopen, fwrite, fclose
    if (fs.exist(path_in_system)) {
        throw std::runtime_error("File already exists: " + path_in_system);
    }
    fs.touch(path_in_system);
    uint32_t fd = fs.fopen(path_in_system);
    std::ifstream file(real_file_path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + real_file_path);
    }
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    file.read(buffer.data(), size);
    fs.fwrite(fd, buffer.data(), size);
    fs.fclose(fd);
}

void Shell::download(const std::vector<std::string> &vector) {
    if (vector.size() < 2) {
        std::cout << "Usage: download <path_in_system> <real_file_path>" << std::endl;
        return;
    }
    const std::string& path_in_system = vector[0];
    const std::string& real_file_path = vector[1];
    // 使用fopen, fread, fclose
    if (!fs.exist(path_in_system)) {
        throw std::runtime_error("File not found: " + path_in_system);
    }
    uint32_t fd = fs.fopen(path_in_system);
    std::ofstream file(real_file_path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + real_file_path);
    }
    auto size = fs.get_file_size(fd);
    std::vector<char> buffer(size);
    fs.fread(fd, buffer.data(), size);
    file.write(buffer.data(), size);
    fs.fclose(fd);
}
