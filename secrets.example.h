#pragma once
// ======================================================================
// secrets.example.h
// EN: TEMPLATE for secrets.h. Copy it to secrets.h (git-ignored, never committed) and fill in your own values.
//     Without secrets.h the project still compiles: WiFi stays empty (the module starts in AP mode), OTA has no
//     password. These are only START-UP values - whatever you save in the panel (Settings) is kept in the module
//     memory and takes precedence.
// PL: WZOR pliku secrets.h. Skopiuj do secrets.h (w .gitignore, nie trafia do gita) i wpisz własne wartości.
//     Bez secrets.h projekt się kompiluje: WiFi puste (moduł wstaje w trybie AP), OTA bez hasła. To tylko wartości
//     STARTOWE - to, co zapiszesz w panelu (Ustawienia), zostaje w pamięci modułu i ma pierwszeństwo.
// ======================================================================

// Start-up WiFi network / startowa sieć WiFi
#define SECRET_WIFI_SSID "network_name"
#define SECRET_WIFI_PASS "network_password"

// Password of the module's own access point (AP), min. 8 characters. Without this line the module makes a random password on first start and prints it to the USB serial monitor.
// Hasło własnego punktu dostępowego (AP), min. 8 znaków. Bez tej linii moduł losuje hasło przy pierwszym starcie i wypisuje je na monitorze portu szeregowego (USB).
// #define SECRET_AP_PASSWORD "your_ap_password"

// OTA password: the Arduino IDE asks for it when uploading and the /update page asks you to log in (user: admin).
// Write it down - if it is lost, new firmware can only be flashed over USB.
// Hasło OTA: Arduino IDE zapyta o nie przy wgrywaniu, a strona /update poprosi o logowanie (użytkownik: admin).
// Zapisz je - przy zapomnianym haśle wgrasz nowe oprogramowanie tylko kablem USB.
// #define SECRET_OTA_PASSWORD "your_ota_password"
