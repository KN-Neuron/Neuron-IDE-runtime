# P300

A visual oddball. Circles flash one at a time, each for 150 ms at a new random place: most are
blue (standards), one in five is red (targets). The participant keeps their eyes on the cross in
the middle and counts the red circles in their head. A rare target that the participant attends
to evokes the P300, a positive wave about 300 ms after its onset, largest over Pz. The scene
comes from `experiment.pbtxt` and the trial logic from the Python script `p300.py`.

## Timeline

1. **Intro, 6 s.** Instructions.
2. **120 circles, 24 of them red,** in random order, never two red ones in a row. Between
   circles only the cross is on screen, for a random 0.8–1.2 s, so the next onset can't be
   predicted. A circle never covers the cross.
3. **End.** "Koniec. Dziękujemy! Ile było czerwonych kółek?" stays on screen until you close the
   window. Ask for the count: it shows whether the participant attended. The right answer is
   in the log (`[script] 24 targets (T) among 120 circles: …`), together with the order and the
   seed that produced it.

A run takes about 2.5 minutes.

## Markers

Each marker is stamped with the time of the frame on which its change appears:

| Marker | When |
| --- | --- |
| `experiment_start` | First frame |
| `stim_on n=<i> type=<target\|standard> x=<px> y=<px>` | Circle i appears, centred on x, y (window pixels) |
| `stim_off n=<i>` | Circle i disappears |
| `experiment_end` | End screen appears |

For the P300, cut the EEG around each `stim_on`, average the target epochs and the standard
epochs, and compare them at Pz, Cz and Fz. `x` and `y` let you check whether the position of a
circle matters.

## Files

| File | What it is |
| --- | --- |
| `experiment.pbtxt` | The scene: instructions, cross, the two circles, end screen |
| `p300.py` | The trial logic, with its counts and timings at the top |
| `target.png`, `standard.png` | The red and the blue circle |

## Changing it

- **Counts and timings** are at the top of `p300.py`. `TARGETS` may be at most one more than
  the number of standards, or the targets can't be kept apart.
- **Where circles appear** is `AREA` in `p300.py`, in pixels of the 1280x720 window, and
  `CLEAR_OF_CROSS_PX` keeps them off the cross.
- **Flash length** is rounded to whole frames. Choose one that is a whole number of frames at
  the monitor's refresh rate: 0.15 s is 9 frames at 60 Hz and 18 at 120 Hz.
