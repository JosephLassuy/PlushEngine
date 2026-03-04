#pragma once
#include <unordered_map>
#include <vector>
struct App;

struct Schedule {
    std::vector<void (*)(App&)> functions;

    void addSystem(void (*run)(App&)) {
        functions.push_back(run);
    }
    void run(App& app) {
        for (auto& function : functions) {
            function(app);
        }
    }
};

enum ScheduleLabel {
    StartUp,
    Update,
};

struct Schedules {
    std::unordered_map<ScheduleLabel, Schedule> schedules;
    
    void addSystem(ScheduleLabel label, void (*run)(App&)) {
        schedules[label].addSystem(run);
    }
    void run(ScheduleLabel label, App& app) {
        schedules[label].run(app);
    }
};
