#include "palmyraOS/palmyraSDK.h"
#include "palmyraOS/unistd.h"
#include <algorithm>

namespace PalmyraOS::Userland::builtin::taskManager {

    // Process information structure
    struct ProcessInfo {
        uint32_t pid;
        char name[64];
        char state;
        uint64_t cpuTicks;
        uint32_t rssPages;
    };

    // Sort column enumeration
    enum class SortColumn { PID, NAME, CPU, MEMORY, STATE };

    // Refresh rate options
    enum class RefreshRate { OFF, ONE_SEC, TWO_SEC, THREE_SEC, FIVE_SEC, TEN_SEC };

    // Convert refresh rate to frame count
    uint32_t getRefreshFrames(RefreshRate rate) {
        switch (rate) {
            case RefreshRate::OFF: return 0;          // No auto-refresh
            case RefreshRate::ONE_SEC: return 40;     // ~1 second
            case RefreshRate::TWO_SEC: return 80;     // ~2 seconds
            case RefreshRate::THREE_SEC: return 120;  // ~3 seconds
            case RefreshRate::FIVE_SEC: return 200;   // ~5 seconds
            case RefreshRate::TEN_SEC: return 400;    // ~10 seconds
            default: return 80;
        }
    }

    // Convert refresh rate to display text
    const char* getRefreshText(RefreshRate rate) {
        switch (rate) {
            case RefreshRate::OFF: return "Refresh: Off";
            case RefreshRate::ONE_SEC: return "Refresh: 1s";
            case RefreshRate::TWO_SEC: return "Refresh: 2s";
            case RefreshRate::THREE_SEC: return "Refresh: 3s";
            case RefreshRate::FIVE_SEC: return "Refresh: 5s";
            case RefreshRate::TEN_SEC: return "Refresh: 10s";
            default: return "Refresh: 2s";
        }
    }

    // Cycle to next refresh rate
    RefreshRate nextRefreshRate(RefreshRate current) {
        switch (current) {
            case RefreshRate::OFF: return RefreshRate::ONE_SEC;
            case RefreshRate::ONE_SEC: return RefreshRate::TWO_SEC;
            case RefreshRate::TWO_SEC: return RefreshRate::THREE_SEC;
            case RefreshRate::THREE_SEC: return RefreshRate::FIVE_SEC;
            case RefreshRate::FIVE_SEC: return RefreshRate::TEN_SEC;
            case RefreshRate::TEN_SEC: return RefreshRate::OFF;
            default: return RefreshRate::TWO_SEC;
        }
    }

    // Parse a single /proc/{pid}/stat file and extract process info
    bool parseProcessStat(uint32_t pid, ProcessInfo& info) {
        char statPath[32];
        snprintf(statPath, sizeof(statPath), "/proc/%u/stat", pid);

        int fd = open(statPath, O_RDONLY);
        if (fd < 0) return false;

        char buffer[512];
        int bytesRead = read(fd, buffer, sizeof(buffer) - 1);
        close(fd);

        if (bytesRead <= 0) return false;
        buffer[bytesRead]   = '\0';

        // Manual parsing: pid (name) state ... utime stime ... rss
        // Format: %d (%s) %c followed by many fields

        const char* p       = buffer;
        uint32_t parsed_pid = 0;

        // Parse pid
        while (*p && *p >= '0' && *p <= '9') {
            parsed_pid = parsed_pid * 10 + (*p - '0');
            p++;
        }

        if (*p != ' ') return false;
        p++;  // Skip space

        if (*p != '(') return false;
        p++;  // Skip '('

        // Parse name until closing ')'
        char parsed_name[64] = {0};
        size_t nameIdx       = 0;
        while (*p && *p != ')' && nameIdx < sizeof(parsed_name) - 1) { parsed_name[nameIdx++] = *p++; }
        parsed_name[nameIdx] = '\0';

        if (*p != ')') return false;
        p++;  // Skip ')'

        if (*p != ' ') return false;
        p++;  // Skip space

        // Parse state (single character)
        char parsed_state = *p++;

        // Skip fields: ppid pgrp session tty tpgid flags minflt cminflt majflt cmajflt
        // That's 10 more fields to skip
        for (int i = 0; i < 10; i++) {
            while (*p && *p != ' ') p++;
            if (*p) p++;  // Skip space
        }

        // Now parse utime
        uint64_t utime = 0;
        while (*p && *p >= '0' && *p <= '9') {
            utime = utime * 10 + (*p - '0');
            p++;
        }
        if (*p) p++;  // Skip space

        // Parse stime
        uint64_t stime = 0;
        while (*p && *p >= '0' && *p <= '9') {
            stime = stime * 10 + (*p - '0');
            p++;
        }

        // Skip more fields to reach rss: cutime cstime priority nice threads itrealvalue starttime vsize
        // That's 8 more fields
        for (int i = 0; i < 8; i++) {
            while (*p && *p != ' ') p++;
            if (*p) p++;  // Skip space
        }

        // Parse rss (final field we care about)
        uint32_t rss = 0;
        while (*p && *p >= '0' && *p <= '9') {
            rss = rss * 10 + (*p - '0');
            p++;
        }

        info.pid = parsed_pid;
        strncpy(info.name, parsed_name, sizeof(info.name) - 1);
        info.name[sizeof(info.name) - 1] = '\0';
        info.state                       = parsed_state;
        info.cpuTicks                    = utime + stime;
        info.rssPages                    = rss;

        return true;
    }

