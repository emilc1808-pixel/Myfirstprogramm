#include "USB.h"
#include "USBHIDMouse.h"
#include "Arduino.h"
#include "BluetoothSerial.h"

USBHIDMouse Mouse;
BluetoothSerial SerialBT;

// Pin-Definitionen
const int BUTTON_PIN = 0;  // GPIO0 (Boot-Button auf den meisten ESP32-S3 Boards)
const int LED_PIN = 2;     // Onboard LED

// Variablen für Button-Handling
bool lastButtonState = HIGH;
bool currentButtonState = HIGH;
bool isClicking = false;

// Timing-Variablen für 10 kHz (0.1ms Intervall)
unsigned long lastClickTime = 0;
const unsigned long clickIntervalMicros = 100; // 100 Mikrosekunden = 10 kHz
unsigned long clickCounter = 0;

// Bluetooth-Variablen
bool bluetoothConnected = false;
String deviceName = "ESP32-AutoClicker";

void setup() {
  Serial.begin(115200);
  
  // Pin-Modi setzen
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  
  // USB HID initialisieren
  Mouse.begin();
  USB.begin();
  
  // Bluetooth initialisieren
  SerialBT.begin(deviceName); // Bluetooth-Gerätename
  Serial.println("Bluetooth gestartet! Gerätename: " + deviceName);
  Serial.println("Verbinde dich über Bluetooth um den Auto-Clicker zu steuern.");
  
  Serial.println("ESP32-S3 High Speed Auto Clicker gestartet!");
  Serial.println("Steuerung:");
  Serial.println("- Button drücken: Auto-Click starten/stoppen");
  Serial.println("- Bluetooth Kommandos: 'start', 'stop', 'status'");
  Serial.println("- Serial Kommandos: 'start', 'stop', 'status'");
  Serial.println("10.000 Klicks pro Sekunde - sehr schnell!");
  
  // LED ausschalten
  digitalWrite(LED_PIN, LOW);
  
  // LED-Blink zum Anzeigen dass Bluetooth bereit ist
  for(int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
}

void loop() {
  // Bluetooth-Verbindungsstatus prüfen
  checkBluetoothConnection();
  
  // Button-Status lesen
  currentButtonState = digitalRead(BUTTON_PIN);
  
  // Button-Press Detection (falling edge)
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    delay(50); // Entprellung
    
    // Status umschalten
    toggleClicking();
  }
  
  lastButtonState = currentButtonState;
  
  // Bluetooth-Kommandos verarbeiten
  handleBluetoothCommands();
  
  // Serial-Kommandos verarbeiten
  handleSerialCommands();
  
  // Auto-Clicking ausführen wenn aktiviert
  if (isClicking) {
    unsigned long currentTimeMicros = micros();
    
    // Prüfen ob 100 Mikrosekunden vergangen sind (10 kHz)
    if (currentTimeMicros - lastClickTime >= clickIntervalMicros) {
      // Linke Maustaste klicken (press und release)
      Mouse.click(MOUSE_LEFT);
      
      lastClickTime = currentTimeMicros;
      clickCounter++;
      
      // Status-Update alle 10000 Klicks (jede Sekunde bei 10 kHz)
      if (clickCounter >= 10000) {
        String statusMsg = "10.000 Klicks gesendet (1 Sekunde) - Total: " + String(clickCounter);
        Serial.println(statusMsg);
        if (bluetoothConnected) {
          SerialBT.println(statusMsg);
        }
        clickCounter = 0;
      }
    }
  }
  
  // Minimale CPU-Entlastung
  if (!isClicking) {
    delayMicroseconds(100);
  }
}

void toggleClicking() {
  isClicking = !isClicking;
  
  String statusMsg;
  if (isClicking) {
    statusMsg = "Auto-Clicking GESTARTET (10 kHz = 10.000 Klicks/Sek)";
    digitalWrite(LED_PIN, HIGH); // LED an
  } else {
    statusMsg = "Auto-Clicking GESTOPPT";
    digitalWrite(LED_PIN, LOW);  // LED aus
  }
  
  Serial.println(statusMsg);
  if (bluetoothConnected) {
    SerialBT.println(statusMsg);
  }
}

void checkBluetoothConnection() {
  static bool lastConnectionState = false;
  bool currentConnectionState = SerialBT.hasClient();
  
  if (currentConnectionState != lastConnectionState) {
    bluetoothConnected = currentConnectionState;
    
    if (bluetoothConnected) {
      Serial.println("Bluetooth-Client verbunden!");
      SerialBT.println("Verbindung hergestellt!");
      SerialBT.println("Verfügbare Kommandos: 'start', 'stop', 'status'");
      
      // Kurzes LED-Blinken bei Verbindung
      for(int i = 0; i < 2; i++) {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        delay(100);
      }
    } else {
      Serial.println("Bluetooth-Client getrennt!");
    }
    
    lastConnectionState = currentConnectionState;
  }
}

void handleBluetoothCommands() {
  if (SerialBT.available()) {
    String command = SerialBT.readString();
    command.trim();
    command.toLowerCase();
    
    processCommand(command, true); // true = von Bluetooth
  }
}

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    command.toLowerCase();
    
    processCommand(command, false); // false = von Serial
  }
}

void processCommand(String command, bool fromBluetooth) {
  String response = "";
  
  if (command == "start") {
    if (!isClicking) {
      isClicking = true;
      digitalWrite(LED_PIN, HIGH);
      response = "Auto-Clicking GESTARTET (10 kHz)";
    } else {
      response = "Auto-Clicking läuft bereits!";
    }
  }
  else if (command == "stop") {
    if (isClicking) {
      isClicking = false;
      digitalWrite(LED_PIN, LOW);
      response = "Auto-Clicking GESTOPPT";
    } else {
      response = "Auto-Clicking ist bereits gestoppt!";
    }
  }
  else if (command == "status") {
    response = "Status: " + String(isClicking ? "AKTIV" : "INAKTIV");
    response += " | Bluetooth: " + String(bluetoothConnected ? "VERBUNDEN" : "GETRENNT");
    response += " | Gerätename: " + deviceName;
  }
  else if (command == "help" || command == "?") {
    response = "Verfügbare Kommandos:\n";
    response += "- start: Auto-Clicking starten\n";
    response += "- stop: Auto-Clicking stoppen\n";
    response += "- status: Aktuellen Status anzeigen\n";
    response += "- help: Diese Hilfe anzeigen";
  }
  else if (command != "") {
    response = "Unbekanntes Kommando: '" + command + "'. Sende 'help' für verfügbare Kommandos.";
  }
  
  // Antwort senden
  if (response != "") {
    Serial.println(response);
    if (fromBluetooth && bluetoothConnected) {
      SerialBT.println(response);
    } else if (!fromBluetooth && bluetoothConnected) {
      SerialBT.println("[Serial] " + response);
    }
  }
}
