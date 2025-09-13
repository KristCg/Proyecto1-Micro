#include <iostream>
#include <string>
#include <vector>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

using namespace std;

constexpr int SCREEN_W = 100;
constexpr int SCREEN_H = 36;

constexpr int CURRENT_SCORE = 1200;
constexpr int HIGH_SCORE = 5000;
constexpr int NEXT_1UP_AT = 10000;
constexpr int LIVES = 3;
constexpr int LEVEL = 3;

constexpr int HORDE_ROWS = 3;
constexpr int BEE_COLS = 8;
constexpr int COL_GAP = 3;
constexpr int HORDE_GAP = 2;
static bool USE_COLOR = true;

#ifdef _WIN32
static void tryEnableWindowsAnsi() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        USE_COLOR = false;
        return;
    }
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) {
        USE_COLOR = false;
        return;
    }
    if ((dwMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0) {
        if (!SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            USE_COLOR = false;
        }
    }
}
#endif

static inline string fg(int r, int g, int b) {
    return USE_COLOR ? ("\x1b[38;2;" + to_string(r) + ";" + to_string(g) + ";" + to_string(b) + "m") : "";
}

static inline string resetCode() { return USE_COLOR ? "\x1b[0m" : ""; }

static const vector<string> BEE = {
    " /\\_/\\ ",
    "(=o o=)",
    " \\_-_/ "
};

static const vector<string> PLAYER = {
    "   ^   ",
    "  /|\\  ",
    " /_|_\\ ",
    "   |   "
};

static int interiorWidth() { return max(0, SCREEN_W - 2); }
static int interiorHeight() { return max(0, SCREEN_H - 2); }

static void printBorderTop() {
    cout << fg(0, 180, 255) << "+" << string(interiorWidth(), '-') << "+" << resetCode() << "\n";
}

static void printBorderBottom() {
    cout << fg(0, 180, 255) << "+" << string(interiorWidth(), '-') << "+" << resetCode() << "\n";
}

static void printLine(const string &content, const string &color = "") {
    const int W = interiorWidth();
    string s = content;
    if ((int) s.size() > W) s = s.substr(0, W);
    if ((int) s.size() < W) s += string(W - (int) s.size(), ' ');
    cout << (color.empty() ? "" : color) << "|" << s << "|" << (color.empty() ? "" : resetCode()) << "\n";
}

static void printBlank(int n, const string &color = "") {
    for (int i = 0; i < n; ++i) printLine("", color);
}

static void printCentered(const string &content, const string &color = "") {
    const int W = interiorWidth();
    int left = max(0, (W - (int) content.size()) / 2);
    printLine(string(left, ' ') + content, color);
}

static string buildBeeRowLine(const string &spriteLine, int cols, int colGap) {
    string row;
    for (int c = 0; c < cols; ++c) {
        row += spriteLine;
        if (c < cols - 1) row += string(colGap, ' ');
    }
    return row;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

#ifdef _WIN32
    if (USE_COLOR) tryEnableWindowsAnsi();
#endif

    cout << (USE_COLOR ? "\x1b[2J\x1b[H" : "\n");

    printBorderTop();

    int remainder = CURRENT_SCORE % NEXT_1UP_AT;
    int toNext = (remainder == 0 && CURRENT_SCORE > 0) ? NEXT_1UP_AT : (NEXT_1UP_AT - remainder);

    char buf[256];
    snprintf(buf, sizeof(buf),
             "HI-SCORE: %06d   SCORE: %06d   NEXT 1-UP IN: %d   LIVES: [", HIGH_SCORE, CURRENT_SCORE, toNext);
    string hud = buf;
    for (int i = 0; i < LIVES; ++i) {
        hud += "*";
        if (i < LIVES - 1) hud += " ";
    }
    hud += "]   LEVEL: ";
    hud += (LEVEL < 10 ? ("0" + to_string(LEVEL)) : to_string(LEVEL));

    printLine(hud, fg(144, 238, 144));
    printLine(string(interiorWidth(), '-'), fg(100, 100, 100));

    for (int row = 0; row < HORDE_ROWS; ++row) {
        for (const auto &line: BEE) {
            printCentered(buildBeeRowLine(line, BEE_COLS, COL_GAP), fg(255, 215, 0));
        }
        if (row < HORDE_ROWS - 1) printBlank(HORDE_GAP);
    }

    const int usedSoFar = 1 + 1 + HORDE_ROWS * (int) BEE.size() + (HORDE_ROWS - 1) * HORDE_GAP;
    const int H = interiorHeight();
    const int minBelow = 1;
    int remaining = max(0, H - usedSoFar - (int) PLAYER.size() - minBelow);
    printBlank(remaining);

    for (const auto &line: PLAYER) printCentered(line, fg(200, 200, 255));
    printBlank(minBelow);

    printBorderBottom();

    if (USE_COLOR) cout << resetCode();
    return 0;
}
