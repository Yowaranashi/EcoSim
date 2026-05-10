# Mathematical Models

EcoSim now has a small standalone dynamics layer in `src/models`. It is independent from
`EventBus`, `ModuleManager`, logging and UI code. `SimulationWorld` owns one dynamics object
and calls it from the existing tick lifecycle.

## Tick Integration

The application lifecycle is unchanged:

1. `ScenarioRunner::onPreTick()` enqueues scheduled commands through `IWorldPort`.
2. `SimulationWorld::onPreTick()` applies pending commands such as `set_param` and
   `apply_shock`.
3. `SimulationWorld::onTick()` advances the selected model by one numerical step.
4. `SimulationWorld` emits a buffered `world.tick` event.
5. `EventBus::deliverBuffered()` delivers the event to subscribers such as `RecorderCsv`.

`world.tick` remains buffered: `emit()` only stores the event, and delivery happens after the
module tick phases.

## generalized Lotka-Volterra

The gLV model is implemented by `GlvDynamics`:

```text
dN_i/dt = N_i * (r_i + sum_j(a_ij * N_j) + external_i)
```

Where:

- `N_i` is biomass or population of component `i`;
- `r_i` is the intrinsic growth rate;
- `a_ij` is the interaction matrix;
- `external_i` is an external input that can be changed through parameters.

The model supports any number of species, named components, initial state, growth rates,
interaction matrix, parameter updates and shocks. Its metrics include `biomass_total`,
`species_count`, `dominant_species_index`, `min_population` and `max_population`.

Useful parameter names:

- `growth.rabbit` or `r.rabbit`;
- `sensitivity.rabbit`;
- `external_input.rabbit`;
- `interaction.rabbit.fox`.

## Rosenzweig-MacArthur

The Rosenzweig-MacArthur predator-prey model is implemented by
`RosenzweigMacArthurDynamics`:

```text
dX/dt = r*X*(1 - X/K) - (a*X*Y)/(1 + a*h*X)
dY/dt = e*(a*X*Y)/(1 + a*h*X) - m*Y
```

Where `X` is prey and `Y` is predator. Parameters are:

- `r`: prey growth rate;
- `K`: carrying capacity;
- `a`: attack rate;
- `h`: handling time;
- `e`: conversion efficiency;
- `m`: predator mortality.

The implementation validates finite, non-negative rates and positive carrying capacity.
Metrics include `prey`, `predator`, `biomass_total`, `predation_flow`, `phase_x` and
`phase_y`.

## Integrators

The common base class provides deterministic Euler and classical RK4 stepping.

Euler:

```text
y_next = y + dt * f(y, t)
```

RK4:

```text
k1 = f(y, t)
k2 = f(y + dt*k1/2, t + dt/2)
k3 = f(y + dt*k2/2, t + dt/2)
k4 = f(y + dt*k3, t + dt)
y_next = y + dt * (k1 + 2*k2 + 2*k3 + k4) / 6
```

After each step, negative and near-zero negative state values are clamped to zero. The
checksum is based on deterministic FNV-1a hashing of canonical model and world state data.
