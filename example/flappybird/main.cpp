#include <basic_library/basic_library.h>
#include <app.h>
#include <loop_runner_plugin.h>
#include <iostream>
#include <cstdlib>
#include <ctime>

struct Position { float x, y; };
struct Velocity { float vy; };
struct Bird {};
struct Pipe {};
struct Score { int value; };
struct PipeSpawnTimer { int count; };

constexpr int SCREEN_WIDTH = 40;
constexpr int SCREEN_HEIGHT = 15;
constexpr float GRAVITY = -0.8f;
constexpr float FLAP_STRENGTH = 1.2f;

void clearScreen() {
#ifdef _WIN32
    std::system("cls");
#else
    std::cout << "\033[2J\033[H";
#endif
}

void drawGame(App& app) {
    auto birds = app.world.queryMany<Position, Bird>();
    auto pipes = app.world.queryMany<Position, Pipe>();
    auto scores = app.world.queryMany<Score>();
    int score = scores.empty() ? 0 : std::get<0>(scores[0])->value;

    clearScreen();
    std::cout << "=== FLAPPYBIRD (Space=flap, Q=quit) === Score: " << score << "\n\n";

    for (int row = SCREEN_HEIGHT - 1; row >= 0; --row) {
        std::cout << "|";
        for (int col = 0; col < SCREEN_WIDTH; ++col) {
            char c = ' ';

            for (auto& [pos, _] : birds) {
                int bx = static_cast<int>(pos->x);
                int by = static_cast<int>(pos->y);
                if (bx == col && by == row) c = '@';
            }
            for (auto& [pos, _] : pipes) {
                int px = static_cast<int>(pos->x);
                if (px == col && (row <= 2 || row >= SCREEN_HEIGHT - 3)) c = '#';
            }
            std::cout << c;
        }
        std::cout << "|\n";
    }
    std::cout << "+----------------------------------------+\n";
    std::cout.flush();
}

void birdPhysics(App& app) {
    auto entities = app.world.queryMany<Position, Velocity, Bird>();
    for (auto& [pos, vel, _] : entities) {
        vel->vy += GRAVITY;
        pos->y += vel->vy;
        if (pos->y < 0) pos->y = 0;
        if (pos->y >= SCREEN_HEIGHT) pos->y = SCREEN_HEIGHT - 1;
    }
}

void pipeSpawner(App& app) {
    auto timers = app.world.queryMany<PipeSpawnTimer>();
    if (timers.empty()) return;
    auto& [timer] = timers[0];
    timer->count++;
    if (timer->count >= 8) {
        timer->count = 0;
        auto pipe = app.world.createEntity();
        pipe->addComponent<Position>(Position{static_cast<float>(SCREEN_WIDTH - 1), 0});
        pipe->addComponent<Pipe>(Pipe{});
    }
}

void pipeMovement(App& app) {
    auto pipes = app.world.queryMany<Position, Pipe>();
    for (auto& [pos, _] : pipes) {
        pos->x -= 1.0f;
    }
}

void collisionCheck(App& app) {
    auto birds = app.world.queryMany<Position, Bird>();
    auto pipes = app.world.queryMany<Position, Pipe>();

    for (auto& [bpos, _] : birds) {
        if (bpos->y <= 2 || bpos->y >= SCREEN_HEIGHT - 3) {
            std::cout << "\nGame Over! Hit a pipe. Final score: ";
            auto scores = app.world.queryMany<Score>();
            std::cout << (scores.empty() ? 0 : std::get<0>(scores[0])->value) << "\n";
            std::exit(0);
        }
        for (auto& [ppos, _] : pipes) {
            int bx = static_cast<int>(bpos->x);
            int px = static_cast<int>(ppos->x);
            if (bx == px) {
                std::cout << "\nGame Over! Hit a pipe. Final score: ";
                auto scores = app.world.queryMany<Score>();
                std::cout << (scores.empty() ? 0 : std::get<0>(scores[0])->value) << "\n";
                std::exit(0);
            }
        }
    }
}

void scoreUpdate(App& app) {
    auto birds = app.world.queryMany<Position, Bird>();
    auto pipes = app.world.queryMany<Position, Pipe>();
    auto scores = app.world.queryMany<Score>();
    Score* scorePtr = scores.empty() ? nullptr : std::get<0>(scores[0]);

    for (auto& [bpos, _] : birds) {
        for (auto& [ppos, _] : pipes) {
            if (static_cast<int>(ppos->x) < static_cast<int>(bpos->x) && static_cast<int>(ppos->x) >= static_cast<int>(bpos->x) - 2) {
                if (scorePtr) scorePtr->value++;
            }
        }
    }
}

void processInput(App&) {
}

void flappybirdSetup(App& app) {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    auto bird = app.world.createEntity();
    bird->addComponent<Position>(Position{5.0f, SCREEN_HEIGHT / 2.0f});
    bird->addComponent<Velocity>(Velocity{0.0f});
    bird->addComponent<Bird>(Bird{});

    auto scoreEntity = app.world.createEntity();
    scoreEntity->addComponent<Score>(Score{0});

    auto timerEntity = app.world.createEntity();
    timerEntity->addComponent<PipeSpawnTimer>(PipeSpawnTimer{0});
}

struct FlappybirdRunner {
    static void run(App& app) {
        auto& schedules = app.schedules.schedules;
        schedules[ScheduleLabel::StartUp].run(app);

        while (true) {
            std::cout << "Press SPACE to flap, Q to quit: ";
            char c = static_cast<char>(std::cin.get());
            if (c == 'q' || c == 'Q') return;

            if (c == ' ' || c == '\n') {
                auto entities = app.world.queryMany<Position, Velocity, Bird>();
                for (auto& [pos, vel, _] : entities) {
                    vel->vy = FLAP_STRENGTH;
                }
            }

            schedules[ScheduleLabel::Update].run(app);
        }
    }
};

int main() {
    basic_library::Print("Flappybird Example - PlushEngine");
    App app;

    app.addSystem(ScheduleLabel::StartUp, flappybirdSetup);
    app.addSystem(ScheduleLabel::Update, processInput);
    app.addSystem(ScheduleLabel::Update, pipeSpawner);
    app.addSystem(ScheduleLabel::Update, birdPhysics);
    app.addSystem(ScheduleLabel::Update, pipeMovement);
    app.addSystem(ScheduleLabel::Update, scoreUpdate);
    app.addSystem(ScheduleLabel::Update, collisionCheck);
    app.addSystem(ScheduleLabel::Update, drawGame);

    app.runner.run = FlappybirdRunner::run;
    app.run();
    return 0;
}
