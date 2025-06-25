/*

#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <Audio.h> // ESP32-audioI2S

// --- CONFIGURACIÓN WIFI ---
const char* ssid = "Redmi";
const char* password = "Marti12345";

// --- EEPROM ---
#define EEPROM_SIZE 512
#define MAX_STATIONS 5
#define NAME_LEN 32
#define URL_LEN 128

// --- Audio ---
Audio audio;

// --- WEB ---
WebServer server(80);

// --- Estructura para una estación ---
struct Station {
  char name[NAME_LEN];
  char url[URL_LEN];
};

// --- Variables globales ---
Station stations[MAX_STATIONS];
int currentStation = -1;

// --- Prototipos ---
void handleRoot();
void handleSave();
void handlePlay();
void setupAudio();
void playStation(int index);
void resetEEPROM();
void loadStationsFromEEPROM();
void saveStationsToEEPROM();
String readStringFromEEPROM(int addr, int len);
void writeStringToEEPROM(int addr, const String &str, int len);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Iniciando...");

  // Inicializar I2C en pines 4 (SDA), 21 (SCL)
  Wire.begin(4, 21);

  EEPROM.begin(EEPROM_SIZE);

  // Si quieres resetear EEPROM la primera vez, descomenta:
  // resetEEPROM();

  loadStationsFromEEPROM();

  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Conectado, IP: ");
  Serial.println(WiFi.localIP());

  setupAudio();

  // Rutas web
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/play", HTTP_GET, handlePlay);

  server.begin();
  Serial.println("Servidor iniciado.");
}

void loop() {
  server.handleClient();
  audio.loop();
}

// --- Funciones ---

void resetEEPROM() {
  Serial.println("Borrando EEPROM...");
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  Serial.println("EEPROM borrada.");
}

void loadStationsFromEEPROM() {
  Serial.println("Cargando estaciones desde EEPROM...");
  int addr = 0;
  for (int i = 0; i < MAX_STATIONS; i++) {
    String name = readStringFromEEPROM(addr, NAME_LEN);
    addr += NAME_LEN;
    String url = readStringFromEEPROM(addr, URL_LEN);
    addr += URL_LEN;

    // Guardamos en la estructura, asegurando terminación nula
    name.toCharArray(stations[i].name, NAME_LEN);
    url.toCharArray(stations[i].url, URL_LEN);

    Serial.printf("Estacion %d: '%s' -> '%s'\n", i, stations[i].name, stations[i].url);
  }
}

void saveStationsToEEPROM() {
  Serial.println("Guardando estaciones en EEPROM...");
  int addr = 0;
  for (int i = 0; i < MAX_STATIONS; i++) {
    writeStringToEEPROM(addr, String(stations[i].name), NAME_LEN);
    addr += NAME_LEN;
    writeStringToEEPROM(addr, String(stations[i].url), URL_LEN);
    addr += URL_LEN;
  }
  EEPROM.commit();
  Serial.println("Guardado EEPROM OK.");
}

String readStringFromEEPROM(int addr, int len) {
  String val = "";
  for (int i = 0; i < len; i++) {
    byte c = EEPROM.read(addr + i);
    if (c == 0 || c == 0xFF) break;
    val += (char)c;
  }
  return val;
}

void writeStringToEEPROM(int addr, const String &str, int len) {
  int writeLen = str.length();
  if (writeLen > len - 1) writeLen = len - 1;
  for (int i = 0; i < writeLen; i++) {
    EEPROM.write(addr + i, str[i]);
  }
  EEPROM.write(addr + writeLen, 0); // Termina cadena con 0
  for (int i = writeLen + 1; i < len; i++) {
    EEPROM.write(addr + i, 0); // Limpia el resto
  }
}

void setupAudio() {
  Serial.println("Inicializando audio...");
  // Cambia pines según tu configuración hardware
  // Ejemplo para I2S (ajusta si usas DAC interno o externo)
  audio.setPinout(14, 13, 20); // BCLK, LRC, DIN
  audio.setVolume(20); // 0-21 aprox
  Serial.println("Audio listo.");
}

void playStation(int index) {
  if (index < 0 || index >= MAX_STATIONS) {
    Serial.println("Índice estación inválido");
    return;
  }
  if (strlen(stations[index].url) == 0) {
    Serial.println("URL vacía, no se reproduce");
    return;
  }
  Serial.printf("Reproduciendo estación %d: %s\n", index, stations[index].url);
  audio.stopSong();
  audio.connecttohost(stations[index].url);
  currentStation = index;
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Estaciones de Radio</title></head><body>";
  html += "<h1>Estaciones de Radio</h1>";

  html += "<form action='/save' method='POST'>";
  for (int i = 0; i < MAX_STATIONS; i++) {
    html += "<h3>Estación " + String(i + 1) + "</h3>";
    html += "Nombre:<br><input type='text' name='name" + String(i) + "' value='" + String(stations[i].name) + "' maxlength='" + String(NAME_LEN-1) + "'><br>";
    html += "URL:<br><input type='text' name='url" + String(i) + "' value='" + String(stations[i].url) + "' maxlength='" + String(URL_LEN-1) + "' style='width: 400px;'><br>";
    html += "<button type='button' onclick=\"location.href='/play?idx=" + String(i) + "'\">Reproducir</button>";
    html += "<hr>";
  }
  html += "<input type='submit' value='Guardar estaciones'>";
  html += "</form>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleSave() {
  Serial.println("Guardando estaciones desde formulario...");
  bool changed = false;

  for (int i = 0; i < MAX_STATIONS; i++) {
    if (server.hasArg("name" + String(i)) && server.hasArg("url" + String(i))) {
      String n = server.arg("name" + String(i));
      String u = server.arg("url" + String(i));

      n.trim();
      u.trim();

      if (n.length() >= NAME_LEN) n = n.substring(0, NAME_LEN-1);
      if (u.length() >= URL_LEN) u = u.substring(0, URL_LEN-1);

      if (n != String(stations[i].name) || u != String(stations[i].url)) {
        changed = true;
        n.toCharArray(stations[i].name, NAME_LEN);
        u.toCharArray(stations[i].url, URL_LEN);
        Serial.printf("Estación %d actualizada: '%s' -> '%s'\n", i, stations[i].name, stations[i].url);
      }
    }
  }
  if (changed) {
    saveStationsToEEPROM();
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

void handlePlay() {
  if (!server.hasArg("idx")) {
    server.send(400, "text/plain", "Falta parámetro idx");
    return;
  }
  int idx = server.arg("idx").toInt();
  playStation(idx);

  String resp = "<html><head><meta http-equiv='refresh' content='1; url=/'></head><body>Reproduciendo estación " + String(idx+1) + ". Volviendo...</body></html>";
  server.send(200, "text/html", resp);
}
*/

