# Synchronization Quest - Application 4 

## Author 
**Name:** Blake Whitaker 
**Theme:** Space Systems
---
## Project Overview

This project explores task coordination and shared resource protection using FreeRTOS on the ESP32. The system simulates real-time sensor monitoring and alert handling using three key synchronization mechanisms:

- **Binary Semaphore** - signals from a user pushbutton
- **Counting Semaphore** - queues threshold events from an analog sensor
- **Mutex** - protects shared access to the serial console output

**Hardware Setup (Wokwi simulation):**

- 1x Potentiometer → GPIO34 (Analog Sensor)
- 1x Pushbutton → GPIO18
- 1x Red LED → GPIO4 (Alert)
- 1x Green LED → GPIO5 (Heartbeat)

---

## Engineering Analysis

Answer the following based on your project implementation and observations:

### 1. Signal Discipline (Binary Semaphore)
How does the binary semaphore synchronize the button press with the system? What did you observe when pressing the button quickly? Why is a binary semaphore appropriate here?

**Answer:**  
The binary semaphore is like a simple "go" signal. When you press the button (and our debounce filter approves it), the button task sends a single "go ahead" to the system's event handler. 

The debounce feature only lets one button press through every 200 milliseconds. So, rapid taps don't get counted as multiple commands.

If we used a counting semaphore without debouncing, every quick tap would queue up as a separate command. 
---

### 2. Event Flood Handling (Counting Semaphore)
What happens when the potentiometer crosses the threshold multiple times rapidly? How does the counting semaphore handle that? What would break if you used a binary semaphore instead? How did you tune the max count in the semaphore to capture a 30 second event flood (TODO 7)?

**Answer:**  

When you wiggle that potentiometer quickly, you'll see the red "Radiation Alert" LED flash and the console will be flooded with "Threshold exceeded!" messages. 
It's showing you every time the "radiation" level spikes.

The counting semaphore is like a flag counter for these radiation events. Each spike gets a flag and they all get queued up. This way, our system doesn't miss any critical alerts even if it's busy.

If we used a binary semaphore it could only hold one "flag" at a time. So, if a second radiation spike happened before the first one was processed we'd miss that second critical alert.
---

### 3. Protecting Shared Output (Mutex)
Which shared resource is protected by the mutex? What, if anything, happened when you removed it? What does this reveal about mutual exclusion in FreeRTOS?

**Answer:**  
If you take out the mutex the console output gets unreadable. Messages from different tasks will overlap and get mixed up.

Mutexes are like the watchdogs for shared resources like printing to the console. They make sure only one task prints at a time to keep things clear.

If shared system settings got messed up by concurrent changes the spacecraft could do something unexpected and dangerous.
---

### 4. Scheduling and Preemption
Describe how task priorities influenced scheduling (TODO 6). Provide an example where a high-priority task preempted a lower one. What happened to the heartbeat during busy periods?

**Answer:**  
Higher priority tasks always jump ahead of lower ones. If your green "System Status" light is blinking (low priority), and you hit the "Ground Control" button (high priority), the button task immediately takes over like an overide.

When the system gets really busy with important stuff the green heartbeat might seem to stutter or even pause. It's just waiting for the higher-priority tasks to finish their urgent work.
---

### 5. Timing and Responsiveness
The code provided uses `vTaskDelay` rather than `vTaskDelayUntil`. How did delays impact system responsiveness and behavior? Does the your polling rate affect event detection? Would you consider changing any of the `vTaskDelay` rather than `vTaskDelayUntil` - why or why not? Adjust your code accordingly.

**Answer:**  
vTaskDelay just tells the task to wait at least this long. So if other things happen your timing can drift. vTaskDelayUntil makes sure the task wakes up at exact regular intervals regardless of what else is going on.

How often your polling rate checks things definitely affects how quickly you catch events. Faster checking means you're less likely to miss something brief but it uses more power.

I'd switch the heartbeat and sensor tasks to vTaskDelayUntil. They need consistent and precise timing. The button and event handler tasks are fine with vTaskDelay because their exact timing isn't as critical as they rely on events to occur.
---

### 6. Theme Integration
Relate each component of your system to your chosen theme. For example, what does the sensor represent in a space probe? How does synchronization reflect safety requirements?

**Answer:**  
Green LED/Heartbeat Task: The spacecrafts uplink connection light.

Red LED/Potentiometer/Sensor Task/Counting Semaphore:The counting semaphore acts as a "radiation event log" so we don't miss any dangerous spikes in "radiation".

Button/Button Task/Binary Semaphore: The "ground control command" button. The binary semaphore ensures each command is processed only once.

Event Handler Task: The onboard mission control is reacting to alerts and commands.

Mutex: Our secure telemetry channel that keeps all vital messages clear.
---

### 7. [Bonus] Induced Failure - Starvation or Loss of Responsiveness
Did you design an experiment to break the system (e.g., starving the heartbeat task or missing button presses)? What did you observe? Include the modified (commented-out) code in your Wokwi project.

**Answer:**  
[Your response here]

---

## Presentation Slides

Link to your 4-slide summary here (google slides, onedrive powerpoint):

1. Introduction and Theme
2. Most Important Technical Lesson
3. Favorite Part of the Project
4. Something That Challenged You or You'd Explore More

(Bonus, Optional) If you included a voiceover, describe how to access it or link to a video.

---

## Summary
You’re not just coding — you’re building a real-time system. Each semaphore is a signal. Each mutex is a lock guarding safety. Each LED pulse is a message from your system’s heartbeat.
Can you keep your events ordered, your resources safe, and your system timely? This is your synchronization quest.
Good luck.

**Final Wokwi Project Link:** [Paste your Wokwi link here]

Download the project Zip.

Head over to webcourses
