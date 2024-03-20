#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace COMMON {
    void get_terminal_size(size_t *columns, size_t *rows) {
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        *columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#else
        struct winsize w{};
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        *columns = w.ws_col;
        *rows = w.ws_row;
#endif
    }

    std::string formatBytes(size_t bytes) {
        const char* units[] = { "B", "KB", "MB", "GB", "TB" };
        int unitIndex = 0;
        auto displaySize = (double)bytes;
        while (displaySize >= 1024 && unitIndex < 4) {
            displaySize /= 1024.0;
            ++unitIndex;
        }
        std::stringstream stream;
        stream << std::fixed << std::setprecision(2) << displaySize << " " << units[unitIndex];
        return stream.str();
    }

}