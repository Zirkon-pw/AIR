#ifndef LOGS_H
#define LOGS_H

#include <Arduino.h>
#include <WString.h>

void handleInfoLog();
void handleErrLog();
void handleClearLog(String type);

#endif