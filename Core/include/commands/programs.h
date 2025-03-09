#ifndef PROGRAMS_H
#define PROGRAMS_H

#include "Arduino.h"

void handleEcho(String args);
void catFile(String path);
void handleCompile(String args);
void handleScript(String args);
uint8_t parseOpcode(String command);

#endif