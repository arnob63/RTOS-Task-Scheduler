#include "Logger.h"

Logger::Logger(size_t maxEntries)
    : maxEntries_(maxEntries) {
    entries_.reserve(maxEntries_);
}

void Logger::add(int tick, LogEventType type, const std::string& message) {
    entries_.emplace_back(tick, type, message);

    // Keep the buffer bounded so the log panel doesn't grow forever
    // during a long Auto Run session.
    if (entries_.size() > maxEntries_) {
        // Drop the oldest entry. A vector erase at front is O(n), but
        // maxEntries_ is small (a few hundred), so this is fine here.
        entries_.erase(entries_.begin());
    }
}

void Logger::clear() {
    entries_.clear();
}
