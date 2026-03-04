#pragma once
#include "entity.h"
#include <memory>
#include <vector>
#include <tuple>

struct World {
  std::vector<std::unique_ptr<Entity>> entities;
  int next_entity_id = 0;
  World() {}
  ~World() {}

  Entity* createEntity() {
    entities.emplace_back(std::make_unique<Entity>());
    entities.back().get()->id = next_entity_id;
    next_entity_id++;
    return entities.back().get();
  }

  template <typename T>
  std::vector<T*> query() {
    std::vector<T*> results;
    for (auto& entity : entities) {
      T* ptr = entity->components.get<T>();
      if (ptr) {
        results.push_back(ptr);
      }
    }
    return results;
  }

  template <typename... Ts>
  std::vector<std::tuple<Ts*...>> queryMany() {
    std::vector<std::tuple<Ts*...>> results;
    for (auto& entity : entities) {
      auto components = entity->components.get_many<Ts...>();
      bool all_present = std::apply(
        [](auto*... ptrs) { return (... && (ptrs != nullptr)); },
        components
      );
      if (all_present) {
        results.push_back(components);
      }
    }
    return results;
  }
};
