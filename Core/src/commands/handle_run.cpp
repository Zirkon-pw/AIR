#include <commands/handle_run.h>
#include <commands/utils.h>
#include "vm.h"

VirtualMachine vm; 

void handleRun(String args) {
    
    // Загрузка программы из файла
    String path = normalizePath(args);
    File file = LittleFS.open(path, "r");
    if (!file) {
        writeOutput("File not found: " + path + "\n");
        return;
    }
    
    size_t size = file.size();
    uint8_t* program = new uint8_t[size];
    file.read(program, size);
    file.close();
    
    // Отладочный вывод загруженной программы
    Serial.println("Loaded program:");
    for (size_t i = 0; i < size; i++) {
        Serial.printf("%02X ", program[i]);
        if ((i + 1) % 16 == 0) Serial.println();
    }
    Serial.println();
    
    // Настройка ВМ
    vm.loadProgram(program, size);
    vm.run();
    vm.printState();
    
    delete[] program;
    writeOutput("Execution finished\n");
}