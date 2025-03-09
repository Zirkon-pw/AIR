#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <Arduino.h>
#include <FS.h>

// Объявления функций с использованием передачи по константной ссылке
void printTree(const String &path, int depth = 0);
void listFiles(const String &path);
void createFile(const String &path);
void deleteFile(const String &path);
void createDir(const String &path);
void deleteDir(const String &path);
void changeDir(const String &path);
void copyFile(const String &args);
void moveFile(const String &args);
String formatSize(size_t bytes);
void printFSInfo();
void printWorkingDir();
void writeToFile(const String &path, const String &content, const char* mode = "w");

#endif // FILE_SYSTEM_H
