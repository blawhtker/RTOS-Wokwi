#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_log.h"

//TODO 8 - Update the code variables and comments to match your selected thematic area!
// Space Systems Scenario: Monitor radiation levels and respond to ground-control commands.

//TODO 0a connect the components in the diagram to the GPIO pins listed below.
//Note even if Wokwi won't blow your LEDs, we'll assume them to be off if they're not connected to resistors!
//Next, goto TODO 0b
#define LED_SYSTEM_STATUS   GPIO_NUM_5  // Green LED: Indicates system uplink operational status 
#define LED_RADIATION_ALERT GPIO_NUM_4  // Red LED: Alerts for high radiation levels or system events
#define BUTTON_GROUND_CONTROL GPIO_NUM_18 // Button: Simulates ground control command input
#define RADIATION_SENSOR_ADC_CHANNEL ADC1_CHANNEL_6 // GPIO34: Analog input for potentiometer radiation sensor 

// Maximum count for the counting semaphore for radiation events
// A sensor reading occurs every 100ms. Over 30 seconds = 300 events.
// Setting MAX_COUNT_SEM to 300 ensures all events are captured if the threshold is continuously exceeded.
#define MAX_COUNT_SEM 300
// TODO 7: Based ont the speed of events; 
//can you adjust this MAX Semaphore Counter to not miss a high frequency threshold events
//over a 30 second time period, e.g., assuming that the sensor exceeds the threshold of 30 seconds, 
//can you capture every event in your counting semaphore? what size do you need?



// Threshold for analog sensor
//TODO 1: Adjust threshold based on your scenario or input testing
// You should modify SENSOR_THRESHOLD to better match your Wokwi input behavior;
// note the min/max of the adc raw reading
#define RADIATION_THRESHOLD 3000
// Threshold for analog radiation sensor
// For a 12-bit ADC, raw readings range from 0 to 4095. 3000 is high threshold.

// Handles for semaphores and mutex - you'll initialize these in the main program
SemaphoreHandle_t sem_ground_control_button; // Binary semaphore for button presses
SemaphoreHandle_t sem_radiation_event;     // Counting semaphore for radiation threshold exceedances
SemaphoreHandle_t print_mutex;             // Mutex to protect console (UART) prints

volatile int RADIATION_EVENT_COUNT = 0; //You may not use this value in your logic -- but you can print it if you wish

// Static variables for debouncing and rising edge detection
static TickType_t last_button_press_time = 0;
const TickType_t BUTTON_DEBOUNCE_TIME_MS = 200; // 200ms debounce time
static bool radiation_threshold_exceeded_prev = false; // For rising edge detection of sensor events

//TODO 0b: Set heartbeat to cycle once per second (on for one second, off for one second)
//Find TODO 0c
// Task: System Status Monitor (Heartbeat)
// Blinks the green LED at 1Hz to indicate the system is operational.
void system_status_monitor_task(void *pvParameters) {
    while (1) {
        gpio_set_level(LED_SYSTEM_STATUS, 1); // Turn green LED on
        vTaskDelay(pdMS_TO_TICKS(1000));      // Stay on for 1 second
        gpio_set_level(LED_SYSTEM_STATUS, 0); // Turn green LED off
        vTaskDelay(pdMS_TO_TICKS(1000));      // Stay off for 1 second
    }
}


