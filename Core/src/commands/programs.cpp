#include "commands/programs.h"
#include "Arduino.h"
#include "console.h"
#include "commands/utils.h"
#include "commands/environment.h"
#include "vm.h"

void handleScript(String args) {
    String path = normalizePath(args);
    fs::File file = LittleFS.open(path);
    
    if (!file) {
        writeOutput("Скрипт не найден!\n");
        return;
    }
    
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            handleCommand(line);
        }
    }
    file.close();
}

uint8_t parseOpcode(String line) {
    if (line.startsWith("push")) {
        return OP_PUSH;  // Это должна быть команда с опкодом 0x30
    } else if (line.startsWith("POP")) {
        return OP_POP;   // Опкод для pop
    } else if (line.startsWith("ADD")) {
        return OP_ADD;   // Опкод для add
    } else if (line.startsWith("SUB")) {
        return OP_SUB;   // Опкод для sub
    } else if (line.startsWith("MUL")) {
        return OP_MUL;   // Опкод для mul
    } else if (line.startsWith("DIV")) {
        return OP_DIV;   // Опкод для div
    } else if (line.startsWith("STORE")) {
        return OP_STORE; // Опкод для store
    } else if (line.startsWith("LOAD")) {
        return OP_LOAD;  // Опкод для load
    } else if (line.startsWith("HALT")) {
        return OP_HALT;  // Опкод для halt
    } else if (line.startsWith("SYSCALL")) {
        return OP_SYSCALL; // Опкод для syscall
    }
    return 0xFF;  // Возвращаем "неизвестную команду"
}

void handleCompile(String args) {
    int firstSpace = args.indexOf(' ');
    if (firstSpace == -1) {
        writeOutput("Ошибка: некорректные аргументы.\n");
        return;
    }

    // Разделяем строку на две части вручную, используя индексы
    String inputFilePath = args.substring(0, firstSpace);
    String outputFilePath = args.substring(firstSpace + 1);

    inputFilePath.trim();
    outputFilePath.trim();

    String fullInputPath = normalizePath(inputFilePath);
    File inputFile = LittleFS.open(fullInputPath, "r");
    if (!inputFile) {
        writeOutput("Ошибка открытия файла: " + fullInputPath + "\n");
        return;
    }

    uint8_t buffer[MEM_SIZE];
    size_t bufferSize = 0;

    while (inputFile.available() && bufferSize < MEM_SIZE) {
        String line = inputFile.readStringUntil('\n');
        line.trim();

        if (line.length() == 0 || line.startsWith("#")) {
            continue;
        }

        uint8_t bytecode = parseOpcode(line);
        if (bytecode != 0xFF) {
            buffer[bufferSize++] = bytecode;

            if (line.startsWith("push")) {
                String value = line.substring(5); // Извлекаем число после 'push'
                value.trim();
                buffer[bufferSize++] = value.toInt(); // Преобразуем в целое число
            } else if (line.startsWith("store") || line.startsWith("load")) {
                String address = line.substring(6); // Извлекаем адрес после 'store' или 'load'
                address.trim();
                buffer[bufferSize++] = strtol(address.c_str(), NULL, 16); // Преобразуем в число
            }
        } else {
            writeOutput("Неизвестная команда: " + line + "\n");
        }
    }

    inputFile.close();

    String fullOutputPath = normalizePath(outputFilePath);
    File outputFile = LittleFS.open(fullOutputPath, "w");
    if (!outputFile) {
        writeOutput("Ошибка создания файла: " + fullOutputPath + "\n");
        return;
    }

    outputFile.write(buffer, bufferSize);
    outputFile.close();

    writeOutput("Файл создан: " + fullOutputPath + "\n");
    writeOutput("Размер: " + String(bufferSize) + " байт\n");
}


void catFile(String path) {
  String fullPath = normalizePath(path);
  fs::File file = LittleFS.open(fullPath);
  if (!file) {
    writeOutput("Файл не найден!\n");
    return;
  }
  while (file.available()) {
    writeOutput(String((char)file.read()));
  }
  writeOutput("\n");
  file.close();
}

// Вспомогательная структура для хранения токена
struct EchoToken {
  String value;
  bool singleQuoted;  // true, если токен заключён в одинарные кавычки
};

