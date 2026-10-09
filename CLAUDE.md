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
- Files the runtime receives are exported by the backend (`POST /api/v0/projects/export/{id}`).

<!-- BEGIN SHARED: Neuron IDE system overview. Keep this block IDENTICAL in all three repos
     (Neuron-IDE-frontend, Neuron-IDE-backend, Neuron-IDE-runtime). Change it in one, copy to all. -->
## Neuron IDE — the whole system

Neuron IDE (KN-Neuron, university research group) is an IDE for building EEG/BCI experiments
(SSVEP, visual/text/audio stimuli, LSL markers). It is split across three repos, owned by three
teams. **This repo is one of them — your change may break the other two.**

| Repo | Team owns | Stack | Role |
|---|---|---|---|
| [Neuron-IDE-frontend](https://github.com/KN-Neuron/Neuron-IDE-frontend) | Frontend | React 19 + Vite, JS (no TS), @xyflow/react | Editor UI: scene canvas, experiment flow graph, inspector, EEG device picker |
| [Neuron-IDE-backend](https://github.com/KN-Neuron/Neuron-IDE-backend) | Backend | Python 3.13, FastAPI, SQLModel, Postgres 16, Alembic | Users/auth, project storage, `.neuroz` import/export |
| [Neuron-IDE-runtime](https://github.com/KN-Neuron/Neuron-IDE-runtime) | Runtime | C++17, CMake, SDL2, LSL, protobuf | Loads a `.neuroz` scene and runs the experiment (rendering, timing, LSL markers) |

### Data flow

```
frontend ──JSON over HTTP /api/v0/*──▶ backend ──.neuroz (protobuf Scene bytes)──▶ runtime
 (edits scene)                          (stores scene objects as protobuf blobs)     (parses & runs)
```

### The contract: `neuronide.proto` (single most important file)

- Defines `NeuronIDE.Scene` → `SceneObject` (name, is_visible, `Transform`, `Component`s).
  `Component` is a `oneof`: `renderer` (SpriteRenderer), `text` (TextRenderer),
  `blinker` (BlinkComponent), `script` (ScriptComponent).
- **Copies live in two repos and must stay byte-identical:**
  backend `app/domain/neuronide.proto` and runtime `protoFiles/neuronide.proto`.
- A `.neuroz` file = a serialized `NeuronIDE.Scene`. Backend exports it, runtime parses it.
- Changing the proto = cross-team change. Add fields with new numbers; never renumber or reuse
  field numbers. Update: backend proto + regenerate `neuronide_pb2.py` + `app/domain/` dataclasses,
  runtime proto + component class + `REGISTER_COMPONENT`, frontend inspector/scene editor.

### Backend HTTP API (base `/api/v0`, Swagger at `http://localhost:8000/docs`)

- `POST /auth/register`, `POST /auth/login`, `POST /auth/refresh`, `GET /auth/users`
- `GET /projects/`, `GET /projects/{id}`, `POST /projects/save`, `PUT /projects/{id}`,
  `POST /projects/export/{id}` (→ `.neuroz` bytes), `POST /projects/import` (multipart `.neuroz`)
- Project endpoints require `Authorization: Bearer <token>`. Auth is a **placeholder**:
  login `testuser`/`testpass` returns `fake_access_token`. Keycloak is planned (`app/auth/`).
- Project JSON = the proto shape in JSON; components are discriminated by `component_type`:
  ```json
  {"description": "…", "project": {"project_name": "Demo", "scene_objects": [
    {"name": "Square", "is_visible": true,
     "transform": {"x": 0, "y": 0, "width": 100, "height": 100, "rotation": 0},
     "components": [{"component_type": "blinker", "blink_frequency_hz": 15.0}]}]}}
  ```

### Known integration gaps (as of 2026-10-09 — fix them or update this list)

- Frontend calls `/api/projects`; backend serves `/api/v0/projects/`. No Vite dev proxy is
  configured, so the frontend silently falls back to hardcoded mock projects.
- Frontend expects `{ projects: [...] }`; backend `GET /projects/` returns a bare list.
- Frontend scene objects (`x, y, w, h, color, type: "rect"|"text", lslMarker`) don't match the
  proto `SceneObject` (`transform` + `components`). No `color`/`lslMarker` in the proto yet.
- The frontend experiment flow graph (blocks: trial, pause, stimuli, LSL markers, responses)
  has **no representation in the proto** — it is not saved by the backend nor run by the runtime.
- Runtime `Runtime::start()` is a stub; only the parser + BlinkComponent exist so far.

### Working across repos

Teams usually clone the repos side by side (`../Neuron-IDE-frontend`, `../Neuron-IDE-backend`,
`../Neuron-IDE-runtime`). **If a sibling repo is present, read its actual code instead of
trusting this summary.** If it isn't, ask the user or check GitHub before assuming how the other
side behaves. When a change touches the proto, the API shape, or the `.neuroz` format, tell the
user explicitly which other team(s) must be informed.
<!-- END SHARED -->
