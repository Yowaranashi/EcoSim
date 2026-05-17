# EcoSim

EcoSim is a C++17/CMake ecosystem simulator. The main workflow is headless scenario execution with CSV output. Console mode is available for interactive commands. OGRE viewer is an optional CSV visualization layer and is not part of the simulation logic.

## Build

Standard Release build:

```powershell
cmake -S . -B build_codex
cmake --build build_codex --config Release
ctest --test-dir build_codex -C Release --output-on-failure
```

Run from the portable build directory:

```powershell
cd build_codex\Release
.\ecosim.exe configs\app.toml
```

Build with OGRE viewer through vcpkg installed in `C:\vcpkg`:

```powershell
cmake -S . -B build_ogre `
  -DECOSIM_BUILD_OGRE_VIEWER=ON `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows

cmake --build build_ogre --config Release
ctest --test-dir build_ogre -C Release --output-on-failure
```

Create an install-style portable directory:

```powershell
cmake --install build_ogre --config Release --prefix build_ogre\portable
```

The build and install output contain the runtime payload next to the executable:

- `configs/`
- `scenarios/`
- `modules/`
- `docs/`
- `README.md`
- `LICENSE`
- MSVC runtime DLLs
- for OGRE builds: `ecosim_ogre_viewer.exe`, OGRE/vcpkg DLLs, `plugins.cfg`, `plugins/ogre/*.dll`

OGRE SDK is needed to build the viewer. A different computer does not need the SDK to run the viewer if the portable folder contains the bundled DLLs and plugins.

## Runtime Paths

Relative runtime paths are resolved from the runtime root next to the executable. The runtime root is the directory containing `modules/simulation_world/manifest.toml`.

Example:

```toml
modules_dir = "modules"
scenario_path = "scenario-gfl.toml"
output_dir = "output"
```

This means:

- modules are loaded from `modules/`
- scenario is loaded from `scenarios/scenario-gfl.toml`
- recorder output is written under `output/`

The location of the app TOML file does not become the base directory for modules, scenarios, or output. Absolute paths are still supported for compatibility.

## App Config

Default config: `configs/app.toml`.

```toml
mode = "headless"
error_policy = "fail-fast"
modules_dir = "modules"
scenario_path = "scenario-gfl.toml"
output_dir = "output"
dt = 1.0
max_ticks = 150
log_tick_interval = 25
log_tick_details = false
ogre_visualization = false

instances = [
  { type = "simulation_world", id = "default", enable = true },
  { type = "scenario", id = "default", enable = true },
  { type = "recorder", id = "csv", enable = true, params = { sink = "csv" } }
]
```

Fields:

- `mode`: `headless` or `console`
- `error_policy`: `fail-fast` or `auto-disable`
- `modules_dir`: module manifest directory
- `scenario_path`: file name or relative path inside `scenarios/`
- `output_dir`: CSV and sensitivity output directory
- `dt`: fallback timestep
- `max_ticks`: safety limit for the tick loop
- `log_tick_interval`: console progress interval; `0` disables progress tick logs
- `log_tick_details`: when `true`, progress logs include per-species state values
- `recorder_output_path`: optional explicit CSV path for the normal recorder
- `ogre_visualization`: when `true` in an OGRE build, launch viewer after a simulation run
- `instances`: runtime modules to start

Recommended runtime modules for simulations:

- `simulation_world`
- `scenario`
- `recorder`

## Run Modes

Headless:

```powershell
.\ecosim.exe configs\app.toml
```

Console:

```toml
mode = "console"
```

Then run:

```powershell
.\ecosim.exe configs\app.toml
```

Unknown `mode` logs a warning and falls back to headless.

## Console Commands

Core:

```text
help
module.list
module.start
module.stop
sys.quit
```

Simulation:

```text
scenario.list
sim.run
sim.run -s scenario-gfl.toml
sim.run --scenario scenario-rm.toml
sim.run --output run_glv
sim.run -s scenario-rm.toml --output rm_run.csv
sim.pause
sim.resume
```

`sim.run` without `-s` uses `scenario_path` from `app.toml`. `sim.run -s` loads the selected file from `scenarios/`. Relative console scenario paths cannot escape `scenarios/` through `../`.

`sim.run --output <name>` changes the normal recorder CSV for that run. If `<name>` has no extension, `.csv` is added. A bare file name is written under `output_dir`; a relative path with directories is resolved from runtime root.

Sensitivity analysis:

```text
sim.sensitivity -s scenario-rm.toml -p h --from 0.01 --to 1.0 --samples 100 --output sensitivity_h
```

The CSV is written to:

```text
output/sensitivity_h.csv
```

OGRE runtime flag:

```text
ogre.status
ogre.enable
ogre.disable
```

`ogre.enable` makes the app launch `ecosim_ogre_viewer` after the next completed `sim.run` or headless run. The viewer receives the current recorder CSV path. If `sim.run --output <file>` was used, that file is visualized; otherwise the default `output/simulation.csv` is used. `ogre.disable` turns this off.

## Console Output

EcoSim does not print one line for every tick by default. It prints start, periodic progress, and finish messages.

Example:

```text
[simulation] Simulation started: scenario=scenario_gfl model=glv integrator=euler dt=0.100 stop_at_tick=100 log_tick_interval=25 log_tick_details=false
[simulation] Progress: tick=25/100 time=2.500 biomass=122.430 min_population=4.120 checksum=...
[system] Simulation finished: tick=100 checksum=... output=output/simulation.csv
```

Control it from app config or scenario:

```toml
log_tick_interval = 50
log_tick_details = false
```

Rules:

- `log_tick_interval = 0`: no periodic tick progress
- `log_tick_interval = 25`: log ticks 25, 50, 75, etc.
- final stop tick is always reported by the app
- `log_tick_details = true`: include `state.<species>=...` values in progress lines

