#pragma once
#include <string>

// -----------------------------------------------------------------------
// TaskState
// Represents the lifecycle state of a simulated task.
// -----------------------------------------------------------------------
enum class TaskState {
    Ready,        // waiting in the ready queue, eligible to run
    Running,      // currently executing on the (simulated) CPU
    Waiting,      // blocked, e.g. waiting on a resource/mutex
    Finished,     // completed all of its burst time
    Interrupted   // temporarily knocked out by a hardware interrupt
};

// Helper to convert TaskState to a human-readable string (used by GUI/log).
inline const char* TaskStateToString(TaskState state) {
    switch (state) {
        case TaskState::Ready:       return "Ready";
        case TaskState::Running:     return "Running";
        case TaskState::Waiting:     return "Waiting";
        case TaskState::Finished:    return "Finished";
        case TaskState::Interrupted: return "Interrupted";
    }
    return "Unknown";
}

// -----------------------------------------------------------------------
// TCB (Task Control Block)
// Holds everything the scheduler needs to know about one task.
// This mirrors what a real OS kernel stores per-task, simplified.
// -----------------------------------------------------------------------
struct TCB {
    int id = -1;                 // unique task identifier
    std::string name;            // human-readable name (e.g. "Task-A")

    int priority = 0;            // lower number = higher priority (0 = highest)
    int originalPriority = 0;    // priority before any inheritance boost

    int burstTime = 0;           // total CPU time this task needs (in ticks)
    int remainingTime = 0;       // how much burst time is left
    int waitingTime = 0;         // total ticks spent waiting in Ready queue

    int timeQuantum = 2;         // Round Robin slice size for equal-priority tasks
    int quantumUsed = 0;         // how much of the current quantum has been used

    TaskState state = TaskState::Ready;

    bool holdsResource = false;  // true if this task currently holds a shared resource
    bool boosted = false;        // true if priority was raised via inheritance

    TCB() = default;

    TCB(int id_, const std::string& name_, int priority_, int burstTime_, int quantum_ = 2)
        : id(id_),
          name(name_),
          priority(priority_),
          originalPriority(priority_),
          burstTime(burstTime_),
          remainingTime(burstTime_),
          waitingTime(0),
          timeQuantum(quantum_),
          quantumUsed(0),
          state(TaskState::Ready),
          holdsResource(false),
          boosted(false) {}
};
