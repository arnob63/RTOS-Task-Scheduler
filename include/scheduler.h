#pragma once
#include <vector>
#include <deque>
#include <queue>
#include <unordered_map>
#include <memory>
#include "Task.h"
#include "SchedulerTypes.h"
#include "Logger.h"

// -----------------------------------------------------------------------
// ReadyEntry / ReadyEntryCmp
// The Ready Queue is backed by a genuine std::priority_queue. Each entry
// carries the task's priority AND a monotonically increasing sequence
// number assigned at insertion time. The comparator orders first by
// priority (lower number = more urgent = popped first), and breaks ties
// by sequence number (earlier insertion = popped first). This second
// rule is what gives us correct Round Robin FIFO fairness among
// equal-priority tasks, which a plain std::priority_queue does not
// provide on its own.
// -----------------------------------------------------------------------
struct ReadyEntry {
    int priority;
    unsigned long long seq;
    int taskId;
};

struct ReadyEntryCmp {
    // std::priority_queue is a max-heap by this comparator: it keeps at
    // top() the element for which operator()(top, other) is false for
    // every other element. We want top() to be the LOWEST priority
    // number (most urgent) and, among ties, the LOWEST seq (earliest
    // inserted) - so this operator returns true when 'a' is "worse"
    // (should sit further from the top) than 'b'.
    bool operator()(const ReadyEntry& a, const ReadyEntry& b) const {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.seq > b.seq;
    }
};

// -----------------------------------------------------------------------
// ExecutionSegment
// One contiguous block of ticks during which a single task held the CPU.
// The Gantt chart panel is built entirely from a list of these - it is
// derived directly from real tick-by-tick scheduling decisions, not
// parsed from the text log.
// -----------------------------------------------------------------------
struct ExecutionSegment {
    int taskId;
    std::string taskName;
    int startTick;
    int endTick; // inclusive
};

// -----------------------------------------------------------------------
// Scheduler
// The "brain" of the simulation. Owns all tasks and moves them between
// Ready / Running / Waiting / Finished on every tick().
//
// Scheduling policy:
//   - Preemptive Priority Scheduling: lower priority number always wins.
//     If a Ready task has a strictly better priority than the Running
//     task, the Running task is preempted back into Ready.
//   - Round Robin among equal priority: when two Ready tasks share the
//     same priority as the Running task, the Running task only keeps
//     the CPU for `timeQuantum` ticks before being rotated to the back
//     of that priority's queue.
//   - Priority Inheritance: if a high-priority task is Waiting on a
//     resource held by a lower-priority task, the holder's priority is
//     temporarily boosted to prevent priority inversion.
//
// Ready Queue implementation note: backed by std::priority_queue<ReadyEntry>
// (a genuine binary heap), not a plain list. Because std::priority_queue
// cannot remove or re-prioritize an arbitrary element (needed for
// Suspend/Delete/Resume/Priority Inheritance), this class uses the
// standard "lazy deletion" technique: latestReadySeq_ tracks the most
// recent valid sequence number per task ID. Any popped entry whose seq
// doesn't match is stale and is silently discarded instead of used.
// -----------------------------------------------------------------------
class Scheduler {
public:
    explicit Scheduler(Logger& logger);

    // --- Task management (GUI buttons call these) ---
    void addTask(const std::string& name, int priority, int burstTime, int quantum = 2);
    void deleteTask(int taskId);
    void suspendTask(int taskId);   // move a task to Waiting manually
    void resumeTask(int taskId);    // move a Waiting task back to Ready
    void reset();                   // wipe everything, reload default tasks

    // --- Simulation control ---
    void tick();                              // advance simulation by one time unit
    void triggerInterrupt(InterruptType type); // simulate a hardware interrupt

    // --- Accessors for the GUI (read-only views) ---
    const std::vector<TCB>& allTasks() const { return tasks_; }
    std::vector<int> readyQueueIds() const;   // priority order, FIFO among ties
    std::vector<int> waitingQueueIds() const;
    std::vector<int> finishedIds() const;
    int runningTaskId() const { return runningTaskId_; }
    int currentTick() const { return currentTick_; }
    const Logger& logger() const { return logger_; }
    const std::vector<ExecutionSegment>& executionHistory() const { return history_; }

    void loadDefaultTasks();

private:
    std::vector<TCB> tasks_;          // all tasks, indexed by position (not by id!)

    // The Ready Queue: a real binary-heap priority queue, plus the lazy
    // deletion bookkeeping described above.
    std::priority_queue<ReadyEntry, std::vector<ReadyEntry>, ReadyEntryCmp> readyPQ_;
    std::unordered_map<int, unsigned long long> latestReadySeq_; // taskId -> newest valid seq
    unsigned long long readySeqCounter_ = 0;

    std::deque<int> waitingQueue_;    // holds task IDs currently blocked

    std::vector<ExecutionSegment> history_; // Gantt chart data, one entry per contiguous run

    int runningTaskId_ = -1;          // id of task on "CPU", -1 if idle
    int currentTick_ = 0;
    int nextTaskId_ = 1;

    Logger& logger_;

    // --- internal helpers ---
    TCB* findTask(int id);
    int indexOfTask(int id) const;

    void pushReady(TCB& task);         // insert/re-insert a task into the priority queue
    // Discards stale heap entries from the top and reports the best
    // currently-valid (priority, taskId) without popping it. Returns
    // false if the Ready Queue is empty of valid entries.
    bool peekBestReady(int& outPriority, int& outTaskId);

    void pickNextTask();               // choose who runs next (priority + RR)
    void contextSwitch(int fromId, int toId, const std::string& reason);
    void checkPreemption();            // see if Running task should be preempted
    void applyPriorityInheritance();   // boost holder priority if needed
    void restorePriorityIfNeeded(TCB& task);

    void moveToReady(TCB& task);
    void moveToWaiting(TCB& task);
    void moveToFinished(TCB& task);
};
