#include <Arduino.h>
#include <EEPROM.h>
#include "commands/utils.h"
#include "commands/environment.h"

EnvVar envVars[MAX_ENV_VARS];
int envVarCount = 0;

void handleSetEnv(String args) {
    int spacePos = args.indexOf(' ');
    if (spacePos == -1) {
        writeOutput("Использование: setenv <key> <value>\n");
        return;
    }
    String key = args.substring(0, spacePos);
    String value = args.substring(spacePos + 1);
    setEnvVar(key, value);
}

void handleGetEnv(String args) {
    String value = getEnvVar(args);
    writeOutput(value + "\n");
}

void handleUnsetEnv(String args) {
    unsetEnvVar(args);
}

void handlePrintEnv() {
    for (int i = 0; i < envVarCount; i++) {
        writeOutput(envVars[i].key + "=" + envVars[i].value + "\n");
    }
}

String getEnvVar(const String& key) {
    for (int i = 0; i < envVarCount; i++) {
        if (envVars[i].key == key) return envVars[i].value;
    }
    return "";
}

void setEnvVar(const String& key, const String& value) {
    for (int i = 0; i < envVarCount; i++) {
        if (envVars[i].key == key) {
            envVars[i].value = value;
            saveEnvVars();
            return;
        }
    }

    if (envVarCount < MAX_ENV_VARS) {
        envVars[envVarCount++] = {key, value};
        saveEnvVars();
    }
}

void unsetEnvVar(const String& key) {
    for (int i = 0; i < envVarCount; i++) {
        if (envVars[i].key == key) {
            for (int j = i; j < envVarCount - 1; j++) {
                envVars[j] = envVars[j + 1];
            }
            envVarCount--;
            saveEnvVars();
            return;
        }
    }
}

void loadEnvVars() {
    String envData;
    for (int i = 0; i < EEPROM_SIZE; i++) {
        char c = EEPROM.read(i);
        if (c == 0) break;
        envData += c;
    }

    int pos = 0;
    while (pos < envData.length()) {
        int eqPos = envData.indexOf('=', pos);
        if (eqPos == -1) break;
        
        int endPos = envData.indexOf(';', eqPos);
        if (endPos == -1) endPos = envData.length();

        String key = envData.substring(pos, eqPos);
        String value = envData.substring(eqPos + 1, endPos);
        
        if (envVarCount < MAX_ENV_VARS) {
            envVars[envVarCount++] = {key, value};
        }
        pos = endPos + 1;
    }
}

void saveEnvVars() {
    String envData;
    for (int i = 0; i < envVarCount; i++) {
        envData += envVars[i].key + "=" + envVars[i].value + ";";
        if (envData.length() > EEPROM_SIZE - 2) break;
    }

    for (int i = 0; i < EEPROM_SIZE; i++) {
        if (i < envData.length()) {
            EEPROM.write(i, envData[i]);
        } else {
            EEPROM.write(i, 0);
        }
    }
    EEPROM.commit();
}