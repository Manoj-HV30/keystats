#include <iostream>
#include <unordered_map>
#include <string>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <fcntl.h>
#include <unistd.h>
#include <sqlite3.h>



std::unordered_map<int, std::string> keyNames = {
    {KEY_ESC, "Esc"}, {KEY_F1, "F1"}, {KEY_F2, "F2"}, {KEY_F3, "F3"},
    {KEY_F4, "F4"}, {KEY_F5, "F5"}, {KEY_F6, "F6"}, {KEY_F7, "F7"},
    {KEY_F8, "F8"}, {KEY_F9, "F9"}, {KEY_F10, "F10"}, {KEY_F11, "F11"},
    {KEY_F12, "F12"},
    {KEY_GRAVE, "`"}, {KEY_1, "1"}, {KEY_2, "2"}, {KEY_3, "3"},
    {KEY_4, "4"}, {KEY_5, "5"}, {KEY_6, "6"}, {KEY_7, "7"},
    {KEY_8, "8"}, {KEY_9, "9"}, {KEY_0, "0"}, {KEY_MINUS, "-"},
    {KEY_EQUAL, "="}, {KEY_BACKSPACE, "Backspace"},
    {KEY_TAB, "Tab"}, {KEY_Q, "Q"}, {KEY_W, "W"}, {KEY_E, "E"},
    {KEY_R, "R"}, {KEY_T, "T"}, {KEY_Y, "Y"}, {KEY_U, "U"},
    {KEY_I, "I"}, {KEY_O, "O"}, {KEY_P, "P"}, {KEY_LEFTBRACE, "["},
    {KEY_RIGHTBRACE, "]"}, {KEY_BACKSLASH, "\\"},
    {KEY_CAPSLOCK, "CapsLock"}, {KEY_A, "A"}, {KEY_S, "S"}, {KEY_D, "D"},
    {KEY_F, "F"}, {KEY_G, "G"}, {KEY_H, "H"}, {KEY_J, "J"},
    {KEY_K, "K"}, {KEY_L, "L"}, {KEY_SEMICOLON, ";"}, {KEY_APOSTROPHE, "'"},
    {KEY_ENTER, "Enter"},
    {KEY_LEFTSHIFT, "LShift"}, {KEY_Z, "Z"}, {KEY_X, "X"}, {KEY_C, "C"},
    {KEY_V, "V"}, {KEY_B, "B"}, {KEY_N, "N"}, {KEY_M, "M"},
    {KEY_COMMA, ","}, {KEY_DOT, "."}, {KEY_SLASH, "/"}, {KEY_RIGHTSHIFT, "RShift"},
    {KEY_LEFTCTRL, "LCtrl"}, {KEY_LEFTMETA, "Super"}, {KEY_LEFTALT, "LAlt"},
    {KEY_SPACE, "Space"}, {KEY_RIGHTALT, "RAlt"}, {KEY_RIGHTMETA, "RSuper"},
    {KEY_RIGHTCTRL, "RCtrl"},
    {KEY_INSERT, "Insert"}, {KEY_DELETE, "Delete"}, {KEY_HOME, "Home"},
    {KEY_END, "End"}, {KEY_PAGEUP, "PgUp"}, {KEY_PAGEDOWN, "PgDn"},
    {KEY_UP, "Up"}, {KEY_DOWN, "Down"}, {KEY_LEFT, "Left"}, {KEY_RIGHT, "Right"},
    {KEY_NUMLOCK, "NumLock"}, {KEY_KPSLASH, "KP/"}, {KEY_KPASTERISK, "KP*"},
    {KEY_KPMINUS, "KP-"}, {KEY_KP7, "KP7"}, {KEY_KP8, "KP8"},
    {KEY_KP9, "KP9"}, {KEY_KPPLUS, "KP+"}, {KEY_KP4, "KP4"},
    {KEY_KP5, "KP5"}, {KEY_KP6, "KP6"}, {KEY_KP1, "KP1"},
    {KEY_KP2, "KP2"}, {KEY_KP3, "KP3"}, {KEY_KP0, "KP0"},
    {KEY_KPDOT, "KP."}, {KEY_KPENTER, "KPEnter"},
};

std::unordered_map<int, long long> keyCounts;
sqlite3* db = nullptr;
int      fd = -1;



