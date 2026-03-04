#pragma once
#include "app.h"

void LoopRunner(App& app);

struct LoopRunnerPlugin {
    void build(App& app) {
        app.runner.run = LoopRunner;
    }
};