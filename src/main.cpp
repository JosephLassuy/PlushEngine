#include <basic_library/basic_library.h>
#include <app.h>
#include <loop_runner_plugin.h>

struct Health {
    float value;
    float max;
};

struct Mana {
    float value;
    float max;
};

void takeDamage(App& app) {
    auto healths = app.world.queryMany<Health>();
    for (auto& [health] : healths) {
        health->value = 23.0f;
        std::cout << "Take damage" << std::endl;
    }    
    auto healthManas= app.world.queryMany<Health, Mana>();
    for (auto& [health, mana] : healthManas) {
        health->value = 23.0f;
        mana->value = 23.0f;
        std::cout << "Take damage and mana" << std::endl;
    }

    std::cout << "Update: " << std::endl;

}

int main() {
    basic_library::Print("Hello, World!", "test");
    App app;
    auto entity = app.world.createEntity();
    entity->addComponent<Health>(Health{100.0f, 100.0f});
    auto entity2 = app.world.createEntity();
    entity2->addComponent<Health>(Health{100.0f, 100.0f});
    entity2->addComponent<Mana>(Mana{100.0f, 100.0f});
    app.addSystem(ScheduleLabel::Update, takeDamage);

    LoopRunnerPlugin loopRunner;
    app.addPlugin(loopRunner);
    app.run();
    return 0;
}

