"""Four targets flicker together, each at its own frequency; a red arrow says which one to look at.

After a short intro, every trial is a break followed by the stimulus. During the break the targets
stand still and the arrow is already over the next target, so the eyes move before the flicker
starts. Each target is cued REPETITIONS times, in random order (the runtime seeds `random` and
logs the seed).

Markers, each stamped with the frame on which the change appears:
    experiment_start
    cue trial=<n> target=<1-4> freq_hz=<f>       break starts, arrow over the target
    stim_on trial=<n> target=<1-4> freq_hz=<f>   flicker starts
    stim_off trial=<n> target=<1-4> freq_hz=<f>  flicker stops
    experiment_end
"""

import random

import neuronide

# blink_frequency_hz of Flicker1..Flicker4 in experiment.pbtxt, repeated here for the markers.
FREQUENCIES_HZ = (7.5, 60 / 7, 10.0, 12.0)
REPETITIONS = 1
INTRO_S = 5.0
BREAK_S = 2.0
STIMULUS_S = 6.0


class Target:
    def __init__(self, scene, number):
        self.number = number
        self.still = scene.find(f"Target{number}")
        self.flicker = scene.find(f"Flicker{number}")
        self.arrow = scene.find(f"Arrow{number}")

    def describe(self, trial):
        return f"trial={trial} target={self.number} freq_hz={FREQUENCIES_HZ[self.number - 1]:.4g}"


class SsvepTrials(neuronide.Script):
    def on_start(self):
        self.targets = [Target(self.scene, number) for number in range(1, len(FREQUENCIES_HZ) + 1)]
        order = self.targets * REPETITIONS
        random.shuffle(order)
        print("Target order:", " ".join(str(target.number) for target in order))

        self.timeline = self.run(order)
        self.remaining_s = next(self.timeline)

    def on_update(self, dt):
        self.remaining_s -= dt
        while self.remaining_s <= 0:
            try:
                self.remaining_s += next(self.timeline)
            except StopIteration:
                self.obj.active = False  # nothing left to time; the end screen stays up
                return

    # Yields how many seconds to wait before going on. The remainder of a wait carries over, so
    # frame-length rounding does not add up across trials.
    def run(self, order):
        self.emit_marker("experiment_start")
        yield INTRO_S
        self.scene.find("Instructions").active = False

        for trial, target in enumerate(order, start=1):
            self.point_at(target)
            self.emit_marker("cue " + target.describe(trial))
            yield BREAK_S

            self.set_flickering(True)
            self.emit_marker("stim_on " + target.describe(trial))
            yield STIMULUS_S

            self.set_flickering(False)
            self.emit_marker("stim_off " + target.describe(trial))

        self.point_at(None)
        for target in self.targets:
            target.still.active = False
        self.scene.find("Thanks").active = True
        self.emit_marker("experiment_end")

    def point_at(self, cued):
        for target in self.targets:
            target.arrow.active = target is cued

    def set_flickering(self, flickering):
        for target in self.targets:
            target.still.active = not flickering
            target.flicker.active = flickering
