# Experiments

EcoSim is intended to run diploma experiments in headless batch mode. The main app reads an
application config, loads modules, runs a scenario and writes CSV through `RecorderCsv`.

## Demo Scenarios

The `configs/examples` directory contains ready examples:

- `scenario_glv_euler.toml`: two-species gLV with Euler;
- `scenario_glv_rk4.toml`: the same gLV setup with RK4;
- `scenario_rm_rk4.toml`: Rosenzweig-MacArthur with RK4;
- `scenario_glv_shock.toml`: three-species gLV with `apply_shock` and `set_param`;
- `glv_three_species_euler.toml` and `glv_three_species_rk4.toml`: longer three-species
  comparison scenarios;
- `rm_predator_prey.toml` and `rm_shock.toml`: additional predator-prey examples.

If the application config accepts only one scenario path, set `scenario_path` in
`configs/app.toml` or copy one example path into an app config. Ready app configs are also
available: `app_glv_euler.toml`, `app_glv_rk4.toml`, `app_rm_rk4.toml`, `app_glv_shock.toml`,
`app_glv.toml` and `app_rm.toml`.

## Running

From a configured build directory:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

For a fresh single-config Ninja build:

```powershell
cmake -S . -B build_ninja -G Ninja
cmake --build build_ninja
ctest --test-dir build_ninja --output-on-failure
```

Run the headless application with an app config:

```powershell
.\build_ninja\ecosim.exe configs\examples\app_glv.toml
.\build_ninja\ecosim.exe configs\examples\app_rm.toml
.\build_ninja\ecosim.exe configs\examples\app_glv_euler.toml
.\build_ninja\ecosim.exe configs\examples\app_glv_rk4.toml
.\build_ninja\ecosim.exe configs\examples\app_rm_rk4.toml
.\build_ninja\ecosim.exe configs\examples\app_glv_shock.toml
```

The CSV output path is controlled by `output_dir` and recorder module parameters. Example
tests write to `output/examples/simulation.csv`.

## CSV Output

The recorder keeps legacy support for old events, but mathematical runs use the extended
wide format:

```text
tick,time,dt,scenario_id,model_id,seed,integrator,checksum,flags,state.<component>,metric.<name>
```

Important columns for experiments:

- `tick`, `time`, `dt`;
- `scenario_id`, `model_id`, `integrator`, `seed`;
- `checksum`;
- `state.rabbit`, `state.fox`, `state.prey`, `state.predator`, or other component names;
- `metric.biomass_total`;
- model-specific metrics such as `metric.predation_flow`.

## What To Compare

Use `scenario_glv_euler.toml` and `scenario_glv_rk4.toml` to compare numerical integration
methods on the same initial state and parameters. Plot component state against `time` and
compare drift, smoothness and final biomass.

Use `scenario_rm_rk4.toml` to plot predator-prey phase dynamics: `state.prey` against
`state.predator`, or `metric.phase_x` against `metric.phase_y`.

Use `scenario_glv_shock.toml` to demonstrate scenario control: a shock changes state at a
specific tick, and `set_param` changes model behavior later in the run.
