//GuiLayout.h



#pragma once
#include "Scheduler.h"

namespace GuiLayout {

struct UiState {
    char newTaskName[32] = "New-Task";
    int newTaskPriority = 3;
    int newTaskBurst = 5;
    int newTaskQuantum = 2;

    int selectedTaskId = -1;

    bool autoRun = false;
    float autoRunIntervalSec = 0.5f;
    float autoRunTimer = 0.0f;
};

void draw(Scheduler& scheduler, UiState& ui, float deltaTimeSec);

} // namespace GuiLayout
