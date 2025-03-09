// =================== Заголовочный файл (console.h) ===================

#ifndef CONSOLE_H
#define CONSOLE_H

#include <WString.h>
#include <Arduino.h>
#include <vector>

// Расширенная структура команды с поддержкой тэгов
struct Command {
    String name;
    String args;
    std::vector<String> tags;
};

void initializeFS();
void handleCommand(String input);
void printHelp();
Command parseCommand(String input);

#endif  // CONSOLE_H