#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <chrono>
#include <ctime>
#include <sqlite3.h>
#include <pwd.h>
#include <unistd.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>

using namespace ftxui;

struct KeyStat {
    std::string name;
    long long   count;
};

std::string getDBPath() {
    struct passwd* pw = getpwuid(getuid());
    return std::string(pw->pw_dir) + "/.keystroke_counts.db";
}

std::vector<KeyStat> lastGoodStats;

std::vector<KeyStat> loadStats() {
    sqlite3* db;
    if (sqlite3_open(getDBPath().c_str(), &db) != SQLITE_OK) return lastGoodStats;

    sqlite3_busy_timeout(db, 100);

    const char* sql = "SELECT key_name, count FROM key_counts ORDER BY count DESC;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return lastGoodStats;
    }

    std::vector<KeyStat> stats;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string name  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        long long   count = sqlite3_column_int64(stmt, 1);
        stats.push_back({name, count});
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (!stats.empty()) lastGoodStats = stats;
    return lastGoodStats;
}

std::vector<KeyStat> loadTodayStats() {
    sqlite3* db;
    if (sqlite3_open(getDBPath().c_str(), &db) != SQLITE_OK) return {};

    sqlite3_busy_timeout(db, 100);

    time_t t = time(nullptr);
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", localtime(&t));
    std::string today(buf);

    const char* sql =
        "SELECT key_name, count FROM daily_counts"
        " WHERE date = ? ORDER BY count DESC;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return {};
    }

    sqlite3_bind_text(stmt, 1, today.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<KeyStat> stats;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string name  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        long long   count = sqlite3_column_int64(stmt, 1);
        stats.push_back({name, count});
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return stats;
}

long long totalCount(const std::vector<KeyStat>& stats) {
    long long total = 0;
    for (auto& s : stats) total += s.count;
    return total;
}

const std::vector<std::vector<std::string>> keyboardLayout = {
    {"`","1","2","3","4","5","6","7","8","9","0","-","=","Backspace"},
    {"Tab","Q","W","E","R","T","Y","U","I","O","P","[","]","\\"},
    {"CapsLock","A","S","D","F","G","H","J","K","L",";","'","Enter"},
    {"LShift","Z","X","C","V","B","N","M",",",".","/","RShift"},
    {"LCtrl","Super","LAlt","Space","RAlt","RCtrl"},
};

Color heatColor(long long count, long long maxCount) {
    if (maxCount == 0 || count == 0) return Color::GrayDark;
    float ratio = (float)count / (float)maxCount;
    if (ratio > 0.75f) return Color::Red;
    if (ratio > 0.50f) return Color::Yellow;
    if (ratio > 0.25f) return Color::Green;
    return Color::Blue;
}

Element renderStats(const std::vector<KeyStat>& stats, const std::vector<KeyStat>& todayStats) {
    if (stats.empty()) {
        return text("No data yet. Press some keys first.") | color(Color::Red);
    }

    std::unordered_map<std::string, long long> countMap;
    for (auto& s : stats) countMap[s.name] = s.count;

    long long maxCount = stats[0].count;
    long long total    = totalCount(stats);

    // ── Heatmap ──────────────────────────────────────────────────────────────
    std::vector<Element> rows;
    for (auto& row : keyboardLayout) {
        std::vector<Element> keys;
        for (auto& key : row) {
            long long count = countMap.count(key) ? countMap[key] : 0;
            Color col = heatColor(count, maxCount);
            keys.push_back(text(" " + key + " ") | color(col) | border);
        }
        rows.push_back(hbox(keys));
    }

    // ── Lifetime top 10 ──────────────────────────────────────────────────────
    std::vector<Element> topList;
    int limit = std::min((int)stats.size(), 10);
    for (int i = 0; i < limit; i++) {
        auto& s = stats[i];
        float pct = 100.0f * s.count / total;
        topList.push_back(
            hbox({
                text(std::to_string(i+1) + ". ") | color(Color::GrayLight),
                text(s.name)                      | color(Color::White) | flex,
                text(std::to_string(s.count))     | color(Color::Yellow),
                text(" (" + std::to_string((int)pct) + "%)") | color(Color::GrayLight),
            })
        );
    }

    // ── Today top 5 ──────────────────────────────────────────────────────────
    long long todayTotal = totalCount(todayStats);
    std::vector<Element> todayList;
    int todayLimit = std::min((int)todayStats.size(), 5);
    for (int i = 0; i < todayLimit; i++) {
        auto& s = todayStats[i];
        float pct = todayTotal > 0 ? 100.0f * s.count / todayTotal : 0;
        todayList.push_back(
            hbox({
                text(std::to_string(i+1) + ". ") | color(Color::GrayLight),
                text(s.name)                      | color(Color::White) | flex,
                text(std::to_string(s.count))     | color(Color::Yellow),
                text(" (" + std::to_string((int)pct) + "%)") | color(Color::GrayLight),
            })
        );
    }
    if (todayList.empty()) {
        todayList.push_back(text("No keys pressed today yet.") | color(Color::GrayDark));
    }

    // ── Legend ───────────────────────────────────────────────────────────────
    auto legend = hbox({
        text(" cold ") | color(Color::GrayDark),
        text("▓") | color(Color::Blue),
        text("▓") | color(Color::Green),
        text("▓") | color(Color::Yellow),
        text("▓") | color(Color::Red),
        text(" hot ")  | color(Color::GrayLight),
    });

    return vbox({
        text(" Keystroke Counter ") | bold | color(Color::Green) | hcenter,
        separator(),
        vbox(rows) | hcenter,
        legend | hcenter,
        separator(),
        hbox({
            vbox({
                text(" Lifetime Top 10 ") | bold | color(Color::White),
                separator(),
                vbox(topList),
            }) | border | flex,
            vbox({
                text(" Today Top 5 ") | bold | color(Color::Green),
                separator(),
                vbox(todayList),
                separator(),
                text(" Today: " + std::to_string(todayTotal)) | color(Color::Cyan),
            }) | border | flex,
            vbox({
                text(" Total keystrokes ") | bold,
                text(" " + std::to_string(total)) | color(Color::Cyan) | bold,
                separator(),
                text(" q to quit ") | color(Color::GrayDark),
            }) | border,
        }),
    });
}

int main() {
    auto screen = ScreenInteractive::Fullscreen();

    std::atomic<bool> running = true;
    std::thread refreshThread([&] {
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            screen.PostEvent(Event::Custom);
        }
    });

    auto renderer = Renderer([&] {
        return renderStats(loadStats(), loadTodayStats());
    });

    auto withQuit = CatchEvent(renderer, [&](Event e) {
        if (e == Event::Character('q')) {
            running = false;
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    screen.Loop(withQuit);
    refreshThread.join();
    return 0;
}
