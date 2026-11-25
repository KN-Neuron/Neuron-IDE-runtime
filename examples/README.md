# Examples

Experiments you can run with the runtime. Each one is a folder with its scene
(`experiment.pbtxt`), its trial logic in Python, its images, and a README.

| Example | What it is |
| --- | --- |
| [`reaction-time`](reaction-time/README.md) | A timer appears at random moments; space stops it |
| [`p300`](p300/README.md) | A visual oddball: count the rare red circles among blue ones |

## Run one

Build the runtime first (`cmake -B build && cmake --build build`), then from
`Neuron-IDE-runtime/`:

```bash
examples/run.sh p300                                    # with a fake EEG stream
EEG=device examples/run.sh p300                         # with the BA MIDI 072 headset
examples/run.sh reaction-time path/to/other-config.json # with another device config
```

`run.sh` encodes the scene into `experiment.neuroz`, copies it with the script, the images and
the device config into `build/example_<name>/`, and runs the runtime there. Everything from a run,
including the CSV recording, ends up in that folder. Close the window to stop: no example ends the
run by itself yet, so each leaves its end screen up.

**No stimulus is drawn without EEG.** The runtime waits for the stream named in the config and
gives up after 30 s.

- **Fake EEG** (the default): `run.sh` also starts `build/tools/fake_eeg`, which streams made-up
  EEG under the name, channel count, rate and channel labels of the config, so the runtime takes
  it for the device. Don't use it while the real device streams: both streams would have the same
  name, and the runtime records from whichever it finds first.
- **The real device** (`EEG=device`): the stream must already be on LSL when you start.

## Device configs

The second argument of `run.sh` is the device `config.json` (README §5); it defaults to
[`devices/ba-midi-072.json`](devices/ba-midi-072.json).

`ba-midi-072.json` is for the BrainAccess MIDI headset `BA MIDI 072` as the KNeuron Interface app
streams it to LSL: stream `KNeuron BA MIDI 072`, 16 EEG channels at 250 Hz, in microvolts, every
channel recorded. The reference electrode sits on Fp1 and is not streamed. P8 (channel 0) also
drives the bias, but it is still recorded. To get the stream, connect the headset in the
Interface app and open the neurostate app there. BrainAccess Board streams the same headset under another
name and with more channels (contact quality, accelerometer, battery), so it needs a config of
its own.

The scenes are laid out for the 1280x720 window the config asks for. In fullscreen they stay at
the top-left of the screen.
