"""A visual oddball for the P300. Circles flash one at a time, each for STIMULUS_S at a new random
place: most are blue (standards), a few are red (targets). The participant looks at the cross in
the middle and counts the red circles in their head. A rare target that the participant attends
to evokes the P300, a positive wave about 300 ms after its onset, largest over Pz.

STIMULI circles, TARGETS of them red, in random order with never two targets in a row (the runtime
seeds `random` and logs the seed). Between circles only the cross is on screen, for a random
MIN_GAP_S..MAX_GAP_S, so the participant cannot predict when the next one comes.

Markers, each stamped with the frame on which the change appears:
    experiment_start
    stim_on n=<i> type=<target|standard> x=<px> y=<px>   circle i appears, centred on x, y
    stim_off n=<i>                                       it disappears
    experiment_end

For the P300, average the EEG after `stim_on ... type=target` and compare it with the average
after `type=standard`.
"""

import math
import random

import neuronide

STIMULI = 120
TARGETS = 24
INTRO_S = 6.0
STIMULUS_S = 0.15
MIN_GAP_S = 0.8
MAX_GAP_S = 1.2
# Circles stay inside this part of the 1280x720 window: left, top, right, bottom, in pixels.
AREA = (60, 60, 1220, 660)
# How close a circle's centre may come to the cross's, in pixels, so it never covers it.
CLEAR_OF_CROSS_PX = 160


def make_sequence():
    """TARGETS times True and STIMULI - TARGETS times False, shuffled, no two True in a row."""
    standards = STIMULI - TARGETS
    if not 0 <= TARGETS <= standards + 1:
        raise ValueError(f"{TARGETS} targets among {STIMULI} circles cannot be kept apart")
    # Each target takes its own gap: before a standard, or after the last one.
    gaps = set(random.sample(range(standards + 1), TARGETS))
    sequence = []
    for gap in range(standards + 1):
        if gap in gaps:
            sequence.append(True)
        if gap < standards:
            sequence.append(False)
    return sequence


class Oddball(neuronide.Script):
    def on_start(self):
        self.cross = self.scene.find("Cross")
        self.target = self.scene.find("Target")
        self.standard = self.scene.find("Standard")

        sequence = make_sequence()
        order = "".join("T" if is_target else "s" for is_target in sequence)
        print(f"{TARGETS} targets (T) among {STIMULI} circles: {order}")

        self.timeline = self.run(sequence)
        self.wait_s = next(self.timeline)

    def on_update(self, dt):
        self.wait_s -= dt
        # Moves on when less than half a frame is left, so a wait lasts its length rounded to whole
        # frames, however much the frame times jitter: at 60 Hz a 0.15 s flash is always 9 frames.
        while self.wait_s < dt / 2:
            try:
                self.wait_s = next(self.timeline)
            except StopIteration:
                self.obj.active = False  # nothing left to time; the end screen stays up
                return

    # Yields how many seconds to wait before going on, counted from the frame the step ran in:
    # that frame is when its change appeared on screen.
    def run(self, sequence):
        self.emit_marker("experiment_start")
        yield INTRO_S
        self.scene.find("Instructions").active = False
        self.cross.active = True

        for number, is_target in enumerate(sequence, start=1):
            yield random.uniform(MIN_GAP_S, MAX_GAP_S)

            circle = self.target if is_target else self.standard
            x, y = self.place(circle)
            circle.active = True
            kind = "target" if is_target else "standard"
            self.emit_marker(f"stim_on n={number} type={kind} x={x} y={y}")
            yield STIMULUS_S

            circle.active = False
            self.emit_marker(f"stim_off n={number}")

        yield MAX_GAP_S
        self.cross.active = False
        self.scene.find("Thanks").active = True
        self.emit_marker("experiment_end")

    def place(self, circle):
        """Moves the circle to a random place in AREA, clear of the cross; returns its centre."""
        left, top, right, bottom = AREA
        half_width, half_height = circle.width / 2, circle.height / 2
        cross_x = self.cross.x + self.cross.width / 2
        cross_y = self.cross.y + self.cross.height / 2
        while True:
            x = random.randint(round(left + half_width), round(right - half_width))
            y = random.randint(round(top + half_height), round(bottom - half_height))
            if math.hypot(x - cross_x, y - cross_y) >= CLEAR_OF_CROSS_PX:
                break
        circle.x = x - half_width
        circle.y = y - half_height
        return x, y
