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

}