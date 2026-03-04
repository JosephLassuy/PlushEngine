#pragma once
#include "components.h"

struct Entity {
    int id;
    Components components;

    template <typename T>
    void addComponent(const T& value) {
      components.add(value);
    }
};