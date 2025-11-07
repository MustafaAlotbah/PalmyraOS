#include "palmyraOS/palmyraSDK.h"
#include "palmyraOS/time.h"  // Must be first to define custom timespec

// Prevent system headers from redefining struct timespec
#define _BITS_TYPES_STRUCT_TIMESPEC_H
#define _SYS_TYPES_H

#include "palmyraOS/unistd.h"

namespace PalmyraOS::Userland::builtin::taskManager {

    // Simple template swap helper
    template<typename T>
    void swap(T& a, T& b) {
        T temp = a;
        a      = b;
        b      = temp;
    }

    // Simple bubble sort (safe, no STL conflicts)
    template<typename T, typename Comparator>
    void sortVector(types::UVector<T>& vec, Comparator comp) {
        if (vec.size() <= 1) return;
        for (size_t i = 0; i < vec.size(); i++) {
            for (size_t j = 0; j < vec.size() - i - 1; j++) {
                if (comp(vec[j + 1], vec[j])) { swap(vec[j], vec[j + 1]); }
            }
        }
    }

    // Process information structure
    struct ProcessInfo {
        uint32_t pid;
        char name[64];
        char state;
        uint64_t cpuTicks;
        uint64_t previousCpuTicks;  // For delta calculation
        uint32_t cpuPercent;        // Calculated CPU percentage
        uint32_t rssPages;
    };

    // Sort column enumeration
    enum class SortColumn { PID, NAME, CPU, MEMORY, STATE };

    // Refresh rate options
    enum class RefreshRate { OFF, ONE_SEC, TWO_SEC, THREE_SEC, FIVE_SEC, TEN_SEC };