## Scenarios

User scenarios live in:

```text
scenarios/
```

Example app config:

```toml
scenario_path = "scenario-gfl.toml"
```

opens:

```text
scenarios/scenario-gfl.toml
```

Common scenario fields:

```toml
scenario_id = "scenario_gfl"
model = "glv"
seed = 42
dt = 0.1
stop_at_tick = 100
integrator = "euler"
log_tick_interval = 25
log_tick_details = false

requires = ["simulation_world", "recorder"]
schedule = []
```

Fields:

- `scenario_id`: stable id used in CSV and logs
- `model`: model id
- `seed`: deterministic random seed
- `dt`: scenario timestep
- `stop_at_tick`: simulation stop tick
- `integrator`: `euler` or `rk4`
- `log_tick_interval`: scenario override for console progress interval
- `log_tick_details`: scenario override for per-species progress details
- `requires`: modules that must be enabled
- `schedule`: commands applied at specific ticks

## Schedule Commands

`spawn`: add population to a species.

```toml
schedule = [
  { tick = 10, command = "spawn", species = "rabbit", count = 5.0 }
]
```

For mathematical models, `spawn` applies a positive shock to the target state.

`set_param`: change a model parameter.

```toml
schedule = [
  { tick = 50, command = "set_param", name = "growth.rabbit", value = 0.1 }
]
```

`apply_shock`: directly change a species state.

```toml
schedule = [
  { tick = 20, command = "apply_shock", target = "rabbit", strength = -10.0 }
]
```

`stop.at_tick`: change the stop tick from a scenario command.

```toml
schedule = [
  { tick = 100, command = "stop.at_tick", value = 100 }
]
```

## Models

Supported ids:

- generalized Lotka-Volterra: `glv`, `generalized_lotka_volterra`
- Rosenzweig-MacArthur: `rm`, `rosenzweig_macarthur`

## Generalized Lotka-Volterra

Formula:

```text
dX_i/dt = X_i * (r_i + sum_j(a_ij * X_j) + s_i * u_i)
```

Scenario example:

```toml
model = "glv"
species = ["grass", "rabbit", "fox"]
initial_state = [100.0, 30.0, 5.0]
growth = [0.4, 0.2, -0.1]
sensitivity = [1.0, 1.0, 1.0]
extinction_threshold = 0.1
environmental_noise = 0.0

interaction_matrix = [
  [-0.01, -0.02,  0.00],
  [ 0.01, -0.01, -0.03],
  [ 0.00,  0.02, -0.01]
]
```

Parameters:

- `species`: species order
- `initial_state`: initial state in the same order
- `growth` or `growth.<species>`: intrinsic growth `r_i`
- `sensitivity` or `sensitivity.<species>`: sensitivity `s_i`
- `external_input.<species>`: external input `u_i`
- `interaction_matrix[i][j]`: influence of species `j` on species `i`
- `interaction.<species_i>.<species_j>`: scheduled interaction override
- `extinction_threshold`: default `0.1`; values below it become `0.0`; `0.0` disables hard extinction
- `environmental_noise`: default `0.0`; reproducible random noise controlled by `seed`

Positive interaction coefficients help the target species grow. Negative coefficients suppress it.

Supported gLV sensitivity parameters:

```text
growth.<species>
sensitivity.<species>
external_input.<species>
interaction.<species_i>.<species_j>
```

Example:

```text
sim.sensitivity -s scenario-gfl.toml -p growth.rabbit --from 0.05 --to 0.3 --samples 20 --output growth_rabbit
```

## Rosenzweig-MacArthur

Formula:

```text
dN/dt = r*N*(1 - N/K) - (a*N*P) / (1 + a*h*N)
dP/dt = e*(a*N*P) / (1 + a*h*N) - m*P
```

Scenario example:

```toml
model = "rosenzweig_macarthur"
species = ["prey", "predator"]
initial_state = [40.0, 9.0]

[parameters]
r = 1.0
K = 100.0
a = 0.1
h = 0.2
e = 0.5
m = 0.2
```

Parameters:

- `r`: prey growth rate
- `K`: carrying capacity
- `a`: predator attack intensity
- `h`: handling time
- `e`: prey-to-predator conversion efficiency
- `m`: predator mortality

Supported RM sensitivity parameters:

```text
r
K
a
h
e
m
```

Example:

```text
sim.sensitivity -s scenario-rm.toml -p h --from 0.01 --to 1.0 --samples 100 --output sensitivity_h
```

## CSV Output

Normal simulation CSV:

```text
output/simulation.csv
```

Important columns:

- `tick`, `time`, `dt`
- `scenario_id`, `model_id`, `seed`, `integrator`
- `checksum`, `flags`
- `state.<species>`
- `metric.<name>`

Sensitivity CSV columns include:

- `sample_index`
- `parameter_name`
- `parameter_value`
- `scenario_id`
- `model_id`
- `seed`
- `final_tick`
- `checksum`
- `state.<species>`
- `metric.<name>`

## OGRE Viewer

Build:

```powershell
cmake -S . -B build_ogre `
  -DECOSIM_BUILD_OGRE_VIEWER=ON `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows

cmake --build build_ogre --config Release
```

Run simulation first:

```powershell
cd build_ogre\Release
.\ecosim.exe configs\app.toml
```

Run viewer:

```powershell
.\ecosim_ogre_viewer.exe output\simulation.csv
.\ecosim_ogre_viewer.exe --input output\simulation.csv
.\ecosim_ogre_viewer.exe --csv output\rm_run.csv
```

The viewer reads extended RecorderCsv output. It does not control `SimulationWorld` or `ScenarioRunner`. After it reaches the last frame, it keeps the final visualization open until the window is closed.
