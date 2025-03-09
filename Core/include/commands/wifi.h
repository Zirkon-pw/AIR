#ifndef WIFI_H
#define WIFI_H

#include <Arduino.h>

struct WifiConfig {
    bool createMode;
    String ssid;
    String password;
    int channel;
};

void addWifiToList(const String& ssid, const String& password);
bool findWifiInList(const String& ssid, String& password);
WifiConfig readWifiConfig();
void writeWifiConfig(const WifiConfig& config);
void handleWifi(String args);
void handleWifiMode(String args);
void handleWifiCreate(String args);
void handleWifiConnect(String args);
void handleWifiInfo();
void handleWifiList();
void handleWifiRemove(String args);
bool isWifiExists(const String& ssid);

#endif