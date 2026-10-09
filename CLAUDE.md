# CLAUDE.md — Neuron-IDE-runtime

You are in the **runtime** repo. Read the system overview below first.

## This repo

- Build: `cmake -B build && cmake --build build`. Tests: `cd build && ctest --output-on-failure`
  (`-L unit` / `-L component`). Format: `cmake --build build --target format` — CI fails on
  unformatted code.
- Layout: `include/` headers, `src/` impl, `src/parser/Parser.cpp` turns a `.neuroz`/`.pb` file
  into a `Scene`, `scene/components/` component classes, `tests/{unit,component}_tests/`.
- New component: subclass `Component`, add a static creator, and
  `REGISTER_COMPONENT(NeuronIDE::Component::k<Name>, Creator)` in its `.cpp`. The proto oneof
  entry must exist first — which means a backend change too.
- Test scenes: write `.pbtxt`, then
  `protoc --encode=NeuronIDE.Scene protoFiles/neuronide.proto < x.pbtxt > x.pb`.
- Read README §3 (architecture: Renderer / LSLReader / DataWriter threads) and §5 (what goes in
  the `.neuroz` proto vs the device `config.json`) before adding settings.
- Inputs: the `.neuroz` comes from the backend export (`POST /api/v0/projects/export/{id}`); the
  device `config.json` is written by the launcher, which starts the runtime once it accepts
  command-line arguments (runtime#37). Exit codes are part of that contract: the launcher shows
  them to the experimenter.
- `config.json` is also produced by the launcher (Python). Changing its keys or validation rules
  is a cross-team change: bump `config_version` and tell the launcher team.

<!-- BEGIN SHARED: Neuron IDE system overview. Keep this block IDENTICAL in all four repos
     (Neuron-IDE-frontend, -backend, -runtime, -launcher). Change it in one, copy to all. -->
## Neuron IDE — the whole system

Neuron IDE (KN-Neuron, university research group) is an IDE for building EEG/BCI experiments
(SSVEP, visual/text/audio stimuli, LSL markers). It is split across four repos, owned by
different teams. **This repo is one of them — your change may break the others.**

| Repo | Team owns | Stack | Role |
|---|---|---|---|
| [Neuron-IDE-frontend](https://github.com/KN-Neuron/Neuron-IDE-frontend) | Frontend | React 19 + Vite, JS (no TS), @xyflow/react | Editor UI: scene canvas, experiment flow graph, inspector |
| [Neuron-IDE-backend](https://github.com/KN-Neuron/Neuron-IDE-backend) | Backend | Python 3.13, FastAPI, SQLModel, Postgres 16, Alembic | Users/auth, project storage, `.neuroz` import/export |
| [Neuron-IDE-launcher](https://github.com/KN-Neuron/Neuron-IDE-launcher) | TBD | Python 3.13, PySide6 (Qt), pyqtgraph, pylsl, numpy | Lab-PC app: experiments/subjects/sessions/runs, device + electrodes, signal check, starts runtime |
| [Neuron-IDE-runtime](https://github.com/KN-Neuron/Neuron-IDE-runtime) | Runtime | C++17, CMake, SDL2, LSL, protobuf | Runs the experiment: rendering, timing, LSL markers, records EEG |

### Data flow

```
frontend ──JSON /api/v0/*──▶ backend ──.neuroz (+ form.json)──▶ launcher ──.neuroz + config.json──▶ runtime
 (designs scene,              (stores                           (subjects, device,                 (runs, records
  flow & subject form)         projects)                         electrodes, signal check)          into run folder)
```

### The two runtime inputs (runtime README §5)

- **`.neuroz`** = serialized `NeuronIDE.Scene` (protobuf): **what the experiment does**.
  Authored in the editor, stored and exported by the backend.
- **`config.json`** = device config (JSON, `config_version` 1.x): **what the hardware is**:
  LSL stream identity, channel table (index, label, enabled, unit), reference, ground.
  Produced by the launcher's device + electrode steps.
- Rule for new fields: if it changes the experiment's meaning for analysis → proto; if it only
  changes how this machine acquires or stores data → `config.json`.
- The **subject form** is neither: it's a separate `form.json` designed in the editor and used
  only by the launcher (epic frontend#63). It is not in the proto and the runtime never sees it.

### The proto contract: `neuronide.proto`

- `Scene` → `SceneObject` (name, is_visible, `Transform`, `Component`s). `Component` is a
  `oneof`: `renderer`, `text`, `blinker`, `script`, `marker_emitter` (runtime only so far).
- **Copies live in several repos and must stay byte-identical:** backend
  `app/domain/neuronide.proto`, runtime `protoFiles/neuronide.proto`, and the launcher.
- Changing the proto = cross-team change. Add fields with new numbers; never renumber or reuse
  field numbers. Update: backend proto + regenerate `neuronide_pb2.py` + `app/domain/` dataclasses,
  runtime proto + component class + `REGISTER_COMPONENT`, launcher copy, frontend editor.

### Launcher (lab PC, before the runtime)

Steps: experiment → subject → session → device → electrodes → signal check → run (and "run
again"). Data lives in `<data root>/<experiment>/subjects/S001/sessions/001/runs/01/`.
Subjects' form answers are **personal data** and live only in the experiment's `subjects.csv`,
never in folder names, recordings or logs. For each run it writes `config.json` and starts the
runtime (CLI: runtime#37), which records EEG + markers into the run folder. Tasks: epic
launcher#1.

### Backend HTTP API (base `/api/v0`, Swagger at `http://localhost:8000/docs`)

- `POST /auth/register`, `POST /auth/login`, `POST /auth/refresh`, `GET /auth/users`
- `GET /projects/`, `GET /projects/{id}`, `POST /projects/save`, `PUT /projects/{id}`,
  `POST /projects/export/{id}` (→ `.neuroz` bytes), `POST /projects/import` (multipart `.neuroz`)
- Project endpoints require `Authorization: Bearer <token>`. Auth is a **placeholder**:
  login `testuser`/`testpass` returns `fake_access_token`.
- Frontend calls these via `src/utils/api.js` (Vite proxies `/api` → `localhost:8000` in dev).
- Project JSON = the proto shape in JSON; components are discriminated by `component_type`:
  ```json
  {"description": "…", "project": {"project_name": "Demo", "scene_objects": [
    {"name": "Square", "is_visible": true,
     "transform": {"x": 0, "y": 0, "width": 100, "height": 100, "rotation": 0},
     "components": [{"component_type": "blinker", "blink_frequency_hz": 15.0}]}]}}
  ```

### Known cross-repo gaps (as of 2026-10-09 — fix them or update this list)

- Proto copies have diverged: backend lacks `MarkerEmitterComponent` (backend#23).
- Experiment flow graph is not in the proto, so it isn't saved, exported or run (epic frontend#59).
- Scene object color and LSL marker are not persisted (frontend#56, backend#24).
- Media files (images, audio, scripts) can't be uploaded, so the runtime can't find them (backend#25).
- Auth is a placeholder (frontend#57, backend#14).
- Subject form has no format yet (frontend#64).
- Runtime ignores command-line arguments, so the launcher can't start it yet (runtime#37);
  `Runtime::start()` is a stub until the main loop lands (runtime#15).

### Working across repos

Teams usually clone the repos side by side (`../Neuron-IDE-frontend`, `../Neuron-IDE-backend`,
`../Neuron-IDE-launcher`, `../Neuron-IDE-runtime`). **If a sibling repo is present, read its
actual code instead of trusting this summary.** If it isn't, ask the user or check GitHub before
assuming how the other side behaves. When a change touches the proto, `config.json`, the subject
form JSON, the API shape or the `.neuroz` format, tell the user which other team(s) must be
informed.
<!-- END SHARED -->