#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <Audio.h> // ESP32-audioI2S
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- CONFIGURACIÓN WIFI ---
const char* ssid = "Redmi";
const char* password = "Marti12345";

// --- EEPROM ---
#define EEPROM_SIZE 512
#define MAX_STATIONS 5
#define NAME_LEN 32
#define URL_LEN 128

// --- Audio ---
Audio audio;

// --- WEB ---
WebServer server(80);

// --- OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Estructura para una estación ---
struct Station {
  char name[NAME_LEN];
  char url[URL_LEN];
};

// --- Variables globales ---
Station stations[MAX_STATIONS];
int currentStation = -1;

// --- Prototipos ---
void handleRoot();
void handleSave();
void handlePlay();
void setupAudio();
void playStation(int index);
void resetEEPROM();
void loadStationsFromEEPROM();
void saveStationsToEEPROM();
String readStringFromEEPROM(int addr, int len);
void writeStringToEEPROM(int addr, const String &str, int len);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Iniciando...");

  // Inicializar I2C en pines 4 (SDA), 21 (SCL)
  Wire.begin(4, 21);

  // Inicializar pantalla OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("Error al iniciar OLED");
    // Podrías entrar en bucle o continuar sin pantalla
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Iniciando...");
  display.display();

  EEPROM.begin(EEPROM_SIZE);

  // Si quieres resetear EEPROM la primera vez, descomenta:
  // resetEEPROM();

  loadStationsFromEEPROM();

  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi...");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Conectando a WiFi...");
  display.display();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Conectado, IP: ");
  Serial.println(WiFi.localIP());

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi conectado!");
  display.println(WiFi.localIP().toString());
  display.display();

  setupAudio();

  // Rutas web
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/play", HTTP_GET, handlePlay);

  server.begin();
  Serial.println("Servidor iniciado.");
}

void loop() {
  server.handleClient();
  audio.loop();
}

// --- Funciones ---

void resetEEPROM() {
  Serial.println("Borrando EEPROM...");
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  Serial.println("EEPROM borrada.");
}

void loadStationsFromEEPROM() {
  Serial.println("Cargando estaciones desde EEPROM...");
  int addr = 0;
  for (int i = 0; i < MAX_STATIONS; i++) {
    String name = readStringFromEEPROM(addr, NAME_LEN);
    addr += NAME_LEN;
    String url = readStringFromEEPROM(addr, URL_LEN);
    addr += URL_LEN;

    // Guardamos en la estructura, asegurando terminación nula
    name.toCharArray(stations[i].name, NAME_LEN);
    url.toCharArray(stations[i].url, URL_LEN);

    Serial.printf("Estacion %d: '%s' -> '%s'\n", i, stations[i].name, stations[i].url);
  }
}

