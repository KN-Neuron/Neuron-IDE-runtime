# Image and text

One image and four text objects in a 1280x720 window. Nothing moves: the scene stays on screen
until you close the window.

| File | What it is |
| --- | --- |
| `experiment.pbtxt` | The scene, in readable form |
| `stimulus.png` | The image the `Checkerboard` object shows |
| `config.json` | Device config matching the fake EEG stream from `../lsl_testing/outlet` |
| `run.sh` | Encodes the scene, starts the fake stream and runs the runtime |

## Run it

Build the runtime first, then from `Neuron-IDE-runtime/`:

```bash
examples/image_and_text/run.sh
```

The window appears straight away, and nothing is drawn in it until the EEG stream connects,
usually within a few seconds. **No stimulus is drawn without EEG**: with no stream the runtime
stops after 30 s.

Everything from a run, including the CSV recording, goes to `build/example_image_and_text/`.

To change the scene, edit `experiment.pbtxt` and run the script again. Text with no `font_path`
uses the built-in font, and `font_size` is required.
