#include <commands/utils.h>
#include <commands/file_system.h>
#include <FS.h>
#include <EEPROM.h>

// =================== Функции работы с файловой системой (LittleFS) ===================

// Рекурсивное отображение дерева директорий.
// Для каждого уровня выводится отступ, затем имя директории или файла.
void printTree(const String &path, int depth) {
    String prefix;
    for (int i = 0; i < depth; i++) {
        prefix += "    ";
    }

    // Если path не пуст, нормализуем его; иначе берём текущую директорию.
    String targetPath = (path.length() > 0) ? normalizePath(path) : currentDirectory;
    fs::File dir = LittleFS.open(targetPath);
    if (!dir || !dir.isDirectory()) {
        writeOutput(prefix + "├── [Ошибка открытия: " + targetPath + "]\n");
        return;
    }

    writeOutput(prefix + "└── " + String(dir.name()) + "\n");

    fs::File file = dir.openNextFile();
    while (file) {
        // Получаем полное имя, но извлекаем только базовую часть (последнее имя после '/')
        String fullName = file.name();
        String baseName = fullName;
        int lastSlash = fullName.lastIndexOf('/');
        if (lastSlash >= 0) {
            baseName = fullName.substring(lastSlash + 1);
        }

        if (file.isDirectory()) {
            // Формируем корректный путь для поддиректории:
            // используем targetPath (родительский путь) + "/" + базовое имя поддиректории
            String subPath = targetPath;
            if (!subPath.endsWith("/")) {
                subPath += "/";
            }
            subPath += baseName;
            printTree(subPath, depth + 1);
        } else {
            writeOutput(prefix + "    └── " + baseName + "\n");
        }
        file = dir.openNextFile();
    }
    dir.close();
}


// Вывод списка файлов и директорий в указанном каталоге
void listFiles(const String &path) {
  String targetPath = (path.length() > 0) ? normalizePath(path) : currentDirectory;
  fs::File dir = LittleFS.open(targetPath);
  if (!dir || !dir.isDirectory()) {
    writeOutput("Директория не найдена: " + targetPath + "\n");
    return;
  }

  fs::File file = dir.openNextFile();
  while (file) {
    String entry = String(file.name()) + "\t" +
                   (file.isDirectory() ? "[DIR]" : String(file.size()) + " байт");
    writeOutput(entry + "\n");
    file = dir.openNextFile();
  }
  dir.close();
}

// Создание файла по указанному пути
void createFile(const String &path) {
  String fullPath = normalizePath(path);
  fs::File file = LittleFS.open(fullPath, FILE_WRITE);
  if (file) {
    writeOutput("Файл создан: " + fullPath + "\n");
    file.close();
  } else {
    writeOutput("Ошибка создания файла: " + fullPath + "\n");
  }
}

// Удаление файла
void deleteFile(const String &path) {
  String fullPath = normalizePath(path);
  if (LittleFS.remove(fullPath)) {
    writeOutput("Файл удален: " + fullPath + "\n");
  } else {
    writeOutput("Ошибка удаления файла: " + fullPath + "\n");
  }
}

// Создание директории
void createDir(const String &path) {
  String fullPath = normalizePath(path);
  if (LittleFS.mkdir(fullPath)) {
    writeOutput("Директория создана: " + fullPath + "\n");
  } else {
    writeOutput("Ошибка создания директории: " + fullPath + "\n");
  }
}

// Удаление директории (работает только для пустых директорий)
void deleteDir(const String &path) {
  String fullPath = normalizePath(path);
  if (LittleFS.rmdir(fullPath)) {
    writeOutput("Директория удалена: " + fullPath + "\n");
  } else {
    writeOutput("Ошибка удаления директории: " + fullPath + "\n");
  }
}

