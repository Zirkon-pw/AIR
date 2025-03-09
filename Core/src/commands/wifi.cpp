#include <Arduino.h>
#include <LittleFS.h>
#include <commands/utils.h>
#include <commands/wifi.h>
#include <commands/file_system.h>

void addWifiToList(const String& ssid, const String& password) {
  fs::File file = LittleFS.open("/config/wifi_list.conf", FILE_APPEND);
  if (!file) {
    writeOutput("Ошибка открытия файла wifi_list.conf\n");
    return;
  }
  String entry = ssid + ";" + password + "\n";
  file.print(entry);
  file.close();
  writeOutput("Сеть добавлена в список: " + ssid + "\n");
}

bool findWifiInList(const String& ssid, String& password) {
  fs::File file = LittleFS.open("/config/wifi_list.conf", FILE_READ);
  if (!file) {
    writeOutput("Ошибка открытия файла wifi_list.conf\n");
    return false;
  }
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    int separator = line.indexOf(';');
    if (separator != -1 && line.startsWith(ssid)) {
      password = line.substring(separator + 1);
      file.close();
      return true;
    }
  }
  file.close();
  return false;
}

void handleWifi(String args) {
    int spacePos = args.indexOf(' ');
    if (spacePos == -1) {
        writeOutput("Использование: wifi <SSID> <password>\n");
        return;
    }

    String ssid = args.substring(0, spacePos);
    String password = args.substring(spacePos + 1);

    // Проверяем, существует ли уже такая сеть
    if (isWifiExists(ssid)) {
        writeOutput("Сеть уже существует!\n");
        return;
    }

    // Добавляем сеть в список
    addWifiToList(ssid, password);

    // Обновляем текущую конфигурацию (опционально)
    WifiConfig config = readWifiConfig();
    config.ssid = ssid;
    config.password = password;
    writeWifiConfig(config);

    writeOutput("Сеть добавлена и настроена!\n");
}

// Новые функции для работы с Wi-Fi
bool isWifiExists(const String& ssid) {
    fs::File file = LittleFS.open("/config/wifi_list.conf", FILE_READ);
    if (!file) return false;

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.startsWith(ssid + ";")) {
            file.close();
            return true;
        }
    }
    file.close();
    return false;
}

void handleWifiList() {
    fs::File file = LittleFS.open("/config/wifi_list.conf", FILE_READ);
    if (!file) {
        writeOutput("Список сетей пуст\n");
        return;
    }

    writeOutput("Список сохранённых сетей:\n");
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        int separator = line.indexOf(';');
        if (separator != -1) {
            String ssid = line.substring(0, separator);
            writeOutput("SSID: " + ssid + "\n");
        }
    }
    file.close();
}

void handleWifiRemove(String args) {
    if (args.length() == 0) {
        writeOutput("Использование: wifiremove <SSID>\n");
        return;
    }

    String targetSSID = args;
    String tempContent = "";
    bool found = false;

    fs::File file = LittleFS.open("/config/wifi_list.conf", FILE_READ);
    if (file) {
        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.startsWith(targetSSID + ";")) {
                found = true;
                continue;
            }
            tempContent += line + "\n";
        }
        file.close();
    }

    if (!found) {
        writeOutput("Сеть не найдена!\n");
        return;
    }

    LittleFS.remove("/config/wifi_list.conf");
    writeToFile("/config/wifi_list.conf", tempContent);
    writeOutput("Сеть удалена\n");
}

WifiConfig readWifiConfig() {
    WifiConfig config;
    config.createMode = false;
    config.channel = 1;
    
    fs::File file = LittleFS.open("/config/wifi.conf", "r");
    if (file) {
        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            
            if (line.startsWith("CREATE_WIFI=")) {
                config.createMode = line.substring(12) == "true";
            }
            else if (line.startsWith("SSID=")) {
                config.ssid = line.substring(5);
            }
            else if (line.startsWith("PASSWORD=")) {
                config.password = line.substring(9);
            }
            else if (line.startsWith("CHANNEL=")) {
                config.channel = line.substring(8).toInt();
            }
        }
        file.close();
    }
    return config;
}

void writeWifiConfig(const WifiConfig& config) {
    String content;
    content += "CREATE_WIFI=" + String(config.createMode ? "true" : "false") + "\n";
    content += "SSID=" + config.ssid + "\n";
    content += "PASSWORD=" + config.password + "\n";
    if(config.createMode) {
        content += "CHANNEL=" + String(config.channel) + "\n";
    }
    
    writeToFile("/config/wifi.conf", content);
}

// Обработчики команд
void handleWifiMode(String args) {
    WifiConfig config = readWifiConfig();
    
    if(args == "create") {
        config.createMode = true;
        writeOutput("Режим установлен: Создание точки доступа\n");
    }
    else if(args == "connect") {
        config.createMode = false;
        writeOutput("Режим установлен: Подключение к сети\n");
    }
    else {
        writeOutput("Использование: wifimode <create|connect>\n");
        return;
    }
    
    writeWifiConfig(config);
}

void handleWifiCreate(String args) {
    WifiConfig config = readWifiConfig();
    if(!config.createMode) {
        writeOutput("Сначала переключитесь в режим создания: wifimode create\n");
        return;
    }

    int space1 = args.indexOf(' ');
    int space2 = args.lastIndexOf(' ');
    
    if(space1 == -1 || space1 == space2) {
        writeOutput("Использование: wificreate <SSID> <PASSWORD> [CHANNEL]\n");
        return;
    }
    
    config.ssid = args.substring(0, space1);
    config.password = args.substring(space1+1, space2);
    config.channel = (space2 != -1) ? args.substring(space2+1).toInt() : 6;
    
    writeWifiConfig(config);
    writeOutput("Точка доступа настроена!\n");
}

void handleWifiConnect(String args) {
    if (args.length() == 0) {
        writeOutput("Использование: wificonnect <SSID>\n");
        return;
    }

    String ssid = args;
    String password;

    // Ищем сеть в списке
    if (!findWifiInList(ssid, password)) {
        writeOutput("Сеть не найдена в списке!\n");
        return;
    }

    // Обновляем текущую конфигурацию
    WifiConfig config = readWifiConfig();
    config.ssid = ssid;
    config.password = password;
    writeWifiConfig(config);

    writeOutput("Подключение к сети: " + ssid + "\n");
    // Здесь можно добавить вызов функции для реального подключения к Wi-Fi
}

void handleWifiInfo() {
    WifiConfig config = readWifiConfig();
    String info = "Текущий режим: ";
    info += config.createMode ? "Создание точки доступа\n" : "Подключение к сети\n";
    info += "SSID: " + config.ssid + "\n";
    info += "Пароль: " + config.password + "\n";
    if(config.createMode) {
        info += "Канал: " + String(config.channel) + "\n";
    }
    writeOutput(info);
}