// Функция токенизации строки для команды echo с сохранением кавычек
std::vector<EchoToken> tokenizeEcho(const String &input) {
  std::vector<EchoToken> tokens;
  EchoToken current;
  current.value = "";
  current.singleQuoted = false;

  enum State { NORMAL, IN_SINGLE, IN_DOUBLE } state = NORMAL;

  for (int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    switch (state) {
      case NORMAL:
        if (isspace(c)) {
          if (current.value.length() > 0) {
            tokens.push_back(current);
            current.value = "";
            current.singleQuoted = false;
          }
        } else if (c == '\'') {
          // Добавляем открывающую одинарную кавычку и переходим в режим IN_SINGLE
          current.value += c;
          state = IN_SINGLE;
          current.singleQuoted = true;
        } else if (c == '\"') {
          // Добавляем открывающую двойную кавычку и переходим в режим IN_DOUBLE
          current.value += c;
          state = IN_DOUBLE;
        } else if (c == '\\') {
          if (i + 1 < input.length()) {
            // Обработка escape-последовательности вне кавычек
            current.value += input.charAt(i + 1);
            i++;
          }
        } else if (c == '>') {
          // Если накоплен токен, сохраняем его перед оператором перенаправления
          if (current.value.length() > 0) {
            tokens.push_back(current);
            current.value = "";
            current.singleQuoted = false;
          }
          // Если следующий символ тоже '>', то это оператор append
          if (i + 1 < input.length() && input.charAt(i + 1) == '>') {
            EchoToken op;
            op.value = ">>";
            op.singleQuoted = false;
            tokens.push_back(op);
            i++;
          } else {
            EchoToken op;
            op.value = ">";
            op.singleQuoted = false;
            tokens.push_back(op);
          }
        } else {
          current.value += c;
        }
        break;

      case IN_SINGLE:
        // В одинарных кавычках всё воспринимается буквально
        current.value += c;
        if (c == '\'') {
          state = NORMAL;
        }
        break;

      case IN_DOUBLE:
        if (c == '\"') {
          current.value += c;
          state = NORMAL;
        } else if (c == '\\') {
          if (i + 1 < input.length()) {
            char next = input.charAt(i + 1);
            // Обработка распространённых escape-последовательностей в двойных кавычках
            switch (next) {
              case 'n': current.value += '\n'; break;
              case 't': current.value += '\t'; break;
              case 'r': current.value += '\r'; break;
              case '\\': current.value += '\\'; break;
              case '\"': current.value += '\"'; break;
              default: current.value += next; break;
            }
            i++;
          }
        } else {
          current.value += c;
        }
        break;
    }
  }
  if (current.value.length() > 0) {
    tokens.push_back(current);
  }
  return tokens;
}

// Функция для развёртки переменных окружения внутри строки (заменяет $VAR на значение)
String expandVariables(const String &s) {
  String result = "";
  for (int i = 0; i < s.length(); i++) {
    char c = s.charAt(i);
    if (c == '$') {
      int end = i + 1;
      while (end < s.length() && (isalnum(s.charAt(end)) || s.charAt(end) == '_')) {
        end++;
      }
      String varName = s.substring(i + 1, end);
      String varValue = getEnvVar(varName);
      result += varValue;
      i = end - 1;
    } else {
      result += c;
    }
  }
  return result;
}

// Основная функция echo.
// Поддерживает:
//   - Опцию "-n" (подавление завершающего перевода строки)
//   - Разбор кавычек: если текст в кавычках, то кавычки остаются в выводе.
//     Для токенов в двойных кавычках производится развёртка переменных, а в одинарных – нет.
//   - Перенаправление вывода с операторами ">" и ">>"
void handleEcho(String args) {
  if (args.length() == 0) {
    writeOutput("\n");
    return;
  }
  
  // Токенизируем входящую строку
  std::vector<EchoToken> tokens = tokenizeEcho(args);
  
  // Обработка опций echo (например, -n для подавления завершающего перевода строки)
  bool suppressNewline = false;
  int tokenIndex = 0;
  if (!tokens.empty() && tokens[0].value.startsWith("-") &&
      tokens[0].value != ">" && tokens[0].value != ">>") {
    String opts = tokens[0].value;
    if (opts.indexOf("n") != -1) {
      suppressNewline = true;
    }
    tokenIndex++;
  }
  
  // Разбираем операторы перенаправления (если встречаются вне кавычек)
  bool appendMode = false;
  String redirFile = "";
  std::vector<EchoToken> outputTokens;
  for (int i = tokenIndex; i < tokens.size(); i++) {
    if (tokens[i].value == ">" || tokens[i].value == ">>") {
      appendMode = (tokens[i].value == ">>");
      if (i + 1 < tokens.size()) {
        redirFile = tokens[i + 1].value;
        i++; // пропускаем имя файла
      }
    } else {
      outputTokens.push_back(tokens[i]);
    }
  }
  
  // Собираем итоговую строку.
  // Если токен начинается и заканчивается на двойные кавычки, то:
  //   - Убираем кавычки, разворачиваем переменные, затем возвращаем кавычки.
  // Если токен одинарный (обнаружен по флагу singleQuoted) – оставляем его как есть.
  // Иначе – разворачиваем переменные.
  String outputResult = "";
  for (int i = 0; i < outputTokens.size(); i++) {
    String tokenText = outputTokens[i].value;
    if (tokenText.length() >= 2 && tokenText.startsWith("\"") && tokenText.endsWith("\"")) {
      // Обработка двойных кавычек: удаляем внешние кавычки, разворачиваем переменные, затем добавляем кавычки обратно
      String inner = tokenText.substring(1, tokenText.length() - 1);
      inner = expandVariables(inner);
      tokenText = "\"" + inner + "\"";
    } else if (tokenText.length() >= 2 && tokenText.startsWith("'") && tokenText.endsWith("'")) {
      // Одинарные кавычки – оставляем без развёртки переменных
      // tokenText остаётся без изменений.
    } else {
      tokenText = expandVariables(tokenText);
    }
    
    if (i > 0) outputResult += " ";
    outputResult += tokenText;
  }
  
  if (!suppressNewline) {
    outputResult += "\n";
  }
  
  // Если задано перенаправление, записываем результат в файл, иначе выводим на консоль
  if (redirFile.length() > 0) {
    String fullPath = normalizePath(redirFile);
    fs::File file = LittleFS.open(fullPath, appendMode ? FILE_APPEND : FILE_WRITE);
    if (file) {
      file.print(outputResult);
      file.close();
    } else {
      writeOutput("Ошибка: Не удалось открыть файл для записи!\n");
    }
  } else {
    writeOutput(outputResult);
  }
}

// Функция grep: поиск шаблона в файле.
// Ожидается, что аргументы передаются в виде: "<pattern> <file>"
