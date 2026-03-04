#pragma once
#include "world.h"
#include "runner.h"
#include "schedule.h"
#include <typeindex>
#include <unordered_set>

struct App {
  World world;
  Runner runner;  
  Schedules schedules;
  std::unordered_set<std::type_index> installed_plugins;

  template <typename T>
  void addPlugin(T& plugin) {
      auto tid = std::type_index(typeid(T));
      if (installed_plugins.contains(tid)) {
          return;  // already added, skip
      }
      plugin.build(*this);
      installed_plugins.insert(tid);
  }
  void addSystem(ScheduleLabel label, void (*run)(App&)) {
    schedules.addSystem(label, run);
  }
  void run() {
    runner.run(*this);
  }  
};
