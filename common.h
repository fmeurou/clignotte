#pragma once

#include <QTextStream>

extern QTextStream out;

extern const char *IMPORTANT_TEXT;
extern const char *URGENT_TEXT;
extern const char *BOLD_TEXT;
extern const char *ITALIC_TEXT;
extern const char *END_BOLD_TEXT;
extern const char *INVERTED_TEXT;
extern const char *END_INVERTED_TEXT;
extern const char *NORMAL_TEXT;
extern const char *UNDERLINED_TEXT;
extern const char *BLINK_TEXT;

int terminalCols();
