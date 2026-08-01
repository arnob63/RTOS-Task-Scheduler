#pragma once
#include <vector>
#include "SchedulerTypes.h"

// -----------------------------------------------------------------------
// Logger
// A simple rolling buffer of LogEntry lines. The scheduler pushes events
// into it; the GUI reads from it every frame to render the bottom panel.
// Deliberately dumb/simple: no files, no threads, just a vector.
// -----------------------------------------------------------------------
class Logger {
public:
    explicit Logger(size_t maxEntries = 500);

    void add(int tick, LogEventType type, const std::string& message);
    void clear();

    const std::vector<LogEntry>& entries() const { return entries_; }

private:
    std::vector<LogEntry> entries_;
    size_t maxEntries_;
};
