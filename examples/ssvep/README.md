# SSVEP

Four white squares in a row, each flickering at its own frequency. A red arrow above one of them
says which one to look at. A proof of concept: the scene comes from `experiment.pbtxt` and the
trial logic from the Python script `ssvep.py`.

| Target | 1 | 2 | 3 | 4 |
| --- | --- | --- | --- | --- |
| Frequency | 7.5 Hz | 8.57 Hz (60/7) | 10 Hz | 12 Hz |

## Timeline

1. **Intro, 5 s.** Instructions; the squares stand still.
2. **Each trial, in random order** (each target once; `REPETITIONS` in `ssvep.py` for more):
   - **Break, 2 s.** The squares stand still and the arrow is already over the next target, so
     the participant moves their eyes before the flicker starts.
   - **Stimulus, 6 s.** All four squares flicker, and the participant keeps looking at the
     arrowed one.
3. **End.** "Koniec. Dziękujemy!" stays on screen until you close the window.

Four trials take 32 s, plus the intro.

## Markers

Each marker is stamped with the time of the frame on which its change appears:

| Marker | When |
| --- | --- |
| `experiment_start` | First frame |
| `cue trial=<n> target=<1-4> freq_hz=<f>` | Break starts, arrow moves |
| `stim_on trial=<n> target=<1-4> freq_hz=<f>` | Flicker starts |
| `stim_off trial=<n> target=<1-4> freq_hz=<f>` | Flicker stops |
| `experiment_end` | End screen appears |

To analyse a trial, take the EEG between its `stim_on` and `stim_off`. The order of the targets
is in the log (`[script] Target order: …`), together with the seed that produced it.

## Files

| File | What it is |
| --- | --- |
| `experiment.pbtxt` | The scene: targets, flickering copies, arrows and texts |
| `ssvep.py` | The trial logic, with its timings and repetitions at the top |
| `target.png`, `arrow.png` | The square and the arrow |
| `config.json` | Device config for an 8-channel, 250 Hz stream, with occipital labels |
| `run.sh` | Encodes the scene, starts the fake EEG stream and runs the runtime |

## Run it

Build the runtime first, then from `Neuron-IDE-runtime/`:

```bash
examples/ssvep/run.sh
```

The script starts the fake EEG stream `../lsl_testing/outlet`, which matches `config.json`. To
use another stream, set `EEG_OUTLET` to the program that starts it, or to `none` if it is already
running. **No stimulus is drawn without EEG**: with no stream the runtime stops after 30 s.

Everything from a run, including the CSV recording, goes to `build/example_ssvep/`.

## Changing it

- **Timings and repetitions** are at the top of `ssvep.py`.
- **Frequencies** are the `blink_frequency_hz` of `Flicker1`–`Flicker4` in `experiment.pbtxt`.
  `ssvep.py` repeats them in `FREQUENCIES_HZ` for the markers, so change both. Choose frequencies
  whose period is a whole number of frames at the monitor's refresh rate: at 60 Hz that means
  60/n Hz (7.5, 8.57, 10, 12, 15 Hz). Any other frequency flickers unevenly.
- **Layout** is in window pixels for the 1280x720 window `config.json` asks for. In fullscreen
  the scene stays at the top-left of the screen.

Each target is two objects at the same place: `TargetN` stands still and `FlickerN` blinks, and
the script shows one or the other. That is how the targets stand still during breaks: a script
cannot pause a `BlinkComponent` yet.
