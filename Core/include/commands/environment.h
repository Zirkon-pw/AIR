#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <Arduino.h>

#define EEPROM_SIZE 512
#define MAX_ENV_VARS 20

struct EnvVar {
    String key;
    String value;
};


String getEnvVar(const String& key);
void setEnvVar(const String& key, const String& value);
void unsetEnvVar(const String& key);
void loadEnvVars();
void saveEnvVars();
void handleSetEnv(String args);
void handleGetEnv(String args);
void handleUnsetEnv(String args);
void handlePrintEnv();

extern EnvVar envVars[MAX_ENV_VARS];
extern int envVarCount;

#endif