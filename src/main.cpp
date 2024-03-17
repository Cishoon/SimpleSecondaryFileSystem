#include "shell/Shell.hpp"


int main() {
    // 增加一个功能，如果程序被异常中断，接住异常并打印异常信息
    try {
        Shell shell;
        shell.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown error" << std::endl;
    }

    return 0;
}
