/* --------------------------------------------------------------
   Application: 05 - Space Systems [Final - Rearchitected]
   Class: Real Time Systems - Su 2025
   Author: Blake Whitaker
   UCFID: 5438770
   Company: [University of Central Florida]

   AI Use: Google Gemini was used as a dev partner to generate thematic ideas
   and documentation based on the provided project requirements. Unable to fix 
   stability issues, the program appears to crash then restart when radiation 
   sensor is too high, then attempts to restart but gets hung.
---------------------------------------------------------------*/

// Standard & FreeRTOS Libraries
#include <WiFi.h>
#include <WebServer.h>
#include <uri/UriBraces.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

// --- Mission Configuration ---
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define WIFI_CHANNEL 6

// Hardware Pins
#define RAD_SENSOR_PIN 34
#define GREEN_STATUS_LED 26
#define RED_ALERT_LED 27
#define MODE_BUTTON_PIN 12

// System Parameters
#define RADIATION_THRESHOLD 3000

// --- Global Handles & State Variables ---
WebServer server(80);
SemaphoreHandle_t sensorAlertSemaphore;
SemaphoreHandle_t modeChangeSemaphore;
SemaphoreHandle_t logMutex;
QueueHandle_t sensorDataQueue;
enum SystemMode { NORMAL, SHIELDED };
volatile SystemMode currentMode = NORMAL;

// --- Utility Functions ---
void log_message(const char* message) {
  if (xSemaphoreTake(logMutex, portMAX_DELAY) == pdTRUE) {
    Serial.printf("[%lu] %s\n", xTaskGetTickCount(), message);
    xSemaphoreGive(logMutex);
  }
}

