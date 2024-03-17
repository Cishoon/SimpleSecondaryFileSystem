//
// Created by Cishoon on 2024/3/12.
//

#include "shell/Shell.hpp"
#include "common/common.hpp"

Shell::Shell() {
    commands["help"] = { [this](const std::vector<std::string>& args = {}) { this->help(); }, "Display available commands", "help" };
    commands["exit"] = { [this](const std::vector<std::string>& args = {}) { this->exit_shell(); }, "Exit the shell", "exit" };
    commands["format"] = { [this](const std::vector<std::string>& args = {}) { this->format(); }, "Format the disk", "format" };
    commands["ls"] = { [this](const std::vector<std::string>& args = {}) { this->ls(); }, "List directory contents", "ls" };
    commands["pwd"] = { [this](const std::vector<std::string>& args = {}) { std::cout << fs.pwd() << std::endl; }, "Print working directory", "pwd" };
    commands["echo"] = { [](const std::vector<std::string>& args) { Shell::echo(args); }, "Print the message to the console", "echo <message> [count]" };
    commands["mkdir"] = { [this](const std::vector<std::string>& args) {this->mkdir(args);}, "Create a new directory", "mkdir <dir_name>"};
}

void Shell::run() {
    std::string input;
    while (is_active) {
        std::string current_dir = fs.get_current_dir();
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
        try{
            it->second.action(args); // 使用参数列表执行命令
        } catch (const std::exception& e) {
            // std::cerr << "Error: " << e.what() << std::endl;
            std::cout << red << "Error: " << e.what() << reset << std::endl;
        }
    } else {
        std::cout << "Unknown command: " << command << std::endl;
    }
}

void Shell::help() {
    std::cout << "Available commands:\n";
    for (const auto& cmd : commands) {
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
    for (const auto& entry : entries) {
        max_length = std::max(max_length, (int)entry.size());
    }

    // 确保有空间至少放置一个文件名加上间隔
    max_length += 2; // 假设两个空格作为间隔

    // 计算每行的文件名数量
    auto entries_per_line = terminal_width / max_length;

    int count = 0;
    for (const auto& entry : entries) {
        std::cout << std::left << std::setw(max_length) << entry;
        if (++count % entries_per_line == 0)
            std::cout << std::endl;
    }
    // 如果最后一行没有填满，也需要换行
    if (count % entries_per_line != 0)
        std::cout << std::endl;
}

void Shell::echo(const std::vector<std::string>& args) {
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
