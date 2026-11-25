# Reaction time

A black screen. At random moments a timer appears in the middle and counts up in milliseconds;
the participant stops it with the space bar as fast as they can. The stopped timer stays for a
second, so they see their time, and disappears. The scene comes from `experiment.pbtxt` and the
trial logic from the Python script `reaction_time.py`.

## Timeline

1. **Intro, 5 s.** Instructions.
2. **Each of the 10 trials:**
   - **Wait, 2–8 s,** drawn at random, with an empty screen.
   - **Timer** until space is pressed, at most 5 s. Unstopped, it disappears after 5 s.
   - **Feedback, 1 s.** The stopped timer shows the participant's time.
3. **End.** "Koniec. Dziękujemy!" stays on screen until you close the window.

A run takes 1–2 minutes, depending on the waits drawn and on how many timers run out.

## Markers

| Marker | Stamped with |
| --- | --- |
| `experiment_start` | the first frame |
| `timer_on trial=<n>` | the frame on which the timer appears |
| `response trial=<n>` | the moment space went down |
| `timeout trial=<n>` | the frame on which an unstopped timer disappears |
| `timer_off trial=<n>` | the frame on which the timer disappears |
| `false_start` | the moment space went down while no timer ran |
| `experiment_end` | the frame on which the end screen appears |

The reaction time of trial n is `response` minus `timer_on`. A key press is known to within one
frame (~17 ms at 60 Hz, README §7 "Input and reaction time"). The number on screen is counted
in whole frames, so it can be up to one frame short of the measured time. The log has it for each
trial (`[script] Trial 3: stopped at 312 ms`), together with the seed that drew the waits.

## Changing it

The number of trials and every duration are at the top of `reaction_time.py`. Set `FEEDBACK_S`
to 0 to hide the timer the moment it is stopped.
