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
`species_count`, `dominant_species_index`, `min_population`, `max_population`,
`extinct_species_count` and `extinction.<species>`.

Useful parameter names:

- `growth.rabbit` or `r.rabbit`;
- `sensitivity.rabbit`;
- `external_input.rabbit`;
- `interaction.rabbit.fox`.

`extinction_threshold` defaults to `0.1`. After each numerical step, negative values are
clamped to zero and values below the threshold become exactly zero. A species at zero has
zero derivative and zero interaction contribution, so it cannot resurrect without an explicit
`spawn` or positive `apply_shock`. Set `extinction_threshold = 0.0` to keep only the old
clamp-to-zero behavior.

`environmental_noise` defaults to `0.0`. When positive, the common dynamics base applies a
reproducible multiplicative perturbation after the deterministic step and before clamp /
extinction. The random generator is seeded from `seed`, so fixed seeds reproduce trajectories.

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

`environmental_noise` is supported through the same common post-step layer as gLV. The
state is still clamped after noise, so noise cannot leave negative populations in the
published read model.

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

After each step, negative and near-zero state values are clamped to zero, optional
environmental noise is applied, and the extinction threshold is enforced. The checksum is
based on deterministic FNV-1a hashing of canonical model and world state data.
