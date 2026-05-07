#include "common.h"

#include <QByteArray>
#include <cstdlib>
#include <sys/ioctl.h>
#include <unistd.h>

static bool initColorEnabled()    {
    const char *nc = std::getenv("NO_COLOR");
    if(nc && nc[0] != '\0') return false;
    return isatty(STDOUT_FILENO) != 0;
}
static const bool g_color = initColorEnabled();

const char *IMPORTANT_TEXT     = g_color ? "\e[1;31m"  : "";
const char *URGENT_TEXT        = g_color ? "\e[7;31m"  : "";
const char *BOLD_TEXT          = g_color ? "\e[1m"     : "";
const char *ITALIC_TEXT        = g_color ? "\e[3m"     : "";
const char *END_BOLD_TEXT      = g_color ? "\e[21m"    : "";
const char *INVERTED_TEXT      = g_color ? "\e[7m"     : "";
const char *END_INVERTED_TEXT  = g_color ? "\e[27m"    : "";
const char *NORMAL_TEXT        = g_color ? "\e[0m"     : "";
const char *UNDERLINED_TEXT    = g_color ? "\e[4m"     : "";
const char *BLINK_TEXT         = g_color ? "\e[5;33m"  : "";

QTextStream out(stdout);

int terminalCols()   {
    struct winsize w;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)   {
        return w.ws_col;
    }
    bool ok = false;
    int envCols = qgetenv("COLUMNS").toInt(&ok);
    if(ok && envCols > 0) return envCols;
    return 80;
}