// --- Web Interface ---
void sendHtml() {
  String response = R"(
    <!DOCTYPE html><html><head>
    <title>Radiation Monitor</title>
    <meta http-equiv="refresh" content="5">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      html { font-family: 'Courier New', monospace; text-align: center; background-color:#111; color:#0F0;}
      h1, h2 { margin: 0.5em; }
      .card { background-color:#222; padding: 1em; border: 1px solid #0F0; margin: 1em auto; max-width: 400px; }
      .btn { display:inline-block; background-color:#050; border:1px solid #0F0; color:#0F0; padding:0.8em 1.5em; text-decoration:none; font-size:1.2em; margin-top:1em;}
      .btn:hover { background-color:#0A0; }
      .status { font-weight:bold; }
      .status.normal { color:#0F0; }
      .status.shielded { color:#FF0; }
      .status.alert { color:#F00; animation: blinker 1s linear infinite; }
      @keyframes blinker { 50% { opacity: 0; } }
    </style></head><body>
    <h1>[ESP32 Radiation Monitor]</h1>
    <div class="card">
      <h2>System Status</h2>
      <p>Current Mode: <span class="status MODE_CLASS">MODE_TEXT</span></p>
      <p>Radiation Level: SENSOR_VALUE</p>
      <a href="/toggle_mode" class="btn">Toggle Shielding</a>
    </div></body></html>
  )";

  int sensorSnapshot = 0;
  xQueuePeek(sensorDataQueue, &sensorSnapshot, 0);

  SystemMode modeSnapshot = currentMode;
  
  response.replace("SENSOR_VALUE", String(sensorSnapshot));

  if (sensorSnapshot > RADIATION_THRESHOLD) {
    response.replace("MODE_CLASS", "alert");
    response.replace("MODE_TEXT", "ALERT - HIGH RADIATION");
  } else if (modeSnapshot == SHIELDED) {
    response.replace("MODE_CLASS", "shielded");
    response.replace("MODE_TEXT", "SHIELDED");
  } else {
    response.replace("MODE_CLASS", "normal");
    response.replace("MODE_TEXT", "NORMAL");
  }

  server.send(200, "text/html", response);
}


// --- FreeRTOS Application Tasks ---
void webServerTask(void *pvParameters){
  for(;;){
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void heartbeatTask(void *pvParameters) {
  for (;;) {
    digitalWrite(GREEN_STATUS_LED, HIGH);
    vTaskDelay(pdMS_TO_TICKS(1000));
    digitalWrite(GREEN_STATUS_LED, LOW);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void sensorMonitorTask(void *pvParameters) {
  for (;;) {
    int sensorValue = analogRead(RAD_SENSOR_PIN);
    xQueueOverwrite(sensorDataQueue, &sensorValue);
    if (sensorValue > RADIATION_THRESHOLD) {
      xSemaphoreGive(sensorAlertSemaphore);
    }
    vTaskDelay(pdMS_TO_TICKS(17));
  }
}

void buttonWatchTask(void *pvParameters) {
  // **BUG FIX 1:** Initialize last state to the current pin reading to prevent false trigger on boot.
  int lastButtonState = digitalRead(MODE_BUTTON_PIN);
  for (;;) {
    int currentButtonState = digitalRead(MODE_BUTTON_PIN);
    if (lastButtonState == HIGH && currentButtonState == LOW) {
      vTaskDelay(pdMS_TO_TICKS(50));
      currentButtonState = digitalRead(MODE_BUTTON_PIN);
      if (currentButtonState == LOW) {
        log_message("Physical button pressed. Signaling mode change.");
        xSemaphoreGive(modeChangeSemaphore);
      }
    }
    lastButtonState = currentButtonState;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void eventResponseTask(void *pvParameters) {
  QueueSetHandle_t eventSet = xQueueCreateSet(1 + 1);
  xQueueAddToSet(sensorAlertSemaphore, eventSet);
  xQueueAddToSet(modeChangeSemaphore, eventSet);

  for (;;) {
    QueueSetMemberHandle_t activeSemaphore = xQueueSelectFromSet(eventSet, portMAX_DELAY);

    if (activeSemaphore == sensorAlertSemaphore) {
      if (xSemaphoreTake(sensorAlertSemaphore, 0) == pdTRUE) {
        log_message("CRITICAL: High radiation event received!");
        for (int i = 0; i < 5; i++) {
          digitalWrite(RED_ALERT_LED, HIGH);
          vTaskDelay(pdMS_TO_TICKS(100));
          digitalWrite(RED_ALERT_LED, LOW);
          vTaskDelay(pdMS_TO_TICKS(100));
        }
      }
    }
    
    if (activeSemaphore == modeChangeSemaphore) {
      if (xSemaphoreTake(modeChangeSemaphore, 0) == pdTRUE) {
        currentMode = (currentMode == NORMAL) ? SHIELDED : NORMAL;
        if (currentMode == SHIELDED) {
            log_message("Mode changed to SHIELDED.");
            digitalWrite(RED_ALERT_LED, HIGH);
        } else {
            log_message("Mode changed to NORMAL.");
            digitalWrite(RED_ALERT_LED, LOW);
        }
      }
    }
  }
}

// --- Initializer Task ---
void systemInitTask(void *pvParameters) {
  logMutex = xSemaphoreCreateMutex();
  sensorAlertSemaphore = xSemaphoreCreateCounting(10, 0);
  modeChangeSemaphore = xSemaphoreCreateBinary();
  sensorDataQueue = xQueueCreate(1, sizeof(int));
  
  log_message("System boot. Initializing...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);
  Serial.print("Connecting to ground control network ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
    Serial.print(".");
  }
  Serial.println(" Link established!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/", []() { sendHtml(); });
  server.on("/toggle_mode", []() {
      log_message("Remote command received. Signaling mode change.");
      xSemaphoreGive(modeChangeSemaphore);
      sendHtml();
  });
  server.begin();
  log_message("HTTP command interface online.");

  log_message("Starting application tasks...");
  xTaskCreatePinnedToCore(heartbeatTask, "Heartbeat", 1024, NULL, 0, NULL, 0);
  xTaskCreatePinnedToCore(sensorMonitorTask, "SensorMonitor", 2048, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(buttonWatchTask, "ButtonWatch", 2048, NULL, 3, NULL, 0);
  // **BUG FIX 2:** Increased stack size for the event response task to prevent stack overflow.
  xTaskCreatePinnedToCore(eventResponseTask, "EventResponse", 8192, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(webServerTask, "WebServer", 4096, NULL, 1, NULL, 1);

  log_message("Initialization complete. Deleting init task.");
  vTaskDelete(NULL);
}


// --- Main Arduino Setup and Loop ---
void setup() {
  Serial.begin(115200);

  pinMode(GREEN_STATUS_LED, OUTPUT);
  pinMode(RED_ALERT_LED, OUTPUT);
  pinMode(RAD_SENSOR_PIN, INPUT);
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
  
  xTaskCreatePinnedToCore(systemInitTask, "SystemInit", 8192, NULL, 2, NULL, 1);
}

void loop() {
  vTaskDelete(NULL);
}