    // Convert refresh rate to seconds interval (time-based, accurate)
    uint32_t getRefreshSeconds(RefreshRate rate) {
        switch (rate) {
            case RefreshRate::OFF: return 0;        // No auto-refresh
            case RefreshRate::ONE_SEC: return 1;    // 1 second
            case RefreshRate::TWO_SEC: return 2;    // 2 seconds
            case RefreshRate::THREE_SEC: return 3;  // 3 seconds
            case RefreshRate::FIVE_SEC: return 5;   // 5 seconds
            case RefreshRate::TEN_SEC: return 10;   // 10 seconds
            default: return 2;
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
        return a.cpuPercent > b.cpuPercent;  // Descending: higher percentage first
    }

    bool sortByMemory(const ProcessInfo& a, const ProcessInfo& b) {
        return a.rssPages > b.rssPages;  // Descending: more memory first
    }

    // Main Task Manager application
    int main(uint32_t argc, char** argv) {
        types::UserHeapManager heap;

        SDK::Window window(100, 100, 400, 300, true, "Task Manager");
        SDK::WindowGUI windowGui(window);
        windowGui.text().setFont(PalmyraOS::Font::Poppins12);

        types::UVector<ProcessInfo> processes(heap);
        SortColumn currentSort         = SortColumn::CPU;
        RefreshRate currentRefreshRate = RefreshRate::TWO_SEC;  // Start with 2s refresh
        int scrollY                    = 0;

        const int HEADER_HEIGHT        = 30;
        const int ROW_HEIGHT           = 20;
        const int TABLE_PADDING        = 10;

        // Initial process collection
        collectProcesses(heap, processes);
        sortVector(processes, sortByCPU);

        // Time-based refresh tracking for accurate CPU percentage calculation
        timespec lastRefreshTime{};
        clock_gettime(CLOCK_MONOTONIC, &lastRefreshTime);

        // System constants for CPU calculation
        constexpr uint32_t SYSTEM_HZ      = 100;  // Linux standard: 100 ticks/second
        constexpr uint32_t MIN_REFRESH_MS = 500;  // Minimum 500ms between refreshes for stable readings

        while (true) {
            windowGui.render();
            windowGui.pollEvents();

            // Set background
            windowGui.setBackground(Color::DarkGray);

            // Time-based auto-refresh with proper CPU percentage calculation
            uint32_t refreshSeconds = getRefreshSeconds(currentRefreshRate);
            if (refreshSeconds > 0) {
                timespec currentTime{};
                clock_gettime(CLOCK_MONOTONIC, &currentTime);

                // Calculate elapsed time in milliseconds
                uint64_t elapsedMs       = ((currentTime.tv_sec - lastRefreshTime.tv_sec) * 1000ULL) + ((currentTime.tv_nsec - lastRefreshTime.tv_nsec) / 1000000ULL);

                // Only refresh if enough time has passed (prevents unstable readings)
                uint32_t targetRefreshMs = refreshSeconds * 1000;
                if (elapsedMs >= targetRefreshMs && elapsedMs >= MIN_REFRESH_MS) {

                    // Store previous process data for delta calculation
                    types::UVector<ProcessInfo> previousProcesses(heap);
                    for (const auto& proc: processes) { previousProcesses.push_back(proc); }

                    // Collect new process data
                    collectProcesses(heap, processes);

                    // STEP 1: Calculate delta ticks for each process and sum total delta
                    uint64_t totalDeltaTicks = 0;
                    for (auto& currentProc: processes) {
                        // Find matching previous process data by PID
                        ProcessInfo* prevProc = nullptr;
                        for (auto& prev: previousProcesses) {
                            if (prev.pid == currentProc.pid) {
                                prevProc = &prev;
                                break;
                            }
                        }

                        if (prevProc != nullptr && currentProc.cpuTicks >= prevProc->cpuTicks) {
                            // Calculate delta: how many ticks this process used in this interval
                            uint64_t deltaTicks          = currentProc.cpuTicks - prevProc->cpuTicks;
                            currentProc.previousCpuTicks = deltaTicks;  // Store delta temporarily
                            totalDeltaTicks += deltaTicks;
                        }
                        else {
                            // First measurement or counter reset
                            currentProc.previousCpuTicks = 0;
                        }
                    }

                    // STEP 2: Calculate relative CPU percentages based on share of total delta
                    for (auto& currentProc: processes) {
                        uint64_t thisProcDelta = currentProc.previousCpuTicks;

                        if (totalDeltaTicks > 0 && thisProcDelta > 0) {
                            // Relative CPU% = (this process's delta / total delta) * 100
                            currentProc.cpuPercent = (thisProcDelta * 100) / totalDeltaTicks;
                        }
                        else {
                            // No activity or first measurement
                            currentProc.cpuPercent = 0;
                        }

                        // DEBUG: Show calculation for first process
                        if (currentProc.pid == processes[0].pid) {
                            char debugMsg[200];
                            snprintf(debugMsg,
                                     sizeof(debugMsg),
                                     "[RELATIVE CPU] PID=%d delta=%llu, total=%llu, percent=%u%%",
                                     currentProc.pid,
                                     thisProcDelta,
                                     totalDeltaTicks,
                                     currentProc.cpuPercent);
                            windowGui.text() << debugMsg << "\n";
                        }

                        // Update previousCpuTicks with actual current value for next cycle
                        currentProc.previousCpuTicks = currentProc.cpuTicks;
                    }

                    // Sort by current column
                    switch (currentSort) {
                        case SortColumn::PID: sortVector(processes, sortByPID); break;
                        case SortColumn::NAME: sortVector(processes, sortByName); break;
                        case SortColumn::CPU: sortVector(processes, sortByCPU); break;
                        case SortColumn::MEMORY: sortVector(processes, sortByMemory); break;
                        default: break;
                    }

                    // Update timestamp for next refresh cycle
                    lastRefreshTime = currentTime;
                }
            }


            // Control links row (using link style like FileManager for visibility)
            {
                SDK::Layout layout(windowGui, nullptr, false, 20, nullptr);
                windowGui.brush().fillRectangle(layout.getX(), layout.getY(), layout.getX() + layout.getWidth(), layout.getY() + 20, Color::DarkerGray);

                windowGui.text() << "Sort: ";
                if (windowGui.link("PID", false, Color::LightBlue, Color::LighterBlue, Color::DarkBlue)) {
                    currentSort = SortColumn::PID;
                    sortVector(processes, sortByPID);
                }
                windowGui.text() << " | ";

                if (windowGui.link("Name", false, Color::LightBlue, Color::LighterBlue, Color::DarkBlue)) {
                    currentSort = SortColumn::NAME;
                    sortVector(processes, sortByName);
                }
                windowGui.text() << " | ";

                if (windowGui.link("CPU", false, Color::LightBlue, Color::LighterBlue, Color::DarkBlue)) {
                    currentSort = SortColumn::CPU;
                    sortVector(processes, sortByCPU);
                }
                windowGui.text() << " | ";

                if (windowGui.link("Memory", false, Color::LightBlue, Color::LighterBlue, Color::DarkBlue)) {
                    currentSort = SortColumn::MEMORY;
                    sortVector(processes, sortByMemory);
                }
                windowGui.text() << " --- ";

                if (windowGui.link(getRefreshText(currentRefreshRate), false, Color::Orange, Color::Yellow, Color::DarkRed)) {
                    // Cycle to next refresh rate
                    currentRefreshRate      = nextRefreshRate(currentRefreshRate);
                    // Reset refresh timer when changing rate (force immediate refresh)
                    lastRefreshTime.tv_sec  = 0;
                    lastRefreshTime.tv_nsec = 0;
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
                int yOffset           = 2;
                int rowHeight         = 20;

                // First column: PID with alternating backgrounds (FileManager style)
                for (size_t i = 0; i < processes.size(); i++) {
                    const ProcessInfo& proc = processes[i];
                    windowGui.text().setCursor(0, windowGui.text().getCursorY() + yOffset);
                    if (processIndex % 2 == 0) { windowGui.fillRectangle(0, windowGui.text().getCursorY(), layout.getWidth(), rowHeight, Color::DarkerGray); }

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
                    windowGui.text().setCursor(maxPidOffset + 15, windowGui.text().getCursorY() + yOffset);

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
                    windowGui.text().setCursor(0, windowGui.text().getCursorY() + yOffset);
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
                    windowGui.text().setCursor(maxStateOffset + 15, windowGui.text().getCursorY() + yOffset);

                    char cpuText[32];
                    snprintf(cpuText, sizeof(cpuText), "%u%%", proc.cpuPercent);
                    windowGui.text() << cpuText;

                    maxCpuOffset = (windowGui.text().getCursorX() > maxCpuOffset) ? windowGui.text().getCursorX() : maxCpuOffset;
                    windowGui.text() << "\n";
                }

                // Fifth column: Memory
                windowGui.text().setCursor(maxCpuOffset + 15, scrollY);
                for (size_t i = 0; i < processes.size(); i++) {
                    const ProcessInfo& proc = processes[i];
                    windowGui.text().setCursor(maxCpuOffset + 15, windowGui.text().getCursorY() + yOffset);

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
