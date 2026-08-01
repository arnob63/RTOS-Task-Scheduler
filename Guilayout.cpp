//GuiLayout.cpp

#include "GuiLayout.h"
#include "imgui.h"
#include <cstdio>
#include <algorithm>

namespace {

// Returns an ImGui color for a given task state, matching the spec:
// Green = Running, Blue = Ready, Yellow = Waiting, Gray = Finished, Red = Interrupted
ImVec4 colorForState(TaskState state) {
    switch (state) {
        case TaskState::Running:     return ImVec4(0.20f, 0.75f, 0.30f, 1.0f); // green
        case TaskState::Ready:       return ImVec4(0.25f, 0.55f, 0.95f, 1.0f); // blue
        case TaskState::Waiting:     return ImVec4(0.90f, 0.80f, 0.20f, 1.0f); // yellow
        case TaskState::Finished:    return ImVec4(0.55f, 0.55f, 0.55f, 1.0f); // gray
        case TaskState::Interrupted: return ImVec4(0.85f, 0.25f, 0.25f, 1.0f); // red
    }
    return ImVec4(1, 1, 1, 1);
}

// Color for a log line, based on event type - helps the eye scan the log.
ImVec4 colorForLogEvent(LogEventType type) {
    switch (type) {
        case LogEventType::ContextSwitch:        return ImVec4(0.25f, 0.55f, 0.95f, 1.0f);
        case LogEventType::Interrupt:             return ImVec4(0.85f, 0.25f, 0.25f, 1.0f);
        case LogEventType::Preemption:             return ImVec4(0.95f, 0.55f, 0.15f, 1.0f);
        case LogEventType::PriorityInheritance:    return ImVec4(0.65f, 0.35f, 0.85f, 1.0f);
        case LogEventType::TaskCompletion:         return ImVec4(0.20f, 0.75f, 0.30f, 1.0f);
        case LogEventType::Info:                   return ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
    }
    return ImVec4(1, 1, 1, 1);
}

} // namespace

namespace GuiLayout {

static void drawTopBar(Scheduler& scheduler) {
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1920, 50), ImGuiCond_FirstUseEver);
    ImGui::Begin("Simulation Status", nullptr,
                  ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Tick: %d", scheduler.currentTick());
    ImGui::SameLine();
    ImGui::Text("  |  ");
    ImGui::SameLine();

    int runningId = scheduler.runningTaskId();
    if (runningId == -1) {
        ImGui::Text("Running Task: (CPU idle)");
    } else {
        const TCB* running = nullptr;
        for (const auto& t : scheduler.allTasks()) {
            if (t.id == runningId) { running = &t; break; }
        }
        if (running) {
            ImGui::TextColored(colorForState(TaskState::Running),
                                "Running Task: %s (prio %d)", running->name.c_str(), running->priority);
        }
    }

    ImGui::End();
}

