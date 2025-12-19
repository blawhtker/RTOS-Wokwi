#Engineering Analysis Discussion Questions

1. Task Timing and Jitter: The sensors task message is displayed on the console 
every 500ms as intended. While the print task message and LED toggle display some 
jitter with occasional messages appearing at 1010ms instead of 1000ms. The difference
in stability is due to the 'vTaskDelayUntil' sensor task being based on and
absolute time reference. While the 'vTaskDelay' print and LED tasks are based on
a relative delay. The 'print_status_task' has to wait until the 'sensor_task' completes
and blocks again causing the jitter which over time will cause drift.

2. Priority-Based Preemption: Observed Scenario:
A clear example of preemption occurs when the print_status_task (priority 1) is 
about to run at the same time as the sensor_task (priority 2). A specific timeline 
illustrates this:

-Tick (n): The print_status_task is in the Running state. It has just finished its 
printf and is about to call vTaskDelay.
-Tick (n+1): The 500 ms timer for the sensor_task expires. It moves from the 
Blocked state to the Ready state. At this exact moment, the print_status_task 
is also Ready (it hasn't called vTaskDelay yet).
-Scheduler Decision (n+1): The FreeRTOS scheduler examines the list of 
Ready tasks. It sees print_status_task (priority 1) and sensor_task (priority 2).
Because the scheduler is priority-based and preemptive, it will always choose 
the highest-priority task that is ready to run.
-Action: The scheduler immediately saves the context (e.g., program counter) of 
print_status_task and switches the CPU to execute sensor_task. The sensor task 
does not wait. It runs instantly, interrupting the lower-priority task.
-Tick (n+15 approx): The sensor_task runs, reads the ADC, performs its calculations, 
and prints its alert/status.
-Tick (n+15): The sensor_task calls vTaskDelayUntil and enters the Blocked state.
-Scheduler Decision (n+15): The scheduler now sees that the highest-priority 
Ready task is the print_status_task (priority 1). It restores its context and allows 
it to finally execute its vTaskDelay(pdMS_TO_TICKS(1000)) call, putting it to sleep.
The sensor_task always interrupts immediately, demonstrating that priority is the 
sole factor in the scheduler's decision of which task to run next.

3. The sensor_task runs every 500 ms and executes for 300 ms. This means it is using 
60% of the CPU's time on Core 1 (CPU Utilization = 300ms / 500ms). The led_task and 
print_status_task are left to compete for the remaining 40% of the CPU time. They would 
experience significant delays and jitter, as they would frequently be preempted and have 
to wait for the long-running sensor task to finish.
If the sensor task execution time exceeded its period the system would enter an unstable 
state with symptoms like missing its own deadline and immediately run again, never 
giving the CPU to lower-priority tasks. The led_task and print_status_task would be 
completely starved of CPU time. The LED would freeze in its last state, and no telemetry
messages would ever be printed. The sensor alerts themselves would become unreliable 
because they would be based on old data. This is a classic symptom of CPU utilization 
exceeding 100% for a given core, leading to system failure.
Some options for a designer trying to alleviate these problems could optimize the complex
calculation to execute faster or lengthen the sensor tasks period if the application
can tolerate a lower sample rate. Another solution would just simply use both cores
instead of limiting to a single core like our application. This effectively splits the
workload in half and prevents the heavy calculations from interferring with the real-time
sampling schedule.

4. As stated above The difference in stability is due to the 'vTaskDelayUntil' sensor task 
being based on and absolute time reference. While the 'vTaskDelay' print and LED tasks are 
based on a relative delay. vTaskDelay blocks the task of a number of ticks to the moment 
it is called. vTaskDelayUntil unblocks the task at a specific fixed tick count, calculated
based on the lastWakeTime. vTaskDelayUntil solves this by cumulative timing drift.
vTaskDelay is acceptable for the LED task because it is a simple status indicator, the amount
of drift accumulated is imperceptible to a human and will have no functional impact on
the system.

5. Thematic Integration Reflection: The sensor_task is critical so it is given a high priority
(2). A scenario could be a satellite's orientation being off so the flight controller must act 
immediately to switch from solar power to internal batteries to prevent power loss.
print_status_task is given a medium priority (1) as the satellites immediate survival by 
managing power is more important that reporting its status. 
led_task is given a low priority (0) as this is a useful visual indicator to ground-based
telescopes but it is non-essential for the satellites survival or primary objectives.

** AI Used to generate tick timeline in (2) and for thematic integration reflection. **