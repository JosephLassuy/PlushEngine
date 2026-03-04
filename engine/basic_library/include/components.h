#pragma once

#include <tuple>
#include <typeindex>
#include <vector>
#include <any>
struct Component {
    std::type_index type_id  = std::type_index(typeid(void));
    std::any data = std::any(nullptr);
};

struct Components {
    std::vector<Component> components;

    ~Components() { deinit(); }

    template <typename T>
    void add(const T& value) {
        components.emplace_back(
            std::type_index(typeid(T)),
            std::any(value)
        );
    }

    template <typename T>
    T* get() {
        auto tid = std::type_index(typeid(T));
        for (auto& c : components) {
            if (c.type_id == tid) {
                return std::any_cast<T>(&c.data);
            }
        }
        return nullptr;
    }

    template <typename... Ts>
    std::tuple<Ts*...> get_many() {
        return std::make_tuple(get<Ts>()...);
    }

    void deinit() {
        for (auto& c : components) {
            c.data.reset();
        }
        components.clear();
    }
};
