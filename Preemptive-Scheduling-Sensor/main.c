/* --------------------------------------------------------------
   Blake Whitaker UCFID:5438770
   Application: 02 - Preemptive Scheduling with Sensor Integration (FreeRTOS on ESP32)
   Release Type: Baseline Multitask Skeleton Starter Code (Modified)
   Class: Real Time Systems - Su 2025
---------------------------------------------------------------*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

// TODO1: ADD IN additional INCLUDES BELOW
#include "driver/adc.h"
// TODO1: ADD IN additional INCLUDES ABOVE
#include "math.h"

#define LED_PIN GPIO_NUM_2  // Using GPIO2 for the LED

// TODO2: ADD IN LDR_PIN to gpio pin 32
#define LDR_PIN GPIO_NUM_32
// TODO3: ADD IN LDR_ADC_CHANNEL -- if you used gpio pin 32 it should map to ADC1_CHANNEL4
#define LDR_ADC_CHANNEL ADC1_CHANNEL_4 
// TODO99: Consider Adding AVG_WINDOW and SENSOR_THRESHOLD as global defines
#define AVG_WINDOW 10
#define SENSOR_THRESHOLD_LUX 100 // Threshold for lux warning


//TODO9: Adjust Task to blink an LED at 1 Hz (1000 ms period: 500 ms ON, 500 ms OFF);
//Consider supressing the output
void led_task(void *pvParameters) {
    bool led_status = false;
    
    while (1) {
        led_status = !led_status;  //TODO: toggle state for next loop 
        gpio_set_level(LED_PIN, led_status);  //TODO: Set LED pin high or low based on led_status flag;
        
        // Thematic output for a space systems beacon.
        printf("SATELLITE BEACON: %s\n", led_status ? "ON" : "OFF");
        vTaskDelay(pdMS_TO_TICKS(500)); // Delay for 500 ms
    }
    vTaskDelete(NULL); // We'll never get here; tasks run forever
}

//TODO10: Task to print a message every 1000 ms (1 seconds)
void print_status_task(void *pvParameters) {
    TickType_t currentTime = pdTICKS_TO_MS( xTaskGetTickCount() );
    TickType_t previousTime = 0;
    while (1) {
        previousTime = currentTime;
        currentTime = pdTICKS_TO_MS( xTaskGetTickCount() );
        
        // Prints periodic thematic message. Output a timestamp (ms) and period (ms)
        printf("TELEMETRY UPLINK: OK. Timestamp: %lu ms. Period: %lu ms.\n",currentTime, currentTime-previousTime);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1000 ms
    }
    vTaskDelete(NULL); // We'll never get here; tasks run forever
}

//TODO11: Create new task for sensor reading every 500ms
void sensor_task(void *pvParameters) {
    //TODO110 Configure ADC (12-bit width, 0-3.3V range with 11dB attenuation)
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(LDR_ADC_CHANNEL, ADC_ATTEN_DB_11);

    // Variables to compute LUX
    int raw;
    float Rmeasured = 0.;
    float lux = 0.;
    // Variables for moving average
    float luxreadings[AVG_WINDOW] = {0};
    int idx = 0;
    float sum = 0;

    //TODO11a consider where AVG_WINDOW is defined, it could be here, or global value 
    // Defined globally via #define

    // Pre-fill the readings array with an initial sample to avoid startup anomaly
    for(int i = 0; i < AVG_WINDOW; ++i) {
        raw = adc1_get_raw(LDR_ADC_CHANNEL);
        // Using simplified equation R = (10000 * raw) / (4095 - raw)
        if (4095 - raw == 0) Rmeasured = 0; // Avoid division by zero
        else Rmeasured = (10000.0 * raw) / (4095.0 - raw); //TODO11b/c correct this with the equation seen earlier
        // Using formula lux = pow(50000 / R, 1/gamma) where gamma=0.7
        if (Rmeasured <= 0) lux = 0; // Avoid division by zero or log of non-positive
        else lux = powf(50000.0 / Rmeasured, 1.0 / 0.7); //TODO11d correct this with the equation seen earlier
        
        luxreadings[i] = lux;
        sum += luxreadings[i];
        vTaskDelay(pdMS_TO_TICKS(50)); // Small delay to allow sensor to settle
    }

    const TickType_t periodTicks = pdMS_TO_TICKS(500); // 500 ms period
    TickType_t lastWakeTime = xTaskGetTickCount(); // initialize last wake time

    while (1) {
        // Read current sensor value
        raw = adc1_get_raw(LDR_ADC_CHANNEL);
        
        // Compute LUX
        // Using simplified equation R = (10000 * raw) / (4095 - raw)
        if (4095 - raw == 0) Rmeasured = 0; // Avoid division by zero
        else Rmeasured = (10000.0 * raw) / (4095.0 - raw); //TODO11e/f correct this with the equation seen earlier
        // Using formula lux = pow(50000 / R, 1/gamma) where gamma=0.7
        if (Rmeasured <= 0) lux = 0;
        else lux = powf(50000.0 / Rmeasured, 1.0 / 0.7); //TODO11g correct this with the equation seen earlier
       
        // Update moving average buffer 
        sum -= luxreadings[idx];       // remove oldest value from sum
        luxreadings[idx] = lux;        // place new reading
        sum += lux;                 // add new value to sum
        idx = (idx + 1) % AVG_WINDOW;
        int avg_lux = (int)(sum / AVG_WINDOW); // compute average

        //TODO11h Check threshold and print alert if exceeded or below based on context
        // Space theme: Alert if solar intensity drops, indicating possible eclipse.
        if (avg_lux < SENSOR_THRESHOLD_LUX) {
            printf("ALERT!: Solar Intensity Low!. Avg Lux: %d\n", avg_lux);
        } else {
          //TODO11i
          // Print the avg value for debugging and status confirmation
          printf("SOLAR SENSOR: OK. Avg Lux: %d\n", avg_lux);
        }
        
        //TODO11j: Print out time period [to help with answering Eng/Analysis quetionst (hint check Application Solution #1 )
        //This printout is implicitly handled by the print_status_task, but we ensure this task runs on schedule.
        
        //TODO11k Replace vTaskDelay with vTaskDelayUntil with parameters &lastWakeTime and periodTicks
        vTaskDelayUntil(&lastWakeTime, periodTicks);
    }
}


void app_main() {
    // Initialize LED GPIO      
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    
    // TODO4 : Initialize LDR PIN as INPUT [2 lines mirroring those above]
    gpio_reset_pin(LDR_PIN);
    gpio_set_direction(LDR_PIN, GPIO_MODE_INPUT);
 
    // TODO5 : Set ADC1's resolution by calling:
    // function adc1_config_width(...) 
    // with parameter ADC_WIDTH_BIT_12
    adc1_config_width(ADC_WIDTH_BIT_12);

    // TODO6: Set the the input channel to 11 DB Attenuation using
    // function adc1_config_channel_atten(...,...) 
    // with parameters LDR_ADC_CHANNEL and ADC_ATTEN_DB_11
    adc1_config_channel_atten(LDR_ADC_CHANNEL, ADC_ATTEN_DB_11);

    // Instantiate/ Create tasks: 
    // ... (omitting descriptive comments for brevity)
    
    // TODO7: Pin tasks to core 1    
    // ... (omitting descriptive comments for brevity)

    // Priorities: SENSOR (2-High), STATUS (1-Medium), LED (0-Low)
    xTaskCreatePinnedToCore(led_task, "LED", 2048, NULL, 0, NULL, 1);
    xTaskCreatePinnedToCore(print_status_task, "STATUS", 2048, NULL, 1, NULL, 1);

    // TODO8: Make sure everything still works as expected before moving on to TODO9 (above).

    //TODO12 Add in new Sensor task; make sure it has the correct priority to preempt 
    //the other two tasks. (Stack increased for floating point math).
    xTaskCreatePinnedToCore(sensor_task, "SENSOR", 4096, NULL, 2, NULL, 1);

    //TODO13: Make sure the output is working as expected and move on to the engineering
    //and analysis part of the application. You may need to make modifications for experiments. 
    //Make sure you can return back to the working version!
}