void saveStationsToEEPROM() {
  Serial.println("Guardando estaciones en EEPROM...");
  int addr = 0;
  for (int i = 0; i < MAX_STATIONS; i++) {
    writeStringToEEPROM(addr, String(stations[i].name), NAME_LEN);
    addr += NAME_LEN;
    writeStringToEEPROM(addr, String(stations[i].url), URL_LEN);
    addr += URL_LEN;
  }
  EEPROM.commit();
  Serial.println("Guardado EEPROM OK.");
}

String readStringFromEEPROM(int addr, int len) {
  String val = "";
  for (int i = 0; i < len; i++) {
    byte c = EEPROM.read(addr + i);
    if (c == 0 || c == 0xFF) break;
    val += (char)c;
  }
  return val;
}

void writeStringToEEPROM(int addr, const String &str, int len) {
  int writeLen = str.length();
  if (writeLen > len - 1) writeLen = len - 1;
  for (int i = 0; i < writeLen; i++) {
    EEPROM.write(addr + i, str[i]);
  }
  EEPROM.write(addr + writeLen, 0); // Termina cadena con 0
  for (int i = writeLen + 1; i < len; i++) {
    EEPROM.write(addr + i, 0); // Limpia el resto
  }
}

void setupAudio() {
  Serial.println("Inicializando audio...");
  // Cambia pines según tu configuración hardware
  // Ejemplo para I2S (ajusta si usas DAC interno o externo)
  audio.setPinout(14, 13, 12); // BCLK, LRC, DIN
  audio.setVolume(20); // 0-21 aprox
  Serial.println("Audio listo.");
}

void playStation(int index) {
  if (index < 0 || index >= MAX_STATIONS) {
    Serial.println("Índice estación inválido");
    return;
  }
  if (strlen(stations[index].url) == 0) {
    Serial.println("URL vacía, no se reproduce");
    return;
  }

  Serial.printf("Reproduciendo estación %d: %s\n", index, stations[index].url);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Reproduciendo:");
  display.println(stations[index].name);
  display.display();

  audio.stopSong();
  audio.connecttohost(stations[index].url);
  currentStation = index;
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Estaciones de Radio</title></head><body>";
  html += "<h1>Estaciones de Radio</h1>";

  html += "<form action='/save' method='POST'>";
  for (int i = 0; i < MAX_STATIONS; i++) {
    html += "<h3>Estación " + String(i + 1) + "</h3>";
    html += "Nombre:<br><input type='text' name='name" + String(i) + "' value='" + String(stations[i].name) + "' maxlength='" + String(NAME_LEN-1) + "'><br>";
    html += "URL:<br><input type='text' name='url" + String(i) + "' value='" + String(stations[i].url) + "' maxlength='" + String(URL_LEN-1) + "' style='width: 400px;'><br>";
    html += "<button type='button' onclick=\"location.href='/play?idx=" + String(i) + "'\">Reproducir</button>";
    html += "<hr>";
  }
  html += "<input type='submit' value='Guardar estaciones'>";
  html += "</form>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleSave() {
  Serial.println("Guardando estaciones desde formulario...");
  bool changed = false;

  for (int i = 0; i < MAX_STATIONS; i++) {
    if (server.hasArg("name" + String(i)) && server.hasArg("url" + String(i))) {
      String n = server.arg("name" + String(i));
      String u = server.arg("url" + String(i));

      n.trim();
      u.trim();

      if (n.length() >= NAME_LEN) n = n.substring(0, NAME_LEN-1);
      if (u.length() >= URL_LEN) u = u.substring(0, URL_LEN-1);

      if (n != String(stations[i].name) || u != String(stations[i].url)) {
        changed = true;
        n.toCharArray(stations[i].name, NAME_LEN);
        u.toCharArray(stations[i].url, URL_LEN);
        Serial.printf("Estación %d actualizada: '%s' -> '%s'\n", i, stations[i].name, stations[i].url);
      }
    }
  }
  if (changed) {
    saveStationsToEEPROM();
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

void handlePlay() {
  if (!server.hasArg("idx")) {
    server.send(400, "text/plain", "Falta parámetro idx");
    return;
  }
  int idx = server.arg("idx").toInt();
  playStation(idx);

  String resp = "<html><head><meta http-equiv='refresh' content='1; url=/'></head><body>Reproduciendo estación " + String(idx+1) + ". Volviendo...</body></html>";
  server.send(200, "text/html", resp);
}
