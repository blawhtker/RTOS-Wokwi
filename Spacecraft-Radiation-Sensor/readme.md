1. Migration Strategy & Refactor Map
The three largest structural changes made when porting the sketch were the decentralization of control, 
the shift to event-driven signaling, and the introduction of resource protection. The monolithic loop() 
function was replaced with a multi-tasking architecture of concurrent, independent tasks. Instead of 
functions directly causing actions, tasks now detect events and signal them with semaphores, decoupling 
detection from response. Finally, a mutex was introduced to guard the Serial port, preventing the garbled 
output that would otherwise occur from multiple tasks trying to log messages simultaneously.

This transition is best illustrated by the loop() function's transformation. The "before" state shows 
it as the central engine, while the "after" state shows it being explicitly deleted, ceding all control 
to the FreeRTOS scheduler.

// Before: The loop() was the central engine.
void loop(void) {
  server.handleClient();
  delay(2);
}
// After: The loop() task is deleted to cede full control to FreeRTOS.
void loop(void) {
  vTaskDelete(NULL);
}

2. Framework Trade-off Review
The chosen path of Arduino + FreeRTOS offered two key advantages. The first is rapid prototyping, as the 
familiar Arduino API and libraries like WebServer greatly simplified hardware and network setup. The second 
is the vast library ecosystem, which makes it easy to integrate new components without writing low-level 
drivers. However, a significant limitation is the abstraction layer, which can obscure underlying system 
behavior and make debugging complex timing or memory issues more difficult than with the native framework.

If I had chosen the ESP-IDF path instead, the esp_http_server API would have helped by providing a web server 
designed natively for an asynchronous, multi-tasked environment. Conversely, the much higher complexity of 
setting up basic Wi-Fi and GPIO in ESP-IDF would have hurt the project by significantly increasing initial 
development time compared to the simple calls used in the Arduino framework.

3. Queue Depth & Memory Footprint
The sensorDataQueue was intentionally sized to hold only one item of type int. This was a deliberate choice 
to minimize memory footprint and ensure the web page always displays the absolute latest sensor reading, 
not a stale value from a buffer. To implement this, I used xQueueOverwrite() instead of xQueueSend(), 
which guarantees the queue simply updates its single slot with the most recent value, making an overflow 
condition impossible.

To demonstrate a near-overflow scenario, one could switch to xQueueSend with a small queue depth of 5 and
introduce a long vTaskDelay() in the webServerTask. The sensorMonitorTask, running every 17ms, would quickly 
fill the queue. This overflow would be detected when the xQueueSend function call returns the status 
errQUEUE_FULL. The mitigation would be to either increase the queue depth if historical data were needed or, 
as implemented, use xQueueOverwrite for a "latest value" display.

4. Debug & Trace Toolkit
The most valuable debug technique was a mutex-protected logging function that prepended a system tick timestamp 
(xTaskGetTickCount()) to every Serial message. This created a clean, chronologically-ordered trace of events from 
all concurrent tasks, which was essential for verifying task scheduling and identifying race conditions without 
the interleaved, garbled output that would otherwise occur.

The following log excerpt demonstrates correct task preemption, where a high-priority button press interrupts 
the normal flow. The heartbeat pauses, the button is handled, the mode change is executed, and then the heartbeat 
resumes, proving the scheduler is working correctly.
[15012] [Heartbeat] Green LED ON
[15331] Physical button pressed. Signaling mode change.
[15332] Mode changed to SHIELDED.
[16012] [Heartbeat] Green LED OFF

5. Domain Reflection
A key design decision directly related to the "Space Systems" theme was assigning the buttonWatchTask the highest 
priority. The stakes for a space probe are incredibly high; in a radiation emergency, a command from an astronaut 
or ground control to engage shielding must be executed immediately. This high priority ensures the command preempts
 all other non-critical operations, like routine sensor logging, because a fraction of a second could be the 
 difference between protecting mission-critical equipment and catastrophic failure.

The next real feature to add to an industrial version would be fault tolerance via sensor redundancy. This would 
involve adding a second radiation sensor and a cross-check task to compare readings. If the values diverged 
significantly, the system would flag a sensor fault, allowing it to operate on data from the remaining functional 
sensor and preventing a mission failure caused by a single point of hardware failure.

6. Web-Server Data Streaming: Benefits & Limitations
Pushing live sensor data to the web server offers two concrete advantages. The first is remote real-time diagnostics, 
which allows ground control to monitor the probe's environmental conditions from millions of miles away. The second 
is the offloading of data processing; the resource-constrained ESP32 can focus on its critical real-time duties while 
a more powerful ground station computer handles the logging, trend analysis, and visualization of the streamed data.

However, this introduces two limitations. The first is added latency; an experiment measuring the time from an analogRead() 
to when the web page is fully served would show a delay of hundreds of milliseconds, making this method unsuitable for 
tight control loops. The second trade-off is the increased CPU load and stack usage from the Wi-Fi and TCP/IP stack, 
which could potentially starve lower-priority tasks if not managed carefully. A check with vTaskGetInfo would confirm 
the higher resource consumption of the networking tasks.

To stream data reliably under heavy load, the design should be changed from the current HTTP-refresh mechanism to use 
WebSockets. A WebSocket establishes a single, persistent connection, which is far more efficient for streaming 
high-frequency updates than the high overhead cost of establishing a new HTTP request for every data point.