    // List all processes in /proc/
    void collectProcesses(types::UserHeapManager& heap, types::UVector<ProcessInfo>& processes) {
        processes.clear();

        int dirFd = open("/proc", O_RDONLY);
        if (dirFd < 0) return;

        char buffer[4096];
        int bytesRead = getdents((unsigned int) dirFd, (linux_dirent*) buffer, sizeof(buffer));
        close(dirFd);

        if (bytesRead <= 0) return;

        // Parse directory entries
        linux_dirent* dirent = (linux_dirent*) buffer;
        char* endPtr         = buffer + bytesRead;

        while ((char*) dirent < endPtr && dirent->d_reclen != 0) {
            // Check if this is a numeric PID directory
            if (dirent->d_name[0] >= '0' && dirent->d_name[0] <= '9') {
                uint32_t pid = 0;
                for (const char* p = dirent->d_name; *p && *p >= '0' && *p <= '9'; p++) { pid = pid * 10 + (*p - '0'); }

                ProcessInfo info;
                if (parseProcessStat(pid, info)) { processes.push_back(info); }
            }

            dirent = (linux_dirent*) ((char*) dirent + dirent->d_reclen);
        }
    }

    // Sort comparison functions
    bool sortByPID(const ProcessInfo& a, const ProcessInfo& b) { return a.pid < b.pid; }

    bool sortByName(const ProcessInfo& a, const ProcessInfo& b) { return strcmp(a.name, b.name) < 0; }

    bool sortByCPU(const ProcessInfo& a, const ProcessInfo& b) {
        return a.cpuTicks > b.cpuTicks;  // Descending: more CPU first
    }

    bool sortByMemory(const ProcessInfo& a, const ProcessInfo& b) {
        return a.rssPages > b.rssPages;  // Descending: more memory first
    }

