#include <basic_library/basic_library.h>
#include <iostream>
#include <cstdlib>
#include <string>

void drawMenu() {
#ifdef _WIN32
    std::system("cls");
#else
    std::cout << "\033[2J\033[H";
#endif

    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║       PlushEngine Example Launcher  ║\n";
    std::cout << "╠══════════════════════════════════════╣\n";
    std::cout << "║  1. Flappybird                      ║\n";
    std::cout << "║  2. Exit                             ║\n";
    std::cout << "╚══════════════════════════════════════╝\n\n";
    std::cout << "Enter choice (1-2): ";
    std::cout.flush();
}

int main() {
    basic_library::Print("Launcher - PlushEngine Examples");

    while (true) {
        drawMenu();
        std::string input;
        std::getline(std::cin, input);

        if (input == "1") {
            std::cout << "\nLaunching Flappybird...\n\n";
#ifdef _WIN32
            int err = std::system("build\\Flappybird.exe");
#else
            int err = std::system("./build/Flappybird");
#endif
            if (err != 0) {
                std::cout << "Failed to run Flappybird. Build it first: cmake --build build\n";
            }
            std::cout << "\nPress Enter to return to menu...";
            std::cin.get();
        } else if (input == "2") {
            std::cout << "Goodbye!\n";
            return 0;
        }
    }
}