// Изменение текущей директории.
// Если указываем "..", переходим на уровень выше (с учетом того, что корневой каталог — "/")
void changeDir(const String &path) {
  if (path == "..") {
    if (currentDirectory != "/") {
      int lastSlash = currentDirectory.lastIndexOf('/');
      if (lastSlash <= 0) {
        currentDirectory = "/";
      } else {
        currentDirectory = currentDirectory.substring(0, lastSlash);
      }
      writeOutput("Текущая директория: " + currentDirectory + "\n");
    } else {
      writeOutput("Уже на корневой директории\n");
    }
    return;
  }
  String newPath = normalizePath(path);
  fs::File dir = LittleFS.open(newPath);
  if (dir && dir.isDirectory()) {
    currentDirectory = newPath;
    writeOutput("Текущая директория изменена на: " + currentDirectory + "\n");
  } else {
    writeOutput("Директория не существует: " + newPath + "\n");
  }
  if (dir) { dir.close(); }
}

// Копирование файла.
// Параметр args должен содержать два пути, разделённых пробелом: "<источник> <назначение>"
void copyFile(const String &args) {
  int spacePos = args.indexOf(' ');
  if (spacePos == -1) {
    writeOutput("Неверный формат команды копирования. Используйте: copy <источник> <назначение>\n");
    return;
  }
  String sourcePath = normalizePath(args.substring(0, spacePos));
  String destPath   = normalizePath(args.substring(spacePos + 1));

  fs::File source = LittleFS.open(sourcePath, FILE_READ);
  if (!source) {
    writeOutput("Файл источника не найден: " + sourcePath + "\n");
    return;
  }
  fs::File dest = LittleFS.open(destPath, FILE_WRITE);
  if (!dest) {
    writeOutput("Ошибка создания файла назначения: " + destPath + "\n");
    source.close();
    return;
  }

  const size_t bufSize = 512;
  uint8_t buffer[bufSize];
  while (source.available()) {
    size_t bytesRead = source.read(buffer, bufSize);
    dest.write(buffer, bytesRead);
  }
  source.close();
  dest.close();
  writeOutput("Файл скопирован: " + sourcePath + " -> " + destPath + "\n");
}

// Перемещение файла (копирование + удаление исходного файла)
// Параметр args: "<источник> <назначение>"
void moveFile(const String &args) {
  int spacePos = args.indexOf(' ');
  if (spacePos == -1) {
    writeOutput("Неверный формат команды перемещения. Используйте: move <источник> <назначение>\n");
    return;
  }
  String sourcePath = normalizePath(args.substring(0, spacePos));
  String destPath   = normalizePath(args.substring(spacePos + 1));

  // Сначала копируем файл
  copyFile(args);

  // Если файл назначения создан, удаляем исходный файл
  fs::File destCheck = LittleFS.open(destPath, FILE_READ);
  if (destCheck) {
    destCheck.close();
    deleteFile(sourcePath);
  } else {
    writeOutput("Ошибка перемещения файла: не удалось создать файл назначения.\n");
  }
}

// Форматирование размера в человеко-читаемый вид (байт, KB, MB, GB)
String formatSize(size_t bytes) {
  const char* units[] = {"байт", "KB", "MB", "GB"};
  size_t unitIndex = 0;
  double size = bytes;
  while (size >= 1024 && unitIndex < 3) {
    size /= 1024;
    unitIndex++;
  }
  return String(size, 2) + " " + units[unitIndex];
}

// Вывод информации о файловой системе
void printFSInfo() {
  size_t total = LittleFS.totalBytes();
  size_t used  = LittleFS.usedBytes();
  String info = "Файловая система:\n";
  info += "Всего: " + formatSize(total) + "\n";
  info += "Использовано: " + formatSize(used) + "\n";
  info += "Свободно: " + formatSize(total - used) + "\n";
  writeOutput(info);
}

// Вывод текущей директории
void printWorkingDir() {
  writeOutput("Текущая директория: " + currentDirectory + "\n");
}

// Запись содержимого в файл. Параметр mode позволяет задать режим открытия (например, "w" для записи, "a" для дозаписи)
void writeToFile(const String &path, const String &content, const char* mode ) {
  String fullPath = normalizePath(path);
  fs::File file = LittleFS.open(fullPath, mode);
  if (file) {
    file.print(content);
    file.close();
    writeOutput("Запись в файл " + fullPath + " выполнена успешно.\n");
  } else {
    writeOutput("Ошибка записи в файл: " + fullPath + "\n");
  }
}

// =================== Конец интегрированного кода ===================