    // Main Task Manager application
    int main(uint32_t argc, char** argv) {
        types::UserHeapManager heap;

        SDK::Window window(100, 100, 400, 300, true, "Task Manager");
        SDK::WindowGUI windowGui(window);

        types::UVector<ProcessInfo> processes(heap);
        SortColumn currentSort         = SortColumn::CPU;
        RefreshRate currentRefreshRate = RefreshRate::TWO_SEC;  // Start with 2s refresh
        int scrollY                    = 0;

        const int HEADER_HEIGHT        = 30;
        const int ROW_HEIGHT           = 20;
        const int TABLE_PADDING        = 10;

        // Initial process collection
        collectProcesses(heap, processes);
        std::sort(processes.begin(), processes.end(), sortByCPU);

        // Configurable auto-refresh counter
        uint32_t refreshCounter = 0;

        while (true) {
            windowGui.render();
            windowGui.pollEvents();

            // Set background
            windowGui.setBackground(Color::DarkGray);

            // Configurable auto-refresh logic
            uint32_t refreshInterval = getRefreshFrames(currentRefreshRate);
            if (refreshInterval > 0) {
                refreshCounter++;
                if (refreshCounter >= refreshInterval) {
                    collectProcesses(heap, processes);
                    switch (currentSort) {
                        case SortColumn::PID: std::sort(processes.begin(), processes.end(), sortByPID); break;
                        case SortColumn::NAME: std::sort(processes.begin(), processes.end(), sortByName); break;
                        case SortColumn::CPU: std::sort(processes.begin(), processes.end(), sortByCPU); break;
                        case SortColumn::MEMORY: std::sort(processes.begin(), processes.end(), sortByMemory); break;
                        default: break;
                    }
                    refreshCounter = 0;
                }
            }


            // Control links row (using link style like FileManager for visibility)
            {
                SDK::Layout layout(windowGui, nullptr, false, 20, nullptr);
                windowGui.brush().fillRectangle(layout.getX(), layout.getY(), layout.getX() + layout.getWidth(), layout.getY() + 20, Color::DarkerGray);

                windowGui.text() << "Sort: ";
                if (windowGui.link("PID", false, Color::LightBlue, Color::LighterBlue, Color::DarkBlue)) {
                    currentSort = SortColumn::PID;
                    std::sort(processes.begin(), processes.end(), sortByPID);
                }
                windowGui.text() << " | ";

                if (windowGui.link("Name", false, Color::LightBlue, Color::LighterBlue, Color::DarkBlue)) {
                    currentSort = SortColumn::NAME;
                    std::sort(processes.begin(), processes.end(), sortByName);
                }
                windowGui.text() << " | ";

                if (windowGui.link("CPU", false, Color::LightBlue, Color::LighterBlue, Color::DarkBlue)) {
                    currentSort = SortColumn::CPU;
                    std::sort(processes.begin(), processes.end(), sortByCPU);
                }
                windowGui.text() << " | ";

                if (windowGui.link("Memory", false, Color::LightBlue, Color::LighterBlue, Color::DarkBlue)) {
                    currentSort = SortColumn::MEMORY;
                    std::sort(processes.begin(), processes.end(), sortByMemory);
                }
                windowGui.text() << " --- ";

                if (windowGui.link(getRefreshText(currentRefreshRate), false, Color::Orange, Color::Yellow, Color::DarkRed)) {
                    // Cycle to next refresh rate
                    currentRefreshRate = nextRefreshRate(currentRefreshRate);
                    refreshCounter     = 0;  // Reset counter when changing rate
                }
            }

            // Table header row
            {
                SDK::Layout layout(windowGui, nullptr, false, 18, nullptr);
                windowGui.brush().fillRectangle(layout.getX(), layout.getY(), layout.getX() + layout.getWidth(), layout.getY() + 18, Color::PrimaryDark);

                uint32_t col1 = layout.getX() + 8;
                uint32_t col2 = col1 + 60;
                uint32_t col3 = col2 + 120;
                uint32_t col4 = col3 + 50;
                uint32_t col5 = col4 + 80;

                char headerText[256];
                snprintf(headerText, sizeof(headerText), "  ID      Name                  State    CPU        Memory");
                windowGui.text() << headerText;
            }

            // Process table (scrollable) - FileManager style with column positioning
            {
                SDK::Layout layout(windowGui, &scrollY, true, 0, nullptr);

                uint32_t processIndex = 0;
                int maxPidOffset      = 0;
                int maxNameOffset     = 0;
                int maxStateOffset    = 0;
                int maxCpuOffset      = 0;

                // First column: PID with alternating backgrounds (FileManager style)
                for (size_t i = 0; i < processes.size(); i++) {
                    const ProcessInfo& proc = processes[i];

                    if (processIndex % 2 == 0) { windowGui.fillRectangle(0, windowGui.text().getCursorY() + 1, layout.getWidth(), 16, Color::DarkerGray); }

                    char pidText[16];
                    snprintf(pidText, sizeof(pidText), "%u", proc.pid);
                    if (windowGui.link(pidText, false, Color::LightBlue, Color::LighterBlue, Color::DarkBlue)) {
                        // Future: process details
                    }

                    maxPidOffset = (windowGui.text().getCursorX() > maxPidOffset) ? windowGui.text().getCursorX() : maxPidOffset;
                    windowGui.text() << "\n";
                    processIndex++;
                }

                // Second column: Process Name
                windowGui.text().setCursor(maxPidOffset + 15, scrollY);
                for (size_t i = 0; i < processes.size(); i++) {
                    const ProcessInfo& proc = processes[i];
                    windowGui.text().setCursor(maxPidOffset + 15, windowGui.text().getCursorY());

                    if (windowGui.link(proc.name, false, Color::Primary, Color::PrimaryLight, Color::PrimaryDark)) {
                        // Future: terminate process
                    }

                    maxNameOffset = (windowGui.text().getCursorX() > maxNameOffset) ? windowGui.text().getCursorX() : maxNameOffset;
                    windowGui.text() << "\n";
                }

                // Third column: State
                windowGui.text().setCursor(maxNameOffset + 15, scrollY);
                for (size_t i = 0; i < processes.size(); i++) {
                    const ProcessInfo& proc = processes[i];
                    windowGui.text().setCursor(maxNameOffset + 15, windowGui.text().getCursorY());

                    if (proc.state == 'R') { windowGui.text() << "Running"; }
                    else if (proc.state == 'S') { windowGui.text() << "Sleep"; }
                    else if (proc.state == 'Z') { windowGui.text() << "Zombie"; }
                    else if (proc.state == 'X') { windowGui.text() << "Dead"; }
                    else if (proc.state == 'D') { windowGui.text() << "Waiting"; }
                    else if (proc.state == '?') { windowGui.text() << "Unknown"; }
                    else {
                        // Fallback for any unexpected state
                        char stateText[16];
                        snprintf(stateText, sizeof(stateText), "State_%c", proc.state);
                        windowGui.text() << stateText;
                    }

                    maxStateOffset = (windowGui.text().getCursorX() > maxStateOffset) ? windowGui.text().getCursorX() : maxStateOffset;
                    windowGui.text() << "\n";
                }

                // Fourth column: CPU Ticks
                windowGui.text().setCursor(maxStateOffset + 15, scrollY);
                for (size_t i = 0; i < processes.size(); i++) {
                    const ProcessInfo& proc = processes[i];
                    windowGui.text().setCursor(maxStateOffset + 15, windowGui.text().getCursorY());

                    char cpuText[32];
                    snprintf(cpuText, sizeof(cpuText), "%llu", proc.cpuTicks);
                    windowGui.text() << cpuText;

                    maxCpuOffset = (windowGui.text().getCursorX() > maxCpuOffset) ? windowGui.text().getCursorX() : maxCpuOffset;
                    windowGui.text() << "\n";
                }

                // Fifth column: Memory
                windowGui.text().setCursor(maxCpuOffset + 15, scrollY);
                for (size_t i = 0; i < processes.size(); i++) {
                    const ProcessInfo& proc = processes[i];
                    windowGui.text().setCursor(maxCpuOffset + 15, windowGui.text().getCursorY());

                    char memText[32];
                    snprintf(memText, sizeof(memText), "%u KB", proc.rssPages * 4);
                    windowGui.text() << memText;
                    windowGui.text() << "\n";
                }
            }

            windowGui.swapBuffers();
            sched_yield();
        }

        return 0;
    }

}  // namespace PalmyraOS::Userland::builtin::taskManager

// Entry point for the application
int PalmyraOS_TaskManager_main(uint32_t argc, char** argv) { return PalmyraOS::Userland::builtin::taskManager::main(argc, argv); }
