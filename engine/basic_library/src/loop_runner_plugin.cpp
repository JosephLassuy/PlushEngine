// loop_runner_plugin.h
#include "app.h"
#include "schedule.h"

void LoopRunner(App& app) {
    auto& schedules = app.schedules.schedules;
    schedules[ScheduleLabel::StartUp].run(app);
    while (true) {
        schedules[ScheduleLabel::Update].run(app);
    }
}