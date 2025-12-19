#Engineering Analysis Discussion Questions

1. Using an ISR and a semaphore is more efficient and responsive than polling. 
The logger task consumes zero CPU while it waits, sleeping until the hardware 
interrupt instantly signals it to run.

Polling requires a task to constantly run in a loop, creating two problems.
The loop wastes CPU cycles just to check the button. Adding a delay to save CPU 
makes the system less responsive and might miss button presses.

2. We must use ...FromISR functions because Interrupt Service Routines (ISRs) 
run in a special context where they cannot block or wait. A regular function 
like xSemaphoreTake is unsafe because it might try to put the caller to sleep, 
which is an illegal operation for an ISR that would crash the system.

The ...FromISR functions are safe, non-blocking versions that simply queue an 
action (like giving a semaphore) to be handled by the scheduler after the 
interrupt completes, ensuring system stability.

3. When the button is pressed, the hardware immediately pauses the running 
`SolarPanelMonitorTask` (Priority 2) and executes the ISR. Inside the ISR, 
`xSemaphoreGiveFromISR` makes the `GroundCommandTask` (Priority 3) ready to run. 
Because a higher-priority task has been woken, the ISR signals the scheduler to 
perform a context switch upon exit.

As soon as the ISR completes, the preemptive scheduler immediately runs the 
highest-priority ready task: the `GroundCommandTask`. The lower-priority sensor 
task is forced to wait.

4. Without pinning tasks, unpredictable behavior can occur. An ISR on one core 
could ready a task that runs in parallel with another task on the second core, 
leading to complex timing issues and potential race conditions that are hard to 
debug.

The benefit of pinning all tasks to a single core for this lab is determinism.
It forces a predictable, single-threaded execution model where only one task runs 
at a time. This makes the concept of preemption easy to observe and understand,
without the added complexity of multi-core parallelism.

5. A mutex was used to prevent data corruption when sharing the log buffer between 
tasks. Without it, the higher-priority logger task could preempt the sensor task 
mid-write, causing incorrect data. A robust approach is to use  the mutex to quickly 
copy the data to a local buffer and then release the mutex before processing, 
minimizing blocking time.

6. If the LoggerTask had a lower priority (1) than the BlinkTask (3), the log would 
not dump immediately. When the button is pressed, the LoggerTask becomes ready, but 
the preemptive scheduler would see that the BlinkTask has a higher priority and run 
it instead. The LoggerTask would only execute after all higher-priority tasks have 
finished their work and entered a blocked state. This delay defeats the purpose of 
an event-driven design.

7. Keeping Interrupt Service Routines (ISRs) short is essential for a stable real
time system. Long-running ISRs cause two major problems. A long ISR can delay more 
critical hardware interrupts from running, making the system unresponsive to urgent 
events. A long ISR can also delay the OS's own timing tick, making task delays and 
timeouts unpredictable and breaking real-time guarantees. Deferring heavy work to a 
task keeps ISRs fast, solving both of these issues.

8. I applied the concept of using a semaphore for ISR-to-task synchronization to 
avoid inefficient polling, as described in the course readings. My implementation 
is a direct example of this pattern: The GroundCommandTask blocks on xSemaphoreTake, 
using no CPU while waiting. The button_isr_handler uses xSemaphoreGiveFromISR to 
signal the task.