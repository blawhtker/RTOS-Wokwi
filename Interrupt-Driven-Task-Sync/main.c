#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "math.h"

// Hardware Pin Definitions
#define LED_PIN GPIO_NUM_2          // On-board LED or external LED
#define LDR_PIN GPIO_NUM_34         // LDR connected to GPIO34 (ADC1_CHANNEL_6)
#define BUTTON_PIN GPIO_NUM_4       // Push-button for interrupt

// ADC Configuration
#define LDR_ADC_CHANNEL ADC1_CHANNEL_6 // ADC channel for GPIO34

// Task & Buffer Configuration
#define LOG_BUFFER_SIZE 50          // Store the last 50 sensor readings

// Global Variables
SemaphoreHandle_t xButtonSem;       // Binary semaphore for button press ISR
SemaphoreHandle_t xLogMutex;        // Mutex to protect the shared log buffer
int lightSensorLog[LOG_BUFFER_SIZE]; // Buffer to store raw sensor readings
int logIndex = 0;                   // Current index for the circular buffer


void IRAM_ATTR button_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // Give the semaphore to notify the waiting task
    xSemaphoreGiveFromISR(xButtonSem, &xHigherPriorityTaskWoken);
    // If giving the semaphore woke a higher-priority task, yield immediately
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}


void SatelliteHeartbeatTask(void *pvParameters) {
    bool led_status = false;
    while (1) {
        led_status = !led_status;
        gpio_set_level(LED_PIN, led_status);
        vTaskDelay(pdMS_TO_TICKS(1400)); // Blink every 1.4 seconds
    }
}


void TelemetryTransmitTask(void *pvParameters) {
    while (1) {
        printf("TELEMETRY UPLINK: System status nominal. Timestamp: %lu ms.\n", pdTICKS_TO_MS(xTaskGetTickCount()));
        vTaskDelay(pdMS_TO_TICKS(7000)); // Run every 7 seconds
    }
}

void SolarPanelMonitorTask(void *pvParameters) {
    int raw_value;
    const TickType_t periodTicks = pdMS_TO_TICKS(200); // Sample every 200 ms
    TickType_t lastWakeTime = xTaskGetTickCount();

    while (1) {
        raw_value = adc1_get_raw(LDR_ADC_CHANNEL);

        // Safely update the shared log buffer
        if (xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
            lightSensorLog[logIndex] = raw_value;
            logIndex = (logIndex + 1) % LOG_BUFFER_SIZE; // Wrap around the buffer
            xSemaphoreGive(xLogMutex);
        }

        // Use vTaskDelayUntil for precise periodic execution
        vTaskDelayUntil(&lastWakeTime, periodTicks);
    }
}


void GroundCommandTask(void *pvParameters) {
    for (;;) {
        // Wait indefinitely for the semaphore from the ISR (consumes no CPU while waiting)
        if (xSemaphoreTake(xButtonSem, portMAX_DELAY) == pdTRUE) {
            printf("\n--- COMMAND RECEIVED ---\n");
            printf("ACTION: Compressing and dumping sensor logs...\n");
            
            // Local variables for processing
            int min_val = 4095, max_val = 0;
            long long sum = 0;
            float avg_val = 0;
            int local_log[LOG_BUFFER_SIZE];
            int current_log_index;

            if (xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
                memcpy(local_log, lightSensorLog, sizeof(lightSensorLog));
                current_log_index = logIndex; // Capture the current index
                xSemaphoreGive(xLogMutex);
            }

            // Calculate min, max, and average
            for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
                if (local_log[i] < min_val) min_val = local_log[i];
                if (local_log[i] > max_val) max_val = local_log[i];
                sum += local_log[i];
            }
            avg_val = (float)sum / LOG_BUFFER_SIZE;

            // Print the compressed log dump
            printf("LOG DATA (last %d readings):\n", LOG_BUFFER_SIZE);
            printf("  -> min: %d", min_val);
            printf("  -> max: %d", max_val);
            printf("  -> avg: %.2f\n", avg_val);
            printf("--- END OF TRANSMISSION ---\n\n");
        }
    }
}

void app_main() {

    // Configure LED Pin
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    // Configure LDR ADC Pin
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(LDR_ADC_CHANNEL, ADC_ATTEN_DB_11);

    // Configure Button Pin for Interrupt
    gpio_reset_pin(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY); // Use internal pull-up
    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_NEGEDGE); // Trigger on falling edge (press)
    
    // Create a binary semaphore for the button ISR
    xButtonSem = xSemaphoreCreateBinary();
    // Create a mutex for protecting the shared log buffer
    xLogMutex = xSemaphoreCreateMutex();

    gpio_install_isr_service(0);
    // Attach the ISR handler to the button pin
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);

    // All tasks are pinned to Core 1    
    // Priority 1 (Low): Background tasks
    xTaskCreatePinnedToCore(SatelliteHeartbeatTask, "Heartbeat", 2048, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(TelemetryTransmitTask, "Telemetry", 4096, NULL, 1, NULL, 1);
    
    // Priority 2 (Medium): Periodic data sampling
    xTaskCreatePinnedToCore(SolarPanelMonitorTask, "SolarMonitor", 4096, NULL, 2, NULL, 1);

    // Priority 3 (High): High-priority event-driven task
    xTaskCreatePinnedToCore(GroundCommandTask, "GroundCmd", 4096, NULL, 3, NULL, 1);

    printf("RTOS Application 3 Initialized. System is operational.\n");
}
