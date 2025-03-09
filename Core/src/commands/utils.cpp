#include "commands/utils.h"
#include "commands/environment.h"

fs::File outputFile;
bool outputRedirected = false;
String currentDirectory = "/";

bool checkArgs(String args, int required) {
    int count = 0;
    for (size_t i = 0; i < args.length(); i++) {
        if (args[i] == ' ') count++;
    }
    return (count + 1) >= required;
}

void writeOutput(const String &text) {
    if (outputRedirected && outputFile) {
        outputFile.print(text);
    } else {
        Serial.print(text);
    }
}

String normalizePath(String path) {
    String result = path;
    int pos = 0;
    
    while ((pos = result.indexOf('$', pos)) != -1) {
        int endPos = pos + 1;
        bool braceFormat = false;
        
        if (result.charAt(endPos) == '{') {
            braceFormat = true;
            endPos++;
        }

        while (endPos < result.length() && 
              (isalnum(result.charAt(endPos)) || result.charAt(endPos) == '_')) {
            endPos++;
        }

        String varName;
        int replaceLength;
        if (braceFormat) {
            varName = result.substring(pos + 2, endPos);
            replaceLength = endPos - pos;
        } else {
            varName = result.substring(pos + 1, endPos);
            replaceLength = endPos - pos;
        }

        String varValue = getEnvVar(varName); // Используем getEnvVar из environment.h
        
        String newResult = result.substring(0, pos);
        newResult += varValue;
        newResult += result.substring(pos + replaceLength);
        result = newResult;
        pos += varValue.length();
    }

    // Обработка относительных путей
    if (result.startsWith("/")) return result;
    if (currentDirectory == "/") return "/" + result;
    return currentDirectory + "/" + result;
}

void printLastLines(String path, int lines) {
  fs::File file = LittleFS.open(path);
  if (!file) {
    writeOutput("Лог файл не найден\n");
    return;
  }
  // Здесь можно реализовать чтение последних строк файла (при необходимости)
  while (file.available()) {
    writeOutput(String((char)file.read()));
  }
  file.close();
}

