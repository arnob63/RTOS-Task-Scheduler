#include "Scheduler.h"
#include <algorithm>

Scheduler::Scheduler(Logger& logger)
    : logger_(logger) {
    loadDefaultTasks();
}

// -----------------------------------------------------------------------
// Task lookup helpers
// -----------------------------------------------------------------------
TCB* Scheduler::findTask(int id) {
    for (auto& t : tasks_) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

int Scheduler::indexOfTask(int id) const {
    for (size_t i = 0; i < tasks_.size(); ++i) {
        if (tasks_[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

// -----------------------------------------------------------------------
// Ready Queue (std::priority_queue) helpers
// -----------------------------------------------------------------------

// Inserts (or re-inserts, e.g. after a priority boost) a task into the
// Ready priority queue. latestReadySeq_ is updated to this new sequence
// number, which automatically makes any older queued entry for the same
// task stale - it will be silently discarded when popped.
void Scheduler::pushReady(TCB& task) {
    unsigned long long seq = readySeqCounter_++;
    readyPQ_.push(ReadyEntry{ task.priority, seq, task.id });
    latestReadySeq_[task.id] = seq;
}

// Discards stale entries from the top of the heap (tasks that were
// deleted, suspended, or re-queued with a newer priority since being
// pushed) and reports the best still-valid entry WITHOUT popping it.
bool Scheduler::peekBestReady(int& outPriority, int& outTaskId) {
    while (!readyPQ_.empty()) {
        const ReadyEntry& top = readyPQ_.top();
        auto it = latestReadySeq_.find(top.taskId);
        bool stale = (it == latestReadySeq_.end() || it->second != top.seq);
        if (stale) {
            readyPQ_.pop(); // permanently discard - a newer or cancelling entry exists
            continue;
        }
        outPriority = top.priority;
        outTaskId = top.taskId;
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------
// Default tasks (spec requires at least 3 predefined tasks)
// -----------------------------------------------------------------------
void Scheduler::loadDefaultTasks() {
    tasks_.clear();
    readyPQ_ = std::priority_queue<ReadyEntry, std::vector<ReadyEntry>, ReadyEntryCmp>();
    latestReadySeq_.clear();
    readySeqCounter_ = 0;
    waitingQueue_.clear();
    history_.clear();
    runningTaskId_ = -1;
    currentTick_ = 0;
    nextTaskId_ = 1;

    addTask("Task-A", 1, 8, 2);  // high priority, needs 8 ticks total
    addTask("Task-B", 2, 6, 2);  // medium priority
    addTask("Task-C", 2, 6, 2);  // same priority as B -> Round Robin between them

    // Task-D: low priority, but holds a shared resource.
    // Task-A (high priority) wants that same resource. If you Suspend
    // Task-A (moving it to Waiting), applyPriorityInheritance() will see
    // that a high-priority waiter wants a resource held by a lower-priority
    // task, and temporarily boosts Task-D's priority so it finishes and
    // releases the resource sooner. This gives a visible, demo-able example
    // of the Priority Inheritance Protocol.
    addTask("Task-D", 5, 10, 2);

    TCB* taskA = findTask(1);
    TCB* taskD = findTask(4);
    if (taskA) taskA->wantsResource = true;
    if (taskD) taskD->holdsResource = true;

    logger_.add(currentTick_, LogEventType::Info,
                "Tip: Suspend Task-A to see Priority Inheritance trigger "
                "(Task-D holds a resource Task-A wants).");
}

void Scheduler::reset() {
    logger_.clear();
    loadDefaultTasks();
    logger_.add(currentTick_, LogEventType::Info, "Simulation reset.");
}

// -----------------------------------------------------------------------
// Task management
// -----------------------------------------------------------------------
void Scheduler::addTask(const std::string& name, int priority, int burstTime, int quantum) {
    TCB task(nextTaskId_++, name, priority, burstTime, quantum);
    tasks_.push_back(task);
    pushReady(tasks_.back());

    logger_.add(currentTick_, LogEventType::Info,
                "Added " + task.name + " (priority " + std::to_string(priority) +
                ", burst " + std::to_string(burstTime) + ")");
}

void Scheduler::deleteTask(int taskId) {
    int idx = indexOfTask(taskId);
    if (idx < 0) return;

    // Lazily invalidate any Ready-queue entry for this task; it will be
    // discarded automatically the next time the heap is drained past it.
    latestReadySeq_.erase(taskId);
    waitingQueue_.erase(std::remove(waitingQueue_.begin(), waitingQueue_.end(), taskId), waitingQueue_.end());

    if (runningTaskId_ == taskId) {
        runningTaskId_ = -1;
    }

    logger_.add(currentTick_, LogEventType::Info, "Deleted " + tasks_[idx].name);
    tasks_.erase(tasks_.begin() + idx);
}

void Scheduler::suspendTask(int taskId) {
    TCB* task = findTask(taskId);
    if (!task) return;
    if (task->state == TaskState::Finished) return;

    if (runningTaskId_ == taskId) {
        runningTaskId_ = -1;
    }
    // Invalidate any pending Ready-queue entry for this task.
    latestReadySeq_.erase(taskId);

    moveToWaiting(*task);
    logger_.add(currentTick_, LogEventType::Info, task->name + " suspended manually.");
}

void Scheduler::resumeTask(int taskId) {
    TCB* task = findTask(taskId);
    if (!task) return;
    if (task->state != TaskState::Waiting && task->state != TaskState::Interrupted) return;

    waitingQueue_.erase(std::remove(waitingQueue_.begin(), waitingQueue_.end(), taskId), waitingQueue_.end());
    moveToReady(*task);
    logger_.add(currentTick_, LogEventType::Info, task->name + " resumed to Ready queue.");
}

// -----------------------------------------------------------------------
// State transition helpers
// -----------------------------------------------------------------------
void Scheduler::moveToReady(TCB& task) {
    task.state = TaskState::Ready;
    task.quantumUsed = 0;
    pushReady(task);
}

void Scheduler::moveToWaiting(TCB& task) {
    task.state = TaskState::Waiting;
    if (std::find(waitingQueue_.begin(), waitingQueue_.end(), task.id) == waitingQueue_.end()) {
        waitingQueue_.push_back(task.id);
    }
}

void Scheduler::moveToFinished(TCB& task) {
    task.state = TaskState::Finished;
    task.remainingTime = 0;
    logger_.add(currentTick_, LogEventType::TaskCompletion, task.name + " finished execution.");
}

// -----------------------------------------------------------------------
// Picking the next task to run: pop the top of the real priority queue
// (after discarding any stale entries). Ties are already broken by
// insertion order (seq) inside the heap's comparator, which is what
// gives us correct Round Robin FIFO behavior among equal priorities.
// -----------------------------------------------------------------------
void Scheduler::pickNextTask() {
    int bestPriority, bestId;
    if (!peekBestReady(bestPriority, bestId)) {
        runningTaskId_ = -1;
        return;
    }

    readyPQ_.pop();
    latestReadySeq_.erase(bestId);

    int previousId = runningTaskId_;
    runningTaskId_ = bestId;

    TCB* nextTask = findTask(bestId);
    if (nextTask) {
        nextTask->state = TaskState::Running;
        nextTask->quantumUsed = 0;
    }

    if (previousId != bestId) {
        std::string fromName = (previousId != -1 && findTask(previousId)) ? findTask(previousId)->name : "Idle";
        std::string toName = nextTask ? nextTask->name : "Unknown";
        contextSwitch(previousId, bestId, fromName + " -> " + toName);
    }
}

void Scheduler::contextSwitch(int fromId, int toId, const std::string& reason) {
    (void)fromId;
    (void)toId;
    logger_.add(currentTick_, LogEventType::ContextSwitch, "Context switch: " + reason);
}

// -----------------------------------------------------------------------
// Preemption check: if the currently Running task no longer has the best
// priority among Ready tasks, it gets kicked back to Ready and the better
// task takes over immediately (this is what makes it "preemptive").
// -----------------------------------------------------------------------
void Scheduler::checkPreemption() {
    if (runningTaskId_ == -1) return;

    TCB* running = findTask(runningTaskId_);
    if (!running) return;

    int bestReadyPriority, bestReadyId;
    if (!peekBestReady(bestReadyPriority, bestReadyId)) return;

    if (bestReadyPriority < running->priority) {
        logger_.add(currentTick_, LogEventType::Preemption,
                    running->name + " preempted by a higher priority task.");
        moveToReady(*running);
        runningTaskId_ = -1;
        pickNextTask();
    }
}

// -----------------------------------------------------------------------
// Priority Inheritance Protocol (simplified):
// If a Waiting task is blocked because it wants a resource held by a
// lower-priority (numerically higher) task, temporarily boost the
// holder's priority to match the waiter's, so the holder finishes faster
// and releases the resource sooner. This avoids "priority inversion."
//
// If the holder is currently sitting in the Ready priority queue, its
// existing heap entry reflects its OLD (lower-urgency) priority, so we
// must re-insert it via pushReady() with the boosted priority. The old
// entry is automatically invalidated because pushReady() updates
// latestReadySeq_, so it will be discarded as stale when eventually
// reached in the heap.
// -----------------------------------------------------------------------
void Scheduler::applyPriorityInheritance() {
    for (int waitingId : waitingQueue_) {
        TCB* waiter = findTask(waitingId);
        if (!waiter || !waiter->wantsResource) continue;

        for (auto& holder : tasks_) {
            if (holder.holdsResource && holder.id != waiter->id &&
                holder.priority > waiter->priority &&
                (holder.state == TaskState::Ready || holder.state == TaskState::Running)) {

                if (!holder.boosted) {
                    holder.originalPriority = holder.priority;
                }
                holder.priority = waiter->priority;
                holder.boosted = true;

                if (holder.state == TaskState::Ready) {
                    // Re-queue with the new, boosted priority so the heap
                    // actually reflects it, not just the displayed field.
                    pushReady(holder);
                }

                logger_.add(currentTick_, LogEventType::PriorityInheritance,
                            holder.name + " priority boosted to " + std::to_string(holder.priority) +
                            " (inherited from " + waiter->name + ")");
            }
        }
    }
}

void Scheduler::restorePriorityIfNeeded(TCB& task) {
    if (task.boosted) {
        task.priority = task.originalPriority;
        task.boosted = false;
        logger_.add(currentTick_, LogEventType::PriorityInheritance,
                    task.name + " priority restored to " + std::to_string(task.priority));
    }
}

// -----------------------------------------------------------------------
// tick(): advance the simulation by exactly one time unit.
// -----------------------------------------------------------------------
void Scheduler::tick() {
    currentTick_++;

    // Waiting-time accounting for every task currently valid in the
    // Ready priority queue. readyQueueIds() already performs the
    // lazy-deletion filtering, so this list is guaranteed accurate.
    for (int id : readyQueueIds()) {
        TCB* t = findTask(id);
        if (t) t->waitingTime++;
    }

    applyPriorityInheritance();

    if (runningTaskId_ == -1) {
        pickNextTask();
    } else {
        checkPreemption();
    }

    if (runningTaskId_ != -1) {
        TCB* running = findTask(runningTaskId_);
        if (running) {
            running->remainingTime--;
            running->quantumUsed++;

            // Record this tick's execution for the Gantt chart. If the
            // same task also ran the immediately previous tick, extend
            // that segment instead of starting a new one - this is what
            // produces one continuous bar instead of one bar per tick.
            if (!history_.empty() && history_.back().taskId == running->id &&
                history_.back().endTick == currentTick_ - 1) {
                history_.back().endTick = currentTick_;
            } else {
                history_.push_back(ExecutionSegment{ running->id, running->name, currentTick_, currentTick_ });
            }

            if (running->remainingTime <= 0) {
                moveToFinished(*running);
                restorePriorityIfNeeded(*running);
                runningTaskId_ = -1;
                pickNextTask();
            } else if (running->quantumUsed >= running->timeQuantum) {
                bool samePriorityWaiting = false;
                for (int id : readyQueueIds()) {
                    TCB* t = findTask(id);
                    if (t && t->priority == running->priority) {
                        samePriorityWaiting = true;
                        break;
                    }
                }
                if (samePriorityWaiting) {
                    logger_.add(currentTick_, LogEventType::ContextSwitch,
                                running->name + " time quantum expired, rotating (Round Robin).");
                    moveToReady(*running);
                    runningTaskId_ = -1;
                    pickNextTask();
                } else {
                    running->quantumUsed = 0;
                }
            }
        }
    }
}

// -----------------------------------------------------------------------
// Simulated hardware interrupt.
// -----------------------------------------------------------------------
void Scheduler::triggerInterrupt(InterruptType type) {
    logger_.add(currentTick_, LogEventType::Interrupt,
                std::string("Hardware interrupt fired: ") + InterruptTypeToString(type));

    switch (type) {
        case InterruptType::IO_COMPLETE: {
            if (!waitingQueue_.empty()) {
                int id = waitingQueue_.front();
                waitingQueue_.pop_front();
                TCB* t = findTask(id);
                if (t) {
                    moveToReady(*t);
                    logger_.add(currentTick_, LogEventType::Interrupt,
                                t->name + " woken up by IO_COMPLETE interrupt.");
                }
            }
            break;
        }
        case InterruptType::TIMER_FAULT: {
            if (runningTaskId_ != -1) {
                TCB* running = findTask(runningTaskId_);
                if (running) {
                    logger_.add(currentTick_, LogEventType::Preemption,
                                running->name + " force-preempted by TIMER_FAULT interrupt.");
                    running->state = TaskState::Interrupted;
                    moveToReady(*running);
                    runningTaskId_ = -1;
                    pickNextTask();
                }
            }
            break;
        }
        case InterruptType::RESET_SIGNAL: {
            if (runningTaskId_ != -1) {
                TCB* running = findTask(runningTaskId_);
                if (running) {
                    moveToReady(*running);
                    runningTaskId_ = -1;
                }
            }
            break;
        }
    }
}

// -----------------------------------------------------------------------
// GUI read-only accessors
// -----------------------------------------------------------------------

// Returns the Ready Queue's task IDs in true scheduling order (most
// urgent first, FIFO among ties) WITHOUT disturbing the real queue: a
// full copy of the heap is drained instead of the live one.
std::vector<int> Scheduler::readyQueueIds() const {
    std::priority_queue<ReadyEntry, std::vector<ReadyEntry>, ReadyEntryCmp> copy = readyPQ_;
    std::vector<int> result;
    while (!copy.empty()) {
        ReadyEntry e = copy.top();
        copy.pop();
        auto it = latestReadySeq_.find(e.taskId);
        if (it != latestReadySeq_.end() && it->second == e.seq) {
            result.push_back(e.taskId);
        }
    }
    return result;
}

std::vector<int> Scheduler::waitingQueueIds() const {
    return std::vector<int>(waitingQueue_.begin(), waitingQueue_.end());
}

std::vector<int> Scheduler::finishedIds() const {
    std::vector<int> result;
    for (const auto& t : tasks_) {
        if (t.state == TaskState::Finished) result.push_back(t.id);
    }
    return result;
}
