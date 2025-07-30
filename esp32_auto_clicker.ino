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

// Timing-Variablen für 1000 Hz (1ms Intervall)
unsigned long lastClickTime = 0;
const unsigned long clickInterval = 1; // 1ms = 1000 Hz

void setup() {
  Serial.begin(115200);
  
  // Pin-Modi setzen
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  
  // USB HID initialisieren
  Mouse.begin();
  USB.begin();
  
  Serial.println("ESP32-S3 Auto Clicker gestartet!");
  Serial.println("Drücke den Button um Auto-Click zu starten/stoppen");
  
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
      Serial.println("Auto-Clicking GESTARTET (1000 Hz)");
      digitalWrite(LED_PIN, HIGH); // LED an
    } else {
      Serial.println("Auto-Clicking GESTOPPT");
      digitalWrite(LED_PIN, LOW);  // LED aus
    }
  }
  
  lastButtonState = currentButtonState;
  
  // Auto-Clicking ausführen wenn aktiviert
  if (isClicking) {
    unsigned long currentTime = millis();
    
    // Prüfen ob 1ms vergangen ist (1000 Hz)
    if (currentTime - lastClickTime >= clickInterval) {
      // Linke Maustaste klicken (press und release)
      Mouse.click(MOUSE_LEFT);
      
      lastClickTime = currentTime;
    }
  }
  
  // Kleine Verzögerung um CPU-Last zu reduzieren
  delayMicroseconds(100);
}

// Funktion um Auto-Clicking über Serial-Kommandos zu steuern
void serialEvent() {
  while (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    
    if (command == "start") {
      isClicking = true;
      digitalWrite(LED_PIN, HIGH);
      Serial.println("Auto-Clicking via Serial GESTARTET");
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
