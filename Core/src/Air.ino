#include <LittleFS.h>
#include <FS.h>
#include <EEPROM.h>
#include <console.h>       // Ваша библиотека для работы с консолью

#include <WiFi.h>          // Для ESP32
#include <WebServer.h>     // Для ESP32

#define BAUDRATE 115200

// ================= Настройки WiFi =================
const char* ssid     = "TP-Link_217D";      // Замените на имя вашей WiFi-сети
const char* password = "20983338";  // Замените на пароль

// Создаём объект веб-сервера на порту 80
WebServer server(80);

// Глобальная переменная для хранения файла при загрузке
File fsUploadFile;


// Главная страница с HTML-интерфейсом
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Файловая система</title></head><body>";
  html += "<h1>Веб-интерфейс файловой системы</h1>";
  html += "<h2>Список файлов</h2>";
  html += "<div id='fileList'>Загрузка...</div>";
  html += "<script>";
  html += "fetch('/list').then(response => response.json()).then(data => {";
  html += "  let html = '<ul>';";
  html += "  data.forEach(file => {";
  html += "    html += '<li>' + file.name + ' (' + file.size + ' байт) ';";
  html += "    html += '<a href=\"/read?name=' + file.name + '\">[Читать]</a> ';";
  html += "    html += '<a href=\"/delete?name=' + file.name + '\">[Удалить]</a>';";
  html += "    html += '</li>';";
  html += "  });";
  html += "  html += '</ul>';";
  html += "  document.getElementById('fileList').innerHTML = html;";
  html += "});";
  html += "</script>";
  html += "<h2>Загрузка файла</h2>";
  html += "<form method='POST' action='/upload' enctype='multipart/form-data'>";
  html += "  <input type='file' name='file'><br><br>";
  html += "  <input type='submit' value='Загрузить'>";
  html += "</form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Возвращает список файлов в формате JSON
void handleFileList() {
  String output = "[";
  File root = LittleFS.open("/");
  if (!root) {
    server.send(500, "text/plain", "Не удалось открыть корневую директорию");
    return;
  }
  File file = root.openNextFile();
  bool first = true;
  while (file) {
    if (!first) {
      output += ",";
    }
    first = false;
    String fileName = file.name();
    size_t fileSize = file.size();
    output += "{\"name\":\"" + fileName + "\",\"size\":" + String(fileSize) + "}";
    file = root.openNextFile();
  }
  output += "]";
  server.send(200, "application/json", output);
}

// Чтение файла (имя файла передаётся в GET-параметре name)
void handleFileRead() {
  if (!server.hasArg("name")) {
    server.send(400, "text/plain", "Ошибка: не указано имя файла");
    return;
  }
  String path = server.arg("name");
  if (!path.startsWith("/")) {
    path = "/" + path;
  }
  Serial.print("Запрошен файл: ");
  Serial.println(path);
  
  if (!LittleFS.exists(path)) {
    Serial.println("Файл не найден в файловой системе!");
    server.send(404, "text/plain", "Файл не найден");
    return;
  }
  File file = LittleFS.open(path, "r");
  if (!file) {
    server.send(500, "text/plain", "Ошибка открытия файла");
    return;
  }
  String content = file.readString();
  file.close();
  server.send(200, "text/plain", content);
}

// Удаление файла (имя файла передаётся в GET-параметре name)
void handleFileDelete() {
  if (!server.hasArg("name")) {
    server.send(400, "text/plain", "Ошибка: не указано имя файла");
    return;
  }
  String path = server.arg("name");
  if (!path.startsWith("/")) {
    path = "/" + path;
  }
  Serial.print("Удаляется файл: ");
  Serial.println(path);
  
  if (!LittleFS.exists(path)) {
    server.send(404, "text/plain", "Файл не найден");
    return;
  }
  LittleFS.remove(path);
  server.send(200, "text/plain", "Файл удалён");
}

// Обработчик загрузки файла
void handleFileUpload() {
  HTTPUpload& upload = server.upload();

  Serial.print("upload.status: ");
  Serial.println(upload.status);

  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    Serial.print("Начало загрузки: ");
    Serial.println(filename);
    if (LittleFS.exists(filename)) {
      LittleFS.remove(filename);
      Serial.println("Существующий файл удалён");
    }
    fsUploadFile = LittleFS.open(filename, "w");
    if (!fsUploadFile) {
      Serial.println("Ошибка создания файла для загрузки");
      return;
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile) {
      size_t bytesWritten = fsUploadFile.write(upload.buf, upload.currentSize);
      Serial.print("Записано байт: ");
      Serial.println(bytesWritten);
    } else {
      Serial.println("Файл не открыт для записи (UPLOAD_FILE_WRITE)");
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile.close();
      Serial.print("Загрузка завершена: ");
      Serial.println(upload.filename);
    } else {
      Serial.println("Файл не открыт при завершении загрузки (UPLOAD_FILE_END)");
    }
  }
  else {
    Serial.print("Неизвестный статус загрузки: ");
    Serial.println(upload.status);
  }
}

// Обработчик для не найденных маршрутов (например, favicon.ico)
void handleNotFound() {
  server.send(404, "text/plain", "Ресурс не найден");
}

// Настройка маршрутов веб-сервера
void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/list", HTTP_GET, handleFileList);
  server.on("/read", HTTP_GET, handleFileRead);
  server.on("/delete", HTTP_GET, handleFileDelete);

  server.on("/upload", HTTP_POST, []() {
    server.send(200, "text/plain", "Файл загружен");
  });
  server.onFileUpload(handleFileUpload);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("HTTP сервер запущен");
}

void setup() {
  Serial.begin(BAUDRATE);
  initializeFS();
  printHelp();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Подключение к WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Подключено! IP-адрес: ");
  Serial.println(WiFi.localIP());

  setupWebServer();
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      handleCommand(input);
    }
  }
  server.handleClient();
}