static void drawLeftPanel(Scheduler& scheduler, UiState& ui) {
    ImGui::SetNextWindowPos(ImVec2(0, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380, 900), ImGuiCond_FirstUseEver);
    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoCollapse);

    const ImVec2 buttonSize(-1, 38); // taller buttons, full panel width

    ImGui::SeparatorText("Add Task");
    ImGui::Spacing();
    ImGui::InputText("Name", ui.newTaskName, IM_ARRAYSIZE(ui.newTaskName));
    ImGui::InputInt("Priority", &ui.newTaskPriority);
    ImGui::InputInt("Burst Time", &ui.newTaskBurst);
    ImGui::InputInt("Quantum", &ui.newTaskQuantum);

    if (ui.newTaskPriority < 0) ui.newTaskPriority = 0;
    if (ui.newTaskBurst < 1) ui.newTaskBurst = 1;
    if (ui.newTaskQuantum < 1) ui.newTaskQuantum = 1;

    ImGui::Spacing();
    if (ImGui::Button("Add Task", buttonSize)) {
        scheduler.addTask(ui.newTaskName, ui.newTaskPriority, ui.newTaskBurst, ui.newTaskQuantum);
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Selected Task");
    ImGui::Spacing();
    ImGui::TextWrapped("Selected ID: %s",
                ui.selectedTaskId == -1 ? "(none)" : std::to_string(ui.selectedTaskId).c_str());
    ImGui::TextWrapped("Tip: click a row in the task table to select it.");
    ImGui::Spacing();

    ImGui::BeginDisabled(ui.selectedTaskId == -1);
    if (ImGui::Button("Delete Task", buttonSize)) {
        scheduler.deleteTask(ui.selectedTaskId);
        ui.selectedTaskId = -1;
    }
    if (ImGui::Button("Suspend", buttonSize)) {
        scheduler.suspendTask(ui.selectedTaskId);
    }
    if (ImGui::Button("Resume", buttonSize)) {
        scheduler.resumeTask(ui.selectedTaskId);
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::SeparatorText("Interrupts");
    ImGui::Spacing();
    if (ImGui::Button("Trigger IO_COMPLETE", buttonSize)) {
        scheduler.triggerInterrupt(InterruptType::IO_COMPLETE);
    }
    if (ImGui::Button("Trigger TIMER_FAULT", buttonSize)) {
        scheduler.triggerInterrupt(InterruptType::TIMER_FAULT);
    }
    if (ImGui::Button("Trigger RESET_SIGNAL", buttonSize)) {
        scheduler.triggerInterrupt(InterruptType::RESET_SIGNAL);
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Simulation");
    ImGui::Spacing();
    if (ImGui::Button("Run One Tick", buttonSize)) {
        scheduler.tick();
    }
    ImGui::Spacing();
    ImGui::Checkbox("Auto Run", &ui.autoRun);
    ImGui::SliderFloat("Tick Interval (s)", &ui.autoRunIntervalSec, 0.1f, 2.0f);

    ImGui::Spacing();
    ImGui::Spacing();
    if (ImGui::Button("Reset Simulation", buttonSize)) {
        scheduler.reset();
        ui.selectedTaskId = -1;
        ui.autoRun = false;
    }

    ImGui::End();
}

static void drawCenterPanel(Scheduler& scheduler, UiState& ui) {
    ImGui::SetNextWindowPos(ImVec2(380, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1140, 550), ImGuiCond_FirstUseEver);
    ImGui::Begin("Task Table", nullptr, ImGuiWindowFlags_NoCollapse);

    // SizingStretchProp: columns stretch proportionally to fill the full
    // available width instead of being fixed-width, so the table always
    // occupies the entire panel and never clips long text.
    static ImGuiTableFlags flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingStretchProp;

    // Table spans the full width/height of the panel (0,0 = fill available space).
    if (ImGui::BeginTable("tasks", 6, flags, ImVec2(0, 0))) {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Remaining", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Waiting", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableHeadersRow();

        for (const auto& t : scheduler.allTasks()) {
            ImGui::TableNextRow(ImGuiTableRowFlags_None, 30.0f); // taller rows

            ImGui::TableSetColumnIndex(0);
            bool selected = (ui.selectedTaskId == t.id);
            char label[16];
            snprintf(label, sizeof(label), "%d", t.id);
            if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_SpanAllColumns)) {
                ui.selectedTaskId = t.id;
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(t.name.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%d%s", t.priority, t.boosted ? " (boosted)" : "");

            ImGui::TableSetColumnIndex(3);
            ImGui::TextColored(colorForState(t.state), "%s", TaskStateToString(t.state));

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%d", t.remainingTime);

            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%d", t.waitingTime);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

static void drawTaskIdList(Scheduler& scheduler, const char* title, const std::vector<int>& ids) {
    ImGui::SeparatorText(title);
    if (ids.empty()) {
        ImGui::TextDisabled("(empty)");
        return;
    }
    for (int id : ids) {
        for (const auto& t : scheduler.allTasks()) {
            if (t.id == id) {
                ImGui::TextColored(colorForState(t.state), "[%d] %s", t.id, t.name.c_str());
                break;
            }
        }
    }
}

static void drawRightPanel(Scheduler& scheduler) {
    ImGui::SetNextWindowPos(ImVec2(1520, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 550), ImGuiCond_FirstUseEver);
    ImGui::Begin("Queues", nullptr, ImGuiWindowFlags_NoCollapse);

    drawTaskIdList(scheduler, "Ready Queue", scheduler.readyQueueIds());
    ImGui::Spacing();
    drawTaskIdList(scheduler, "Waiting Queue", scheduler.waitingQueueIds());
    ImGui::Spacing();

    ImGui::SeparatorText("Running Task");
    ImGui::Spacing();
    int runningId = scheduler.runningTaskId();
    if (runningId == -1) {
        ImGui::TextDisabled("(CPU idle)");
    } else {
        for (const auto& t : scheduler.allTasks()) {
            if (t.id == runningId) {
                ImGui::TextColored(colorForState(TaskState::Running), "[%d] %s", t.id, t.name.c_str());
                break;
            }
        }
    }
    ImGui::Spacing();

    drawTaskIdList(scheduler, "Finished Tasks", scheduler.finishedIds());

    ImGui::End();
}

static void drawGanttPanel(Scheduler& scheduler) {
    ImGui::SetNextWindowPos(ImVec2(0, 930), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1920, 280), ImGuiCond_FirstUseEver);
    ImGui::Begin("Gantt Chart", nullptr, ImGuiWindowFlags_NoCollapse);

    const auto& history = scheduler.executionHistory();
    if (history.empty()) {
        ImGui::TextDisabled("No execution history yet - click Run One Tick or enable Auto Run to populate the chart.");
        ImGui::End();
        return;
    }

    // Collect one row per task, in the order each first appears in history.
    std::vector<int> rowIds;
    std::vector<std::string> rowNames;
    for (const auto& seg : history) {
        if (std::find(rowIds.begin(), rowIds.end(), seg.taskId) == rowIds.end()) {
            rowIds.push_back(seg.taskId);
            rowNames.push_back(seg.taskName);
        }
    }

    const float labelWidth = 100.0f;
    const float rowHeight = 30.0f;
    const float tickWidth = 28.0f;
    const float axisHeight = 24.0f;
    int maxTick = scheduler.currentTick();
    if (maxTick < 1) maxTick = 1;

    float chartContentWidth = maxTick * tickWidth + 20.0f;
    float rowsHeight = rowIds.size() * rowHeight;

    // --- Fixed (non-scrolling) label column on the left ---
    ImGui::BeginChild("gantt_labels", ImVec2(labelWidth, rowsHeight + axisHeight + 10), false);
    ImGui::Dummy(ImVec2(0, axisHeight)); // align first label with the chart's tick-axis row
    for (size_t r = 0; r < rowNames.size(); ++r) {
        ImGui::TextUnformatted(rowNames[r].c_str());
        ImGui::Dummy(ImVec2(0, rowHeight - ImGui::GetTextLineHeight()));
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // --- Horizontally scrollable timeline area on the right ---
    ImGui::BeginChild("gantt_chart", ImVec2(0, rowsHeight + axisHeight + 20), false, ImGuiWindowFlags_HorizontalScrollbar);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Reserve scrollable content size (drawing via ImDrawList doesn't do this automatically).
    ImGui::Dummy(ImVec2(chartContentWidth, rowsHeight + axisHeight));

    // Tick axis numbers + vertical gridlines.
    for (int t = 1; t <= maxTick; ++t) {
        float x = origin.x + (t - 1) * tickWidth;
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", t);
        draw->AddText(ImVec2(x + 2, origin.y), IM_COL32(170, 170, 170, 255), buf);
        draw->AddLine(ImVec2(x, origin.y + axisHeight), ImVec2(x, origin.y + axisHeight + rowsHeight),
                       IM_COL32(50, 50, 50, 255));
    }

    // Horizontal row separators.
    for (size_t r = 0; r <= rowIds.size(); ++r) {
        float y = origin.y + axisHeight + r * rowHeight;
        draw->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + chartContentWidth, y), IM_COL32(50, 50, 50, 255));
    }

    // One colored bar per execution segment, colored by task ID so the
    // same task always shows the same color across the whole chart.
    static const ImU32 palette[] = {
        IM_COL32(66, 133, 244, 255), IM_COL32(52, 168, 83, 255), IM_COL32(251, 188, 5, 255),
        IM_COL32(234, 67, 53, 255),  IM_COL32(154, 90, 255, 255), IM_COL32(0, 188, 212, 255),
        IM_COL32(255, 112, 67, 255), IM_COL32(141, 110, 99, 255),
    };
    for (const auto& seg : history) {
        auto it = std::find(rowIds.begin(), rowIds.end(), seg.taskId);
        if (it == rowIds.end()) continue;
        size_t r = static_cast<size_t>(std::distance(rowIds.begin(), it));
        float y0 = origin.y + axisHeight + r * rowHeight + 4;
        float y1 = y0 + rowHeight - 8;
        float x0 = origin.x + (seg.startTick - 1) * tickWidth;
        float x1 = origin.x + seg.endTick * tickWidth;
        ImU32 col = palette[seg.taskId % (sizeof(palette) / sizeof(palette[0]))];
        draw->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), col, 3.0f);
        draw->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(0, 0, 0, 160), 3.0f);

        // Show tick range as a label on wider bars only, to avoid clutter.
        if ((x1 - x0) > 30.0f) {
            char rangeBuf[24];
            snprintf(rangeBuf, sizeof(rangeBuf), "%d-%d", seg.startTick, seg.endTick);
            draw->AddText(ImVec2(x0 + 4, y0 + 2), IM_COL32(0, 0, 0, 220), rangeBuf);
        }
    }

    ImGui::EndChild();
    ImGui::End();
}

static void drawBottomPanel(Scheduler& scheduler) {
    ImGui::SetNextWindowPos(ImVec2(0, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1920, 330), ImGuiCond_FirstUseEver);
    ImGui::Begin("Execution Log", nullptr, ImGuiWindowFlags_NoCollapse);

    const auto& entries = scheduler.logger().entries();
    bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;

    if (ImGui::BeginChild("log_scroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
        for (const auto& entry : entries) {
            ImGui::TextColored(colorForLogEvent(entry.type), "[tick %d][%s]", entry.tick,
                                LogEventTypeToString(entry.type));
            ImGui::SameLine();
            ImGui::TextUnformatted(entry.message.c_str());
        }
        // Auto-scroll to the newest entry as new ticks are logged.
        if (atBottom) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

void draw(Scheduler& scheduler, UiState& ui, float deltaTimeSec) {
    // Handle Auto Run timing.
    if (ui.autoRun) {
        ui.autoRunTimer += deltaTimeSec;
        if (ui.autoRunTimer >= ui.autoRunIntervalSec) {
            ui.autoRunTimer = 0.0f;
            scheduler.tick();
        }
    }

    drawTopBar(scheduler);
    drawLeftPanel(scheduler, ui);
    drawCenterPanel(scheduler, ui);
    drawRightPanel(scheduler);
    drawBottomPanel(scheduler);
    drawGanttPanel(scheduler);
}

} // namespace GuiLayout
