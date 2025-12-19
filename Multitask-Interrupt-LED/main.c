#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define STATUS_BEACON_PIN GPIO_NUM_4  // Using GPIO4 for the Status Beacon LED

// Task to blink an LED at 2 Hz (500 ms period: 250 ms ON, 250 ms OFF)
void status_beacon_controller_task(void *pvParameters) {
    bool is_beacon_active = false; // Tracks if the beacon is currently active (lit)
    while (1) {
        // TODO: Set LED pin high or low based on led_on flag; right now it's always on... boring; hint in the commented out print statement
        gpio_set_level(STATUS_BEACON_PIN, is_beacon_active ? 1 : 0);
        is_beacon_active = !is_beacon_active;  // toggle state for next time
        // Optional: printf("LED %s\n", led_on ? "ON" : "OFF");
        
        vTaskDelay(pdMS_TO_TICKS(250)); // Delay for 250 ms using MS to Ticks Function vs alternative which is MS / ticks per ms
    }
    vTaskDelete(NULL); // We'll never get here; tasks run forever
}

// Task to print a message every 10000 ms (10 seconds)
void telemetry_transmit_task(void *pvParameters) { 
    while (1) {
      // TODO: Print a periodic message based on thematic area. Could be a counter or timestamp.
        printf("Telemetry Uplink: OK. Satellite Uptime: %lu ms\n", 
               (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS));       
        vTaskDelay(pdMS_TO_TICKS(10000)); // Delay for 10000 ms
    }
    vTaskDelete(NULL); // We'll never get here; tasks run forever
}

void app_main() {
    // Initialize LED GPIO 
    gpio_reset_pin(STATUS_BEACON_PIN);
    gpio_set_direction(STATUS_BEACON_PIN, GPIO_MODE_OUTPUT);
    
    // Instantiate/ Create tasks: 
    // . pointer to task function, 
    // . descriptive name, [has a max length; located in the FREERTOS_CONFIG.H]
    // . stack depth, 
    // . parameters [optional] = NULL 
    // . priority [0 = low], 
    // . pointer referencing this created task [optional] = NULL
    // Learn more here https://www.freertos.org/Documentation/02-Kernel/04-API-references/01-Task-creation/01-xTaskCreate
    xTaskCreate(status_beacon_controller_task, "StatusBeaconCtrl", 2048, NULL, 1, NULL);
    xTaskCreate(telemetry_transmit_task, "TelemetryTx", 2048, NULL, 1, NULL); // Example rename for print_task
}
