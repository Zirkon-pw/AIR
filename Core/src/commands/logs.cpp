#include "commands/logs.h"
#include "commands/utils.h"
#include "commands/file_system.h"
#include <WString.h>


void handleInfoLog() {
    printLastLines("/system/outputs/info.log", 6);
}

void handleErrLog() {
    printLastLines("/system/outputs/error.log", 6);
}

void handleClearLog(String type) {
    if (type == "info" || type == "all") {
        LittleFS.remove("/system/outputs/info.log");
        createFile("/system/outputs/info.log");
    }
    if (type == "error" || type == "all") {
        LittleFS.remove("/system/outputs/error.log");
        createFile("/system/outputs/error.log");
    }
    writeOutput("Логи очищены\n");
}