void initDB() {
    struct passwd* pw = getpwuid(getuid());
    std::string dbPath = std::string(pw->pw_dir) + "/.keystroke_counts.db";

    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Failed to open DB: " << sqlite3_errmsg(db) << "\n";
        exit(1);
    }

    const char* sql =
        "CREATE TABLE IF NOT EXISTS key_counts ("
        "  key_code  INTEGER PRIMARY KEY,"
        "  key_name  TEXT,"
        "  count     INTEGER DEFAULT 0"
        ");";
    sqlite3_exec(db, sql, nullptr, nullptr, nullptr);

    const char* sql2 =
    "CREATE TABLE IF NOT EXISTS daily_counts ("
    "  date      TEXT,"
    "  key_code  INTEGER,"
    "  key_name  TEXT,"
    "  count     INTEGER DEFAULT 0,"
    "  PRIMARY KEY (date, key_code)"
    ");";
sqlite3_exec(db, sql2, nullptr, nullptr, nullptr);
}

void loadFromDB() {
    const char* sql = "SELECT key_code, count FROM key_counts;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int       code = sqlite3_column_int(stmt, 0);
        long long cnt  = sqlite3_column_int64(stmt, 1);
        keyCounts[code] = cnt;
    }
    sqlite3_finalize(stmt);
}

void flushToDB() {
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    const char* sql =
        "INSERT INTO key_counts (key_code, key_name, count) VALUES (?, ?, ?)"
        " ON CONFLICT(key_code) DO UPDATE SET count = excluded.count;";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    for (auto& [code, count] : keyCounts) {
        std::string name = keyNames.count(code)
            ? keyNames[code]
            : "Unknown(" + std::to_string(code) + ")";

        sqlite3_bind_int(stmt,   1, code);
        sqlite3_bind_text(stmt,  2, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, count);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
}



std::string getToday() {
    time_t t = time(nullptr);
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", localtime(&t));
    return std::string(buf);
}

void flushDailyToDB(int code, long long count) {
    std::string date = getToday();
    std::string name = keyNames.count(code)
        ? keyNames[code]
        : "Unknown(" + std::to_string(code) + ")";

    const char* sql =
        "INSERT INTO daily_counts (date, key_code, key_name, count) VALUES (?, ?, ?, 1)"
        " ON CONFLICT(date, key_code) DO UPDATE SET count = count + 1;";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  2, code);
    sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}


std::string findKeyboard() {
    DIR* dir = opendir("/dev/input");
    if (!dir) return "";

    struct dirent* entry;
    std::string found;

    while ((entry = readdir(dir)) != nullptr) {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;

        std::string path = "/dev/input/" + std::string(entry->d_name);
        int tempFd = open(path.c_str(), O_RDONLY);
        if (tempFd < 0) continue;

        char name[256] = {0};
        ioctl(tempFd, EVIOCGNAME(sizeof(name)), name);
        std::string devName(name);
        for (auto& c : devName) c = tolower(c);

        uint8_t evBits[EV_MAX / 8 + 1]   = {0};
        uint8_t keyBits[KEY_MAX / 8 + 1] = {0};
        ioctl(tempFd, EVIOCGBIT(0, sizeof(evBits)), evBits);
        ioctl(tempFd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits);

        bool hasEvKey   = evBits[EV_KEY / 8]     & (1 << (EV_KEY    % 8));
        bool hasSpace   = keyBits[KEY_SPACE / 8]  & (1 << (KEY_SPACE % 8));
        bool isKeyboard = devName.find("keyboard") != std::string::npos;

        close(tempFd);

        if (hasEvKey && hasSpace && isKeyboard) {
            found = path;
            break;
        }
    }

    closedir(dir);
    return found;
}



void onExit(int) {
    flushToDB();
    if (db) sqlite3_close(db);
    if (fd >= 0) close(fd);
    exit(0);
}



int main() {
    signal(SIGINT,  onExit);
    signal(SIGTERM, onExit);

    initDB();
    loadFromDB();

    std::string device = findKeyboard();
    if (device.empty()) {
        std::cerr << "No keyboard found. Are you in the 'input' group?\n";
        return 1;
    }

    fd = open(device.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "Failed to open device.\n";
        return 1;
    }

    struct input_event ev;

    while (true) {
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n != sizeof(ev)) break;

        if (ev.type == EV_KEY && ev.value == 1) {
            keyCounts[ev.code]++;
            flushToDB();
            flushDailyToDB(ev.code, keyCounts[ev.code]);
        }
    }

    onExit(0);
    return 0;
}
