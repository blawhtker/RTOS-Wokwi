/* Application: 06 - Final
   Theme: Theme Park Systems (Orlando)
   Author: Blake Whitaker
   UCFID: 543877
   Google Gemini used for some logic and commenting
   ADJUSTED SENSOR DELAY FROM 150MS TO 50MS AS SUGGESTED BY STUDENTS ON YELLOWDIG
*/

// Header files
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"

// Mapping hardware components to GPIO pins on the ESP32 
#define LED_SYSTEM_POWER              GPIO_NUM_5  // Yellow LED for the power/heartbeat indicator.
#define LED_EMERGENCY_BRAKE           GPIO_NUM_4  // Red LED for the fault/emergency brake indicator.
#define LED_ALL_CLEAR                 GPIO_NUM_19 // Green LED for the 'all clear' and ready to restart indicator.
#define BUTTON_EMERGENCY_STOP         GPIO_NUM_18 // Emergency brake and system restart.
#define PROXIMITY_SENSOR_TRIG_PIN     GPIO_NUM_17 // Trigger (output) pin for the ultrasonic sensor.
#define PROXIMITY_SENSOR_ECHO_PIN     GPIO_NUM_16 // Echo (input) pin for the ultrasonic sensor.

// System Constants 
#define PROXIMITY_THRESHOLD_CM 30          // The safety threshold; @10mph->need ~1.5ft; @25mph-> ~10.5 ft; @40-> ~27ft. 
#define BUTTON_DEBOUNCE_TIME_MS 200         // The time in milliseconds to ignore subsequent button presses to prevent 'bouncing'.

// Global variables to hold references to the RTOS objects we create 
SemaphoreHandle_t sem_emergency_stop_button; // Handle for the binary semaphore signaled by the E-Stop ISR.
SemaphoreHandle_t sem_train_proximity_event; // Handle for the binary semaphore signaled by the sensor task.

// State Machine 
// A custom data type (enum) to represent the possible states of the ride system.
typedef enum {
    RIDE_ALL_CLEAR,      // Normal operation.
    HALTED_BY_PROXIMITY, // Halted due to a proximity obstruction.
    HALTED_BY_ESTOP,     // Halted due to a human operator pressing the restart button.
    AWAITING_RESTART     // Obstruction cleared, waiting for operator confirmation to restart.
} RideStatus;

// 'volatile' keyword tells the compiler that a variable's value can be changed by an external source
// (like another task or an ISR) and prevents optimizations that could lead to using a stale value.
volatile RideStatus ride_status = RIDE_ALL_CLEAR; // The main global variable that holds the current state of the ride.
volatile bool is_train_in_zone = false;           // A flag indicating if an obstruction is currently within the threshold.
volatile int current_proximity_cm = 999;          // Stores the latest proximity reading from the sensor task.
volatile int64_t last_isr_time_us = 0;            // Stores the timestamp of the last valid E-Stop interrupt for debouncing.

// Forward Declarations for Tasks & an ISR 
void system_power_monitor_task(void *pvParameters);
void train_sensor_monitor_task(void *pvParameters);
void status_output_task(void* pvParameters);
static void IRAM_ATTR gpio_isr_handler(void* arg);