void radiation_sensor_monitor_task(void *pvParameters) {
    while (1) {
        int current_radiation_level = adc1_get_raw(RADIATION_SENSOR_ADC_CHANNEL);

        //TODO 2: Add serial print to log the raw sensor value (mutex protected)
        //Hint: use xSemaphoreTake( ... which semaphore ...) and printf
        // Protect console print with a mutex to prevent garbled output
        xSemaphoreTake(print_mutex, portMAX_DELAY);
        printf("Radiation Sensor: Current Level = %d\n", current_radiation_level);
        xSemaphoreGive(print_mutex);

        // Check if radiation level exceeds threshold and detect rising edge
        if (current_radiation_level > RADIATION_THRESHOLD) {
            //TODO 3: prevent spamming by only signaling on rising edge; See prior application #3 for help!
            if (!radiation_threshold_exceeded_prev) { // Rising edge detected
                if(RADIATION_EVENT_COUNT < MAX_COUNT_SEM) { // Prevent overflow of the counter for display purposes
                    RADIATION_EVENT_COUNT++;
                }
                xSemaphoreGive(sem_radiation_event); // Signal a radiation event
            }
            radiation_threshold_exceeded_prev = true; // Update previous state
        } else {
            radiation_threshold_exceeded_prev = false; // Reset previous state if below threshold
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Read sensor every 100ms
    }
}


void ground_control_button_watch_task(void *pvParameters) {
    while (1) {
        int button_state = gpio_get_level(BUTTON_GROUND_CONTROL);
        TickType_t current_ticks = xTaskGetTickCount();

        // TODO 4a: Add addtional logic to prevent bounce effect (ignore multiple events for 'single press')
        // You must do it in code - not by modifying the wokwi simulator button
        if (button_state == 0) { // Button is pressed
            if ((current_ticks - last_button_press_time) > pdMS_TO_TICKS(BUTTON_DEBOUNCE_TIME_MS)) {
                // Valid button press after debounce period
                xSemaphoreGive(sem_ground_control_button); // Signal ground control button event

                //TODO 4b: Add a console print indicating button was pressed (mutex protected); different message than in event handler
                // Protect console print with a mutex
                xSemaphoreTake(print_mutex, portMAX_DELAY);
                printf("Ground Control: Command button pressed!\n");
                xSemaphoreGive(print_mutex);

                last_button_press_time = current_ticks; // Update last press time
            }
        }
            
        vTaskDelay(pdMS_TO_TICKS(10)); // Do Not Modify This Delay! (Frequent polling for responsiveness)
    }
}

void system_event_handler_task(void *pvParameters) {
    while (1) {
        if (xSemaphoreTake(sem_radiation_event, 0)) { // Non-blocking check
            RADIATION_EVENT_COUNT--; // Decrement event counter

            xSemaphoreTake(print_mutex, portMAX_DELAY);
            printf("Radiation Alert: Threshold exceeded! Total events: %d\n", RADIATION_EVENT_COUNT);
            xSemaphoreGive(print_mutex);

            gpio_set_level(LED_RADIATION_ALERT, 1); // Turn red LED on
            vTaskDelay(pdMS_TO_TICKS(100));         // Stay on briefly
            gpio_set_level(LED_RADIATION_ALERT, 0); // Turn red LED off
        }

        if (xSemaphoreTake(sem_ground_control_button, 0)) { // Non-blocking check
            xSemaphoreTake(print_mutex, portMAX_DELAY);
            printf("System Response: Processing ground control command...\n");
            xSemaphoreGive(print_mutex);

            gpio_set_level(LED_RADIATION_ALERT, 1); // Turn red LED on for longer
            vTaskDelay(pdMS_TO_TICKS(300));         // Indicate system response
            gpio_set_level(LED_RADIATION_ALERT, 0); // Turn red LED off
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Idle delay to yield CPU and prevent busy-waiting
    }
}

void app_main(void) {
    // Configure output LEDs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_SYSTEM_STATUS) | (1ULL << LED_RADIATION_ALERT),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    // Configure input button
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GROUND_CONTROL),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE // Enable internal pull-up resistor
    };
    gpio_config(&btn_conf);

    // Configure ADC
    adc1_config_width(ADC_WIDTH_BIT_12); // Set ADC to 12-bit resolution (0-4095)
    adc1_config_channel_atten(RADIATION_SENSOR_ADC_CHANNEL, ADC_ATTEN_DB_11); // Full range attenuation

    // Create sync primitives
    // TODO 0c: Attach the three SemaphoreHandle_t defined earlier 
    // (sem_button, sem_sensor, print_mutex) to appropriate Semaphores.
    // binary, counting, mutex by using the appropriate xSemaphoreCreate APIs.
    // the counting semaphore should be set to (MAX_COUNT_SEM,0);
    // Move on to TODO 1; remaining TODOs are numbered 1,2,3, 4a 4b, 5, 6 ,7
    sem_ground_control_button = xSemaphoreCreateBinary(); // Binary semaphore for button events
    sem_radiation_event = xSemaphoreCreateCounting(MAX_COUNT_SEM, 0); // Counting semaphore for sensor events
    print_mutex = xSemaphoreCreateMutex(); // Mutex for protecting shared console output


    //TODO 5: Test removing the print_mutex around console output (expect interleaving)
    //Observe console when two events are triggered close together

    // Create tasks
    xTaskCreate(system_status_monitor_task, "SystemStatus", 2048, NULL, 1, NULL); // Lowest priority
    xTaskCreate(radiation_sensor_monitor_task, "RadiationSensor", 2048, NULL, 2, NULL);
    xTaskCreate(ground_control_button_watch_task, "GroundControlBtn", 2048, NULL, 3, NULL); // Highest priority
    xTaskCreate(system_event_handler_task, "EventHandler", 2048, NULL, 2, NULL);

    //TODO 6: Experiment with changing task priorities to induce or fix starvation
    //E.G> Try: xTaskCreate(sensor_task, ..., 4, ...) and observe heartbeat blinking
    //You should do more than just this example ...

}
