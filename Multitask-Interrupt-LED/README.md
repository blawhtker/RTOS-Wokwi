Blake Whitaker

Analysis/Engineering

Does anything happen to the LED if you increase the delay within the print task? 
-No the LED still blinks at 250ms but the print message will simply output 
less depending on your delay.

What if you increase the number of characters printed?
-No any reasonable amount of text should not delay the LED.

Explain why this system benefits from having correct functionality at predictable times.
-Determinism in real-time systems ensures reliability and safety.

LED blink task period: 250ms
Print task period: 10000ms

Did our system tasks meet the timing requirements?
-Yes

How do you know?
-I visually cross checked the simulators timer with the LED blinks and the status text.

How did you verify it?
-I ran the simulation multiple times to check for any errors.

Did you try running the code?
-Yes

Can you cause the LED to miss it's timing requirements?
-Yes

If yes, how?
-You could increasing cpu work load with a task of the same priority 
or introduce a higher priority task.

If no, what did you try?