// [Hard Real-Time] Processes safety events and manages the ride state machine logic.
void ride_control_handler_task(void *pvParameters) {
    while (1) { // Loop forever to continuously manage the ride's state.
        // State Transition Logic 

        // 1. A proximity event can halt a ride that is running OR awaiting restart.
        if ((ride_status == RIDE_ALL_CLEAR || ride_status == AWAITING_RESTART) && xSemaphoreTake(sem_train_proximity_event, 0) == pdTRUE) {
            ride_status = HALTED_BY_PROXIMITY;
            gpio_set_level(LED_EMERGENCY_BRAKE, 1);
            gpio_set_level(LED_ALL_CLEAR, 0);
        }
        
        // 2. An E-Stop event is processed based on the current ride status.
        if (xSemaphoreTake(sem_emergency_stop_button, 0) == pdTRUE) { // Check if the E-Stop ISR gave the semaphore.
            switch (ride_status) { // Use a switch statement to handle the logic for each possible state.
                // If ride is running or halted by proximity, an E-Stop press engages the E-Stop.
                case RIDE_ALL_CLEAR:
                case HALTED_BY_PROXIMITY:
                    ride_status = HALTED_BY_ESTOP;
                    gpio_set_level(LED_EMERGENCY_BRAKE, 1);
                    gpio_set_level(LED_ALL_CLEAR, 0);
                    break;
                
                // If ride is E-Stopped or waiting for a proximity-halt reset, a button press performs the reset.
                case HALTED_BY_ESTOP:
                case AWAITING_RESTART: // <<-- This case is now correctly grouped with the reset logic.
                    if (!is_train_in_zone) { // Final safety check: track must be clear to restart.
                        ride_status = RIDE_ALL_CLEAR;
                        gpio_set_level(LED_EMERGENCY_BRAKE, 0);
                        gpio_set_level(LED_ALL_CLEAR, 1);
                    }
                    break;
            }
        }

        // 3. Automatically transition from a PROXIMITY halt to AWAITING_RESTART if the obstruction clears.
        if (ride_status == HALTED_BY_PROXIMITY && !is_train_in_zone) {
            ride_status = AWAITING_RESTART;
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Pause task for 10ms to let other tasks run.
    }
}

// [Soft Real-Time] Provides periodic status updates based on the state machine.
void status_output_task(void* pvParameters) {
    while (1) { // Loop forever.
        const char* status_text;
        int proximity = current_proximity_cm;
        RideStatus current_status = ride_status;

        switch (current_status) {
            case HALTED_BY_PROXIMITY:
                status_text = "Obstruction - Ride Halted";
                break;
            case HALTED_BY_ESTOP:
                status_text = "Emergency Stop Activated";
                break;
            case AWAITING_RESTART:
                status_text = "Obstruction Cleared - Awaiting Restart";
                break;
            case RIDE_ALL_CLEAR:
            default:
                status_text = "All Clear";
                break;
        }

        // Print the formatted status line to the serial monitor.
        printf("[%lu] Proximity = %-4dcm  Status: %s\n", xTaskGetTickCount(), proximity, status_text);

        vTaskDelay(pdMS_TO_TICKS(250)); // Pause for 250ms to print status messages 4 times per second.
    }
}

// Main entry point of the application 
void app_main(void) {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << LED_SYSTEM_POWER) | (1ULL << LED_EMERGENCY_BRAKE) | (1ULL << LED_ALL_CLEAR);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);

    gpio_config_t btn_conf = {};
    btn_conf.pin_bit_mask = (1ULL << BUTTON_EMERGENCY_STOP);
    btn_conf.mode = GPIO_MODE_INPUT;
    btn_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    btn_conf.intr_type = GPIO_INTR_NEGEDGE;
    gpio_config(&btn_conf);

    gpio_set_direction(PROXIMITY_SENSOR_TRIG_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(PROXIMITY_SENSOR_ECHO_PIN, GPIO_MODE_INPUT);
    
    gpio_set_level(LED_EMERGENCY_BRAKE, 0);
    gpio_set_level(LED_ALL_CLEAR, 1);

    sem_emergency_stop_button = xSemaphoreCreateBinary();
    sem_train_proximity_event = xSemaphoreCreateBinary();

    xTaskCreate(system_power_monitor_task, "SystemPower", 2048, NULL, 1, NULL);
    xTaskCreate(train_sensor_monitor_task, "TrainSensor", 2048, NULL, 2, NULL);
    xTaskCreate(ride_control_handler_task, "RideControl", 2048, NULL, 3, NULL);
    xTaskCreate(status_output_task, "StatusOutput", 2048, NULL, 1, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_EMERGENCY_STOP, gpio_isr_handler, (void*) BUTTON_EMERGENCY_STOP);
}

// The ISR handler function for the E-Stop button.
static void IRAM_ATTR gpio_isr_handler(void* arg) {
    int64_t current_time_us = esp_timer_get_time();
    if ((current_time_us - last_isr_time_us) > (BUTTON_DEBOUNCE_TIME_MS * 1000)) {
        last_isr_time_us = current_time_us;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(sem_emergency_stop_button, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

// [Hard Real-Time] Monitors the track proximity sensor.
void train_sensor_monitor_task(void *pvParameters) {
    bool train_in_zone_prev = false;
    while (1) {
        gpio_set_level(PROXIMITY_SENSOR_TRIG_PIN, 0);
        ets_delay_us(2);
        gpio_set_level(PROXIMITY_SENSOR_TRIG_PIN, 1);
        ets_delay_us(10);
        gpio_set_level(PROXIMITY_SENSOR_TRIG_PIN, 0);
        
        while (gpio_get_level(PROXIMITY_SENSOR_ECHO_PIN) == 0);
        int64_t start_time = esp_timer_get_time();
        while (gpio_get_level(PROXIMITY_SENSOR_ECHO_PIN) == 1);
        int64_t end_time = esp_timer_get_time();

        long duration_us = end_time - start_time;
        int proximity_cm = (int)(duration_us * 0.0343 / 2.0);
        current_proximity_cm = proximity_cm;
        bool train_in_zone_now = (proximity_cm > 0 && proximity_cm < PROXIMITY_THRESHOLD_CM);
        is_train_in_zone = train_in_zone_now;
        
        if (train_in_zone_now && !train_in_zone_prev) {
            xSemaphoreGive(sem_train_proximity_event);
        }
        train_in_zone_prev = train_in_zone_now;
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// [Soft Real-Time] Blinks the control panel's power LED.
void system_power_monitor_task(void *pvParameters) {
    while (1) {
        gpio_set_level(LED_SYSTEM_POWER, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(LED_SYSTEM_POWER, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
