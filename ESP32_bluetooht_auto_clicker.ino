#include "Arduino.h"
#include "BluetoothSerial.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_defs.h"
#include "esp_hid_gap.h"

// Für ESP32-S3 verwenden wir ESP-HID
#include "esp_hidd_prf_api.h"
#include "hidd_le_prf_int.h"

// Pin-Definitionen
const int BUTTON_PIN = 0;  // GPIO0 (Boot-Button)
const int LED_PIN = 2;     // Onboard LED

// Variablen für Button-Handling
bool lastButtonState = HIGH;
bool currentButtonState = HIGH;

// Timing-Variablen für 10 kHz (0.1ms Intervall)
unsigned long lastClickTime = 0;
const unsigned long clickIntervalMicros = 100; // 100 Mikrosekunden = 10 kHz
unsigned long clickCounter = 0;

// HID-Variablen
bool connected = false;
String deviceName = "ESP32 Auto Mouse";

// HID Report Descriptor für Maus
static const uint8_t hid_mouse_descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x03,        //     Usage Maximum (0x03)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x95, 0x03,        //     Report Count (3)
    0x75, 0x01,        //     Report Size (1)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x05,        //     Report Size (5)
    0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x02,        //     Report Count (2)
    0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0xC0,              // End Collection
};

static esp_hidd_app_param_t hidd_app_param = {0};
static esp_hidd_qos_param_t hidd_qos_param = {
    .service_type = 1,
    .token_rate = 1,
    .token_bucket_size = 1,
    .peak_bandwidth = 1,
    .latency = 1,
    .delay_variation = 1,
};

// HID Callbacks
static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param);

void setup() {
  Serial.begin(115200);
  
  // Pin-Modi setzen
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  
  Serial.println("ESP32-S3 Bluetooth HID Mouse 10kHz Clicker");
  Serial.println("Initialisiere Bluetooth HID...");
  
  // LED ausschalten
  digitalWrite(LED_PIN, LOW);
  
  // Bluetooth HID initialisieren
  initBluetoothHID();
  
  Serial.println("Bluetooth HID Maus gestartet!");
  Serial.println("Gerätename: " + deviceName);
  Serial.println("HALTE den Button gedrückt für 10kHz Auto-Clicking!");
  Serial.println("Das Gerät erscheint als Bluetooth-Maus in den Systemeinstellungen");
  
  // LED-Blink zum Anzeigen dass BT bereit ist
  for(int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(300);
    digitalWrite(LED_PIN, LOW);
    delay(300);
  }
}

void loop() {
  // Button-Status lesen
  currentButtonState = digitalRead(BUTTON_PIN);
  
  // Solange Button gedrückt ist: 10kHz Clicking
  if (currentButtonState == LOW && connected) {  // Button gedrückt (LOW wegen INPUT_PULLUP)
    digitalWrite(LED_PIN, HIGH);  // LED an während Clicking
    
    unsigned long currentTimeMicros = micros();
    
    // Prüfen ob 100 Mikrosekunden vergangen sind (10 kHz)
    if (currentTimeMicros - lastClickTime >= clickIntervalMicros) {
      // Linke Maustaste klicken (press und release)
      sendMouseClick();
      
      lastClickTime = currentTimeMicros;
      clickCounter++;
      
      // Status-Update alle 10000 Klicks (jede Sekunde bei 10 kHz)
      if (clickCounter >= 10000) {
        Serial.print("10.000 Klicks gesendet (1 Sekunde) - Total: ");
        Serial.println(clickCounter);
        clickCounter = 0;
      }
    }
  } else {
    // Button nicht gedrückt oder nicht verbunden
    digitalWrite(LED_PIN, connected ? LOW : LOW);  // LED aus
    
    // Minimale CPU-Entlastung wenn nicht geklickt wird
    delayMicroseconds(100);
  }
  
  // Verbindungsstatus anzeigen
  if (currentButtonState == LOW && !connected) {
    // Button gedrückt aber nicht verbunden - LED blinken lassen
    static unsigned long lastBlinkTime = 0;
    static bool ledState = false;
    
    if (millis() - lastBlinkTime > 200) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      lastBlinkTime = millis();
      
      if (ledState) {
        Serial.println("Keine Bluetooth-Verbindung! Verbinde das Gerät zuerst.");
      }
    }
  }
  
  lastButtonState = currentButtonState;
  
  // Serial-Kommandos verarbeiten
  handleSerialCommands();
}

