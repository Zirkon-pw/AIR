#include "console.h"
#include "command_includes.h"
#include <map>
#include <functional>
#include <vm.h>
#include <EEPROM.h>

// Обратите внимание: структура Command уже объявлена в console.h, поэтому повторно её не определяем здесь.

// =================== Инициализация файловой системы ===================

void initializeFS() {
  Serial.println("\nИнициализация LittleFS...");
  if (!LittleFS.begin(true)) {
    Serial.println("Ошибка монтирования, пробуем форматировать...");
    if (!LittleFS.format()) {
      Serial.println("Ошибка форматирования!");
      return;
    }
    if (!LittleFS.begin(true)) {
      Serial.println("Повторная ошибка монтирования!");
      return;
    }
  }
  EEPROM.begin(EEPROM_SIZE);
  loadEnvVars();  // Загрузка переменных окружения из EEPROM

  // Создание структуры каталогов (убран путь, который был указан как файл)
  const char* dirs[] = {
    "/system", "/system/outputs", "/config",
    "/utils", "/utils/scripts", "/utils/tools", 
    "/home"
  };

  for (const char* dir : dirs) {
    if (!LittleFS.exists(dir)) {
      if (!LittleFS.mkdir(dir)) {
        Serial.println("Ошибка создания директории: " + String(dir));
      }
    }
  }

  String prog = "";
  prog += "PUSH 10\n";
  prog += "PUSH 20\n";
  prog += "ADD\n";
  prog += "STORE 0x01\n";
  prog += "LOAD 0x01\n";
  prog += "SYSCALL\n";
  prog += "HALT\n";

  // Создание базовых конфигурационных файлов
  const char* files[] = {
    "prog",
    "/system/board.conf",
    "/system/outputs/info.log",
    "/system/outputs/error.log",
    "/system/settings.conf",
    "/system/device_info.conf",
    "/config/wifi.conf",
    "/config/wifi_list.conf",
    "/config/port_init.conf",
    "/config/interface_init.conf"
  };

  for (const char* file : files) {
    if (!LittleFS.exists(file)) {
      fs::File f = LittleFS.open(file, FILE_WRITE);
      if (!f) {
        Serial.println("Ошибка создания файла: " + String(file));
      } else {
        f.close();
      }
    }
  }
  
  // Если файла wifi.conf нет – создать его с настройками по умолчанию
  if (!LittleFS.exists("/config/wifi.conf")) {
    WifiConfig defaultConfig;
    defaultConfig.createMode = false;
    defaultConfig.ssid = "";
    defaultConfig.password = "";
    defaultConfig.channel = 1;
    writeWifiConfig(defaultConfig);
  }
  // Если файла systemdata.dat нет – создать пустой файл
  if (!LittleFS.exists("/system/systemdata.dat")) {
    fs::File f = LittleFS.open("/system/systemdata.dat", "w");
    f.close();
  }
  writeToFile("prog", prog, "a");
  Serial.println("Файловая система готова\n");
}

// =================== Разбор команды ===================

// Функция разбора команды: первая часть – имя, далее каждый токен, начинающийся с '-',
// считается тэгом, остальные объединяются в строку аргументов.
Command parseCommand(String input) {
    Command cmd;
    input.trim();
    if (input.length() == 0) return cmd;

    std::vector<String> tokens;
    int start = 0;
    while (start < input.length()) {
        int spaceIndex = input.indexOf(' ', start);
        String token;
        if (spaceIndex == -1) {
            token = input.substring(start);
            start = input.length();
        } else {
            token = input.substring(start, spaceIndex);
            start = spaceIndex + 1;
        }
        token.trim();
        if (token.length() > 0) {
            tokens.push_back(token);
        }
    }

    if (!tokens.empty()) {
        cmd.name = tokens[0];
        String argsStr = "";
        for (size_t i = 1; i < tokens.size(); i++) {
            if (tokens[i].startsWith("-")) {
                // Токены, начинающиеся с '-', считаем тэгами
                cmd.tags.push_back(tokens[i]);
            } else {
                if (argsStr.length() > 0) argsStr += " ";
                argsStr += tokens[i];
            }
        }
        cmd.args = argsStr;
    }
    return cmd;
}

