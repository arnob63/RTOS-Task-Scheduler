#pragma once
#include <string>

// -----------------------------------------------------------------------
// LogEventType
// Categorizes entries shown in the bottom "live execution log" panel.
// Kept separate from plain text so the GUI can color-code log lines.
// -----------------------------------------------------------------------
enum class LogEventType {
    ContextSwitch,
    Interrupt,
    Preemption,
    PriorityInheritance,
    TaskCompletion,
    Info          // generic / tick info messages
};

inline const char* LogEventTypeToString(LogEventType type) {
    switch (type) {
        case LogEventType::ContextSwitch:        return "Context Switch";
        case LogEventType::Interrupt:             return "Interrupt";
        case LogEventType::Preemption:             return "Preemption";
        case LogEventType::PriorityInheritance:    return "Priority Inheritance";
        case LogEventType::TaskCompletion:         return "Task Completion";
        case LogEventType::Info:                   return "Info";
    }
    return "Unknown";
}

// -----------------------------------------------------------------------
// InterruptType
// Simple set of simulated hardware interrupts the user can trigger
// from the GUI ("Trigger Interrupt" button).
// -----------------------------------------------------------------------
enum class InterruptType {
    IO_COMPLETE,   // simulated I/O device finished, wakes a waiting task
    TIMER_FAULT,   // forces a context switch regardless of priority
    RESET_SIGNAL   // generic external reset-like interrupt
};

inline const char* InterruptTypeToString(InterruptType type) {
    switch (type) {
        case InterruptType::IO_COMPLETE: return "IO_COMPLETE";
        case InterruptType::TIMER_FAULT: return "TIMER_FAULT";
        case InterruptType::RESET_SIGNAL: return "RESET_SIGNAL";
    }
    return "Unknown";
}

// -----------------------------------------------------------------------
// LogEntry
// One line in the execution log, with the tick it happened on.
// -----------------------------------------------------------------------
struct LogEntry {
    int tick = 0;
    LogEventType type = LogEventType::Info;
    std::string message;

    LogEntry() = default;
    LogEntry(int tick_, LogEventType type_, const std::string& message_)
        : tick(tick_), type(type_), message(message_) {}
};