void initBluetoothHID() {
  esp_err_t ret;
  
  // HID Profil Parameter setzen
  hidd_app_param.name = (char*)deviceName.c_str();
  hidd_app_param.description = "ESP32 HID Mouse";
  hidd_app_param.provider = "ESP32";
  hidd_app_param.subclass = 0x80; // Combo keyboard/mouse
  hidd_app_param.desc_list = hid_mouse_descriptor;
  hidd_app_param.desc_list_len = sizeof(hid_mouse_descriptor);
  
  // HID initialisieren
  ret = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  if (ret != ESP_OK) {
    Serial.printf("Bluetooth controller release memory failed: %s\n", esp_err_to_name(ret));
    return;
  }

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ret = esp_bt_controller_init(&bt_cfg);
  if (ret != ESP_OK) {
    Serial.printf("Bluetooth controller initialize failed: %s\n", esp_err_to_name(ret));
    return;
  }

  ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
  if (ret != ESP_OK) {
    Serial.printf("Bluetooth controller enable failed: %s\n", esp_err_to_name(ret));
    return;
  }

  ret = esp_bluedroid_init();
  if (ret != ESP_OK) {
    Serial.printf("Bluedroid initialize failed: %s\n", esp_err_to_name(ret));
    return;
  }

  ret = esp_bluedroid_enable();
  if (ret != ESP_OK) {
    Serial.printf("Bluedroid enable failed: %s\n", esp_err_to_name(ret));
    return;
  }

  // HID Device Profil registrieren
  ret = esp_hidd_profile_init();
  if (ret != ESP_OK) {
    Serial.printf("HID device profile init failed: %s\n", esp_err_to_name(ret));
    return;
  }

  // Event Callback registrieren
  ret = esp_hidd_register_callbacks(hidd_event_callback);
  if (ret != ESP_OK) {
    Serial.printf("HID device register callbacks failed: %s\n", esp_err_to_name(ret));
    return;
  }

  // App registrieren
  ret = esp_hidd_app_register(&hidd_app_param, &hidd_qos_param);
  if (ret != ESP_OK) {
    Serial.printf("HID device app register failed: %s\n", esp_err_to_name(ret));
    return;
  }

  Serial.println("Bluetooth HID erfolgreich initialisiert");
  Serial.println("Gerät ist nun als Bluetooth-Maus sichtbar");
}

static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param) {
  switch (event) {
    case ESP_HIDD_EVENT_REG_FINISH: {
      if (param->init_finish.state == ESP_HIDD_INIT_OK) {
        Serial.println("HID device profile registered, bereit für Verbindung");
      } else {
        Serial.println("HID device profile register failed");
      }
      break;
    }
    case ESP_HIDD_EVENT_DEINIT_FINISH:
      break;
    case ESP_HIDD_EVENT_BLE_CONNECT: {
      Serial.println("✅ Bluetooth HID Maus verbunden!");
      Serial.println("HALTE den Button für 10kHz Auto-Clicking!");
      connected = true;
      break;
    }
    case ESP_HIDD_EVENT_BLE_DISCONNECT: {
      Serial.println("❌ Bluetooth HID Maus getrennt");
      connected = false;
      digitalWrite(LED_PIN, LOW);
      break;
    }
    case ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT: {
      Serial.printf("Vendor report empfangen, Länge = %d\n", param->vendor_write.length);
      break;
    }
    default:
      break;
  }
}

void sendMouseClick() {
  uint8_t mouse_report[4] = {0};
  
  // Mouse Click (linke Taste drücken)
  mouse_report[0] = 0x01; // Linke Maustaste
  mouse_report[1] = 0x00; // X-Movement
  mouse_report[2] = 0x00; // Y-Movement
  mouse_report[3] = 0x00; // Wheel
  
  esp_hidd_send_mouse_value(0, mouse_report, 4);
  
  // Minimal wait für sauberen Click
  delayMicroseconds(5);
  
  // Mouse Release (linke Taste loslassen)
  mouse_report[0] = 0x00; // Keine Taste gedrückt
  esp_hidd_send_mouse_value(0, mouse_report, 4);
}

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    command.toLowerCase();
    
    if (command == "status") {
      Serial.println("=== ESP32 Auto Mouse Status ===");
      Serial.println("Bluetooth: " + String(connected ? "VERBUNDEN ✅" : "GETRENNT ❌"));
      Serial.println("Button: " + String(digitalRead(BUTTON_PIN) == LOW ? "GEDRÜCKT" : "NICHT GEDRÜCKT"));
      Serial.println("Gerätename: " + deviceName);
      Serial.println("Modus: Button halten für 10kHz Clicking");
    }
    else if (command == "help" || command == "?") {
      Serial.println("=== ESP32 Auto Mouse Hilfe ===");
      Serial.println("VERWENDUNG:");
      Serial.println("1. Bluetooth-Maus in Systemeinstellungen verbinden");
      Serial.println("2. Button auf ESP32 GEDRÜCKT HALTEN für 10kHz Auto-Clicking");
      Serial.println("3. Button loslassen stoppt das Clicking");
      Serial.println("");
      Serial.println("SERIAL KOMMANDOS:");
      Serial.println("- status: Aktuellen Status anzeigen");
      Serial.println("- help: Diese Hilfe anzeigen");
      Serial.println("");
      Serial.println("LED BEDEUTUNG:");
      Serial.println("- 3x Blinken beim Start: Bluetooth bereit");
      Serial.println("- Dauerlicht: Button gedrückt und clicking aktiv");
      Serial.println("- Schnelles Blinken: Button gedrückt aber nicht verbunden");
      Serial.println("- Aus: Bereit/Idle");
    }
    else if (command == "test") {
      Serial.println("Teste Maus-Click...");
      if (connected) {
        sendMouseClick();
        Serial.println("Test-Click gesendet!");
      } else {
        Serial.println("Nicht verbunden - kann nicht testen");
      }
    }
  }
}