// =================== Обработка команды ===================

// Обработка команды с поддержкой перенаправления вывода (">" или ">>")
void handleCommand(String input) {
    int redirectPos = input.indexOf(">");
    String outputFilename;
    int mode = 0;

    if (redirectPos != -1) {
        if (input.charAt(redirectPos + 1) == '>') {
            mode = 2;
            outputFilename = input.substring(redirectPos + 2);
        } else {
            mode = 1;
            outputFilename = input.substring(redirectPos + 1);
        }
        input = input.substring(0, redirectPos);
        outputFilename.trim();
        input.trim();

        if (outputFilename.length() > 0) {
            String fullPath = normalizePath(outputFilename);
            outputFile = LittleFS.open(fullPath, (mode == 1) ? FILE_WRITE : FILE_APPEND);
            outputRedirected = true;
        }
    }

    Command cmd = parseCommand(input);

    // Карта команд с обработчиками.
    // Для функций, которые не поддерживают второй параметр, передаём только cmd.args.
    std::map<String, std::function<void(const Command&)>> commandHandlers = {
        {"ls",         [](const Command &cmd) { listFiles(cmd.args); }},
        {"cat",        [](const Command &cmd) { if (checkArgs(cmd.args, 1)) catFile(cmd.args); }},
        {"touch",      [](const Command &cmd) { if (checkArgs(cmd.args, 1)) createFile(cmd.args); }},
        {"echo",       [](const Command &cmd) { handleEcho(cmd.args); }},
        {"rm",         [](const Command &cmd) { if (checkArgs(cmd.args, 1)) deleteFile(cmd.args); }},
        {"mkdir",      [](const Command &cmd) { if (checkArgs(cmd.args, 1)) createDir(cmd.args); }},
        {"rmdir",      [](const Command &cmd) { if (checkArgs(cmd.args, 1)) deleteDir(cmd.args); }},
        {"cd",         [](const Command &cmd) { if (checkArgs(cmd.args, 1)) changeDir(cmd.args); }},
        {"pwd",        [](const Command &) { printWorkingDir(); }},
        {"tree",       [](const Command &cmd) { printTree(cmd.args); }},
        {"info",       [](const Command &) { printFSInfo(); }},
        {"cp",         [](const Command &cmd) { if (checkArgs(cmd.args, 2)) copyFile(cmd.args); }},
        {"mv",         [](const Command &cmd) { if (checkArgs(cmd.args, 2)) moveFile(cmd.args); }},
        {"setenv",     [](const Command &cmd) { handleSetEnv(cmd.args); }},
        {"getenv",     [](const Command &cmd) { handleGetEnv(cmd.args); }},
        {"unsetenv",   [](const Command &cmd) { handleUnsetEnv(cmd.args); }},
        {"printenv",   [](const Command &) { handlePrintEnv(); }},
        {"shutdown",   [](const Command &) { handleShutdown(); }},
        {"reboot",     [](const Command &) { handleReboot(); }},
        {"status",     [](const Command &) { handleStatus(); }},
        {"skript",     [](const Command &cmd) { handleScript(cmd.args); }},
        {"run",        [](const Command &cmd) { handleRun(cmd.args); }},
        {"infolog",    [](const Command &) { handleInfoLog(); }},
        {"errlog",     [](const Command &) { handleErrLog(); }},
        {"clear",      [](const Command &) { handleClearLog("all"); }},
        {"clearinfolog",[](const Command &) { handleClearLog("info"); }},
        {"clearerrlog",[](const Command &) { handleClearLog("error"); }},
        {"wifi",       [](const Command &cmd) { handleWifi(cmd.args); }},
        {"wifimode",   [](const Command &cmd) { handleWifiMode(cmd.args); }},
        {"wificreate", [](const Command &cmd) { handleWifiCreate(cmd.args); }},
        {"wificonnect",[](const Command &cmd) { handleWifiConnect(cmd.args); }},
        {"wifiinfo",   [](const Command &) { handleWifiInfo(); }},
        {"wifilist",   [](const Command &) { handleWifiList(); }},
        {"wifiremove", [](const Command &cmd) { handleWifiRemove(cmd.args); }},
        {"compile",    [](const Command &cmd) { handleCompile(cmd.args); }},
        {"help",       [](const Command &) { printHelp(); }}
    };

    if (commandHandlers.find(cmd.name) != commandHandlers.end()) {
        commandHandlers[cmd.name](cmd);
    } else {
        writeOutput("Неизвестная команда\n");
    }

    if (outputRedirected && outputFile) {
        outputFile.close();
        outputRedirected = false;
    }
}

