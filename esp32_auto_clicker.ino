#include "USB.h"
#include "USBHIDMouse.h"

USBHIDMouse Mouse;

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

void setup() {
  Serial.begin(115200);
  
  // Pin-Modi setzen
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  
  // USB HID initialisieren
  Mouse.begin();
  USB.begin();
  
  Serial.println("ESP32-S3 High Speed Auto Clicker gestartet!");
  Serial.println("Drücke den Button um Auto-Click zu starten/stoppen (10 kHz)");
  Serial.println("10.000 Klicks pro Sekunde - sehr schnell!");
  
  // LED ausschalten
  digitalWrite(LED_PIN, LOW);
}

void loop() {
  // Button-Status lesen
  currentButtonState = digitalRead(BUTTON_PIN);
  
  // Button-Press Detection (falling edge)
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    delay(50); // Entprellung
    
    // Status umschalten
    isClicking = !isClicking;
    
    if (isClicking) {
      Serial.println("Auto-Clicking GESTARTET (10 kHz = 10.000 Klicks/Sek)");
      digitalWrite(LED_PIN, HIGH); // LED an
    } else {
      Serial.println("Auto-Clicking GESTOPPT");
      digitalWrite(LED_PIN, LOW);  // LED aus
    }
  }
  
  lastButtonState = currentButtonState;
  
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
        Serial.print("10.000 Klicks gesendet (1 Sekunde) - Total: ");
        Serial.println(clickCounter);
        clickCounter = 0;
      }
    }
  }
  
  // Minimale CPU-Entlastung
  if (!isClicking) {
    delayMicroseconds(100);
  }
}

// Funktion um Auto-Clicking über Serial-Kommandos zu steuern
void serialEvent() {
  while (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    
    if (command == "start") {
      isClicking = true;
      digitalWrite(LED_PIN, HIGH);
      Serial.println("Auto-Clicking via Serial GESTARTET (10 kHz)");
    }
    else if (command == "stop") {
      isClicking = false;
      digitalWrite(LED_PIN, LOW);
      Serial.println("Auto-Clicking via Serial GESTOPPT");
    }
    else if (command == "status") {
      Serial.println(isClicking ? "Status: AKTIV" : "Status: INAKTIV");
    }
  }
}
