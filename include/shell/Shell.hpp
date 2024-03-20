#pragma once

#include <iostream>
#include <string>
#include <map>
#include <functional>
#include <iomanip>
#include "fs/FileSystem.hpp"

class Shell {
private:
    // ANSI颜色代码
    const std::string reset = "\033[0m";
    const std::string blue = "\033[1;34m";  // 加粗蓝色
    const std::string green = "\033[1;32m"; // 加粗绿色
    const std::string red = "\033[1;31m";   // 加粗红色
private:
    FileSystem fs;

    // 命令结构，包括处理函数、介绍和使用方法
    struct Command {
        std::function<void(std::vector<std::string>)> action;
        std::string description;
        std::string usage;
    };

    FileSystem::ProgressCallback callback;

public:
    Shell();

    void run();

private:
    std::map<std::string, Command> commands;
    bool is_active = true;

    void process_command(const std::string& input);

    void help();

    void exit_shell();

    void format();

    void ls();

    // std::string message, int count
    static void echo(const std::vector<std::string>& args);


    void mkdir(const std::vector<std::string> &vector);

    void cd(const std::vector<std::string> &vector);

    void init();

    void touch(const std::vector<std::string> &vector);

    void rm(const std::vector<std::string> &vector);

    void fopen(const std::vector<std::string> &vector);

    void fclose(const std::vector<std::string> &vector);

    void fseek(const std::vector<std::string> &vector);

    void fwrite(const std::vector<std::string> &vector);

    void cat(const std::vector<std::string> &vector);

    void flist();

    void upload(const std::vector<std::string> &vector);

    void download(const std::vector<std::string> &vector);
};
