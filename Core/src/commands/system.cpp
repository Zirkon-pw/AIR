#include <Arduino.h>
#include <commands/utils.h>
#include <commands/system.h>

void handleShutdown() {
    writeOutput("Система выключается...\n");
    ESP.deepSleep(0);
}

void handleReboot() {
    writeOutput("Перезагрузка системы...\n");
    ESP.restart();
}

void handleStatus() {
    String status = "Состояние системы:\n";
    status += "Версия ПО: 1.0\n";
    writeOutput(status);
}