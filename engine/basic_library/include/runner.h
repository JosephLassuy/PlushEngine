#pragma once

struct App;

struct Runner {
    void (*run)(App&) = nullptr;
};