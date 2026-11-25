"""A timer appears in the middle of a black screen at random moments and counts up in
milliseconds. The participant stops it with the space bar as fast as they can; it stays for a
moment, so they see their time, and disappears.

There are TRIALS timers. The wait before each one is drawn from MIN_WAIT_S..MAX_WAIT_S, so its
moment cannot be predicted (the runtime seeds `random` and logs the seed). A timer nobody stops
disappears after TIMEOUT_S.

Markers marked (key press) are stamped with the moment space went down; the others with the frame
on which the change appears:
    experiment_start
    timer_on trial=<n>       the timer appears
    response trial=<n>       space stopped it (key press)
    timeout trial=<n>        nobody stopped it within TIMEOUT_S
    timer_off trial=<n>      the timer disappears
    false_start              space while no timer runs (key press)
    experiment_end

The reaction time is `response` minus `timer_on`. The number on screen is counted in whole frames,
so it can be up to one frame short of that.
"""

import random

import neuronide

TRIALS = 10
INTRO_S = 5.0
MIN_WAIT_S = 2.0
MAX_WAIT_S = 8.0
TIMEOUT_S = 5.0
# How long a stopped timer stays on screen; 0 hides it at once.
FEEDBACK_S = 1.0


class ReactionTime(neuronide.Script):
    def on_start(self):
        self.timer = self.scene.find("Timer")
        self.trial = 0
        self.counting = False
        self.counted_s = 0.0
        self.stopped = False
        self.on_key("Space", self.on_space)

        self.timeline = self.run()
        self.wait_s = next(self.timeline)

    # Runs before on_update, in the frame after the press.
    def on_space(self, time):
        if not self.counting:
            self.emit_marker("false_start", time=time)
            return
        self.counting = False
        self.stopped = True
        self.emit_marker(f"response trial={self.trial}", time=time)

    def on_update(self, dt):
        if self.counting:
            self.counted_s += dt
            self.timer.text = f"{self.counted_s * 1000:.0f}"

        self.wait_s -= dt
        if self.stopped:  # a response ends the wait for it
            self.stopped = False
            self.wait_s = 0.0
        # Moves on when less than half a frame is left, so a wait lasts its length rounded to whole
        # frames, however much the frame times jitter.
        while self.wait_s < dt / 2:
            try:
                self.wait_s = next(self.timeline)
            except StopIteration:
                self.obj.active = False  # nothing left to time; the end screen stays up
                return

    # Yields how many seconds to wait before going on, counted from the frame the step ran in:
    # that frame is when its change appeared on screen.
    def run(self):
        self.emit_marker("experiment_start")
        yield INTRO_S
        self.scene.find("Instructions").active = False

        for trial in range(1, TRIALS + 1):
            self.trial = trial
            yield random.uniform(MIN_WAIT_S, MAX_WAIT_S)

            self.counted_s = 0.0
            self.timer.text = "0"
            self.timer.active = True
            self.counting = True
            self.emit_marker(f"timer_on trial={trial}")
            yield TIMEOUT_S

            if self.counting:
                self.counting = False
                self.emit_marker(f"timeout trial={trial}")
                print(f"Trial {trial}: no response")
            else:
                print(f"Trial {trial}: stopped at {self.counted_s * 1000:.0f} ms")
                yield FEEDBACK_S

            self.timer.active = False
            self.emit_marker(f"timer_off trial={trial}")

        self.scene.find("Thanks").active = True
        self.emit_marker("experiment_end")