// =================== Справка по командам ===================

// Функция справки (printHelp) в файле src/console.cpp
void printHelp() {
    String helpText;
    helpText += "\n=== Справка по командам ===\n\n";

    helpText += "Файловая система:\n";
    helpText += "  ls [path]             - список файлов и каталогов\n";
    helpText += "  cat <file>            - показать содержимое файла\n";
    helpText += "  touch <file>          - создать пустой файл\n";
    helpText += "  echo <text>           - вывести текст (также можно записать в файл через '>')\n";
    helpText += "  rm <file>             - удалить файл\n";
    helpText += "  mkdir <dir>           - создать директорию\n";
    helpText += "  rmdir <dir>           - удалить директорию\n";
    helpText += "  cd <dir>              - сменить текущую директорию\n";
    helpText += "  pwd                   - показать текущую директорию\n";
    helpText += "  tree [path]           - показать дерево файловой системы\n";
    helpText += "  info                  - информация о файловой системе\n";
    helpText += "  cp <src> <dst>        - копировать файл\n";
    helpText += "  mv <src> <dst>        - переместить файл\n\n";

    helpText += "Переменные окружения:\n";
    helpText += "  setenv <key> <value>  - установить переменную окружения\n";
    helpText += "  getenv <key>          - получить значение переменной\n";
    helpText += "  unsetenv <key>        - удалить переменную\n";
    helpText += "  printenv              - вывести список всех переменных\n\n";

    helpText += "Системные команды:\n";
    helpText += "  shutdown              - выключить систему\n";
    helpText += "  reboot                - перезагрузить систему\n";
    helpText += "  status                - показать состояние системы\n\n";

    helpText += "Скрипты и программы:\n";
    helpText += "  skript <file>         - выполнить скрипт\n";
    helpText += "  run <file>            - запустить программу\n";
    helpText += "  compile <file> <code> - создать бинарный файл из текстового байт-кода\n\n";

    helpText += "Логи:\n";
    helpText += "  infolog               - показать лог информационных сообщений\n";
    helpText += "  errlog                - показать лог сообщений об ошибках\n";
    helpText += "  clear                 - очистить все логи\n";
    helpText += "  clearinfolog          - очистить лог информационных сообщений\n";
    helpText += "  clearerrlog           - очистить лог ошибок\n\n";

    helpText += "WiFi:\n";
    helpText += "  wifi <ssid> <pass>      - добавить сеть (опционально с дополнительными тегами)\n";
    helpText += "  wifimode <create|connect> - установить режим работы WiFi\n";
    helpText += "  wificreate <ssid> <pass> [channel] - настроить точку доступа\n";
    helpText += "  wificonnect <ssid>      - подключиться к сети\n";
    helpText += "  wifiinfo                - показать текущие настройки WiFi\n";
    helpText += "  wifilist                - показать список известных сетей\n";
    helpText += "  wifiremove <ssid>       - удалить сеть из списка\n\n";

    helpText += "Прочее:\n";
    helpText += "  help                  - показать эту справку\n\n";

    writeOutput(helpText);
}

