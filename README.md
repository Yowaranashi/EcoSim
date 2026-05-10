# EcoSim

EcoSim - модульная headless-система симуляции экосистем на C++17. Проект рассчитан на пакетные прогоны сценариев из TOML, детерминированный tick-loop, запись результатов в CSV и дальнейшее подключение viewer-а, который сможет читать готовые результаты.

## Что уже есть

- Модульная архитектура: `Application`, `ModuleRegistry`, `ModuleManager`, `IModule`.
- Буферизованный `EventBus`: `emit()` только кладет событие в буфер, `deliverBuffered()` доставляет события после tick-фаз.
- `SimulationWorld`: хранит состояние мира, выполняет математическую динамику, публикует `world.tick`.
- `ScenarioRunner`: читает TOML-сценарий и отправляет команды миру.
- `RecorderCsv`: пишет расширенный `world.tick` в CSV.
- Математический слой в `src/models`, отделенный от модулей приложения.
- Модели:
  - generalized Lotka-Volterra (`glv`);
  - Rosenzweig-MacArthur (`rosenzweig_macarthur`, `rm`).
- Интеграторы:
  - Euler;
  - RK4.
- Интеграционные тесты для конфигурации, EventBus, сценариев, моделей, связи мира с динамикой, CSV и примеров.

## Структура проекта

```text
EcoSim/
  CMakeLists.txt
  README.md
  configs/
    app.toml
    scenario.toml
    examples/
  docs/
  modules/
    */manifest.toml
    recorder/recorder_csv.dll
  src/
    main.cpp
    core/
    models/
    modules/
  tests/
    data/
    integration/
```

Основные каталоги:

- `src/core` - ядро приложения: конфиги, EventBus, модули, реестр, приложение.
- `src/models` - чистый вычислительный слой, не зависящий от `Application`, `EventBus`, CSV и OGRE.
- `src/modules` - runtime-модули: мир, runner сценариев, CSV recorder.
- `configs` - базовые и демонстрационные TOML-конфиги.
- `modules` - манифесты модулей и динамическая библиотека recorder-а.
- `tests/integration` - интеграционный test runner.

## Требования

- CMake 3.16 или новее.
- Компилятор с поддержкой C++17.
- Windows: Visual Studio/MSVC или другой C++17 toolchain.
- Linux/macOS: GCC/Clang с C++17.

## Сборка

### Универсальный вариант

```bash
cmake -S . -B build
cmake --build build
```

### Visual Studio generator

```bash
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
```

Для `Release`:

```bash
cmake --build build --config Release
```

### NMake на Windows

Команды нужно запускать из Developer PowerShell или Developer Command Prompt, где доступны `cl.exe`, `rc.exe` и `mt.exe`.

```powershell
cmake -S . -B build\nmake_release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build\nmake_release
```

## Запуск проекта

По умолчанию приложение читает `configs/app.toml`:

```bash
./build/ecosim
```

Или можно передать путь к app-конфигу:

```bash
./build/ecosim configs/app.toml
./build/ecosim configs/examples/app_glv.toml
./build/ecosim configs/examples/app_rm.toml
```

На Windows путь к бинарнику зависит от генератора:

```powershell
.\build\nmake_release\ecosim.exe configs\examples\app_glv.toml
.\build\Debug\ecosim.exe configs\examples\app_glv.toml
```

## Режимы запуска

Режим задается в app TOML:

```toml
mode = "headless"
```

Поддерживаемые значения:

- `headless` - сразу выполнить сценарий и завершиться.
- `console` - запустить интерактивную консоль.

Если указан неизвестный режим, приложение пишет предупреждение и запускается как `headless`.

## App-конфиг

Пример:

```toml
mode = "headless"
error_policy = "fail-fast"
modules_dir = "../modules"
scenario_path = "scenario.toml"
output_dir = "../output"
dt = 1.0
max_ticks = 100

instances = [
  { type = "simulation_world", id = "default", enable = true },
  { type = "scenario", id = "default", enable = true },
  { type = "recorder", id = "csv", enable = true, params = { sink = "csv" } }
]
```

Поля:

- `mode` - `headless` или `console`.
- `error_policy` - `fail-fast` или `auto-disable`.
- `modules_dir` - каталог с manifest.toml модулей.
- `scenario_path` - путь к TOML-сценарию.
- `output_dir` - каталог для результатов.
- `dt` - fallback timestep приложения.
- `max_ticks` - верхняя граница tick-loop.
- `instances` - включенные экземпляры модулей.

## Консольные команды

Команды доступны в режиме:

```toml
mode = "console"
```

Список команд:

- `help` - вывести список доступных команд.
- `module.list` - вывести загруженные модули.
- `module.start` - команда зарегистрирована, но в MVP не поддерживает динамический старт.
- `module.stop` - команда зарегистрирована, но в MVP не поддерживает динамическую остановку.
- `sim.run` - выполнить headless-симуляцию.
- `sim.start` - синоним `sim.run`.
- `sim.pause` - no-op в headless MVP.
- `sim.resume` - no-op в headless MVP.
- `sys.quit` - завершить консольный цикл и приложение.

## Сценарии

Сценарий описывает seed, модель, интегратор, начальное состояние и расписание команд.

### Старый совместимый формат

```toml
seed = 42
stop_at_tick = 5
requires = ["simulation_world", "recorder"]

schedule = [
  { tick = 1, command = "spawn", species = "rabbit", count = 3 },
  { tick = 2, command = "spawn", species = "fox", count = 1 }
]
```

Этот формат продолжает работать. Если математическая модель не задана, мир может использовать legacy-поведение.

### gLV сценарий

```toml
scenario_id = "glv_three_species_euler"
model = "glv"
seed = 42
dt = 0.1
stop_at_tick = 100
integrator = "euler"

species = ["grass", "rabbit", "fox"]
initial_state = [100.0, 30.0, 5.0]
growth = [0.4, 0.2, -0.1]
sensitivity = [1.0, 1.0, 1.0]

interaction_matrix = [
  [-0.01, -0.02,  0.00],
  [ 0.01, -0.01, -0.03],
  [ 0.00,  0.02, -0.01]
]

requires = ["simulation_world", "recorder"]

schedule = [
  { tick = 20, command = "apply_shock", target = "rabbit", strength = -10.0 },
  { tick = 50, command = "set_param", name = "growth.rabbit", value = 0.1 }
]
```

Формула gLV:

```text
dN_i/dt = N_i * (r_i + sum_j(a_ij * N_j) + b_i * u_i(t))
```

### Rosenzweig-MacArthur сценарий

```toml
scenario_id = "rm_predator_prey"
model = "rosenzweig_macarthur"
seed = 42
dt = 0.05
stop_at_tick = 200
integrator = "rk4"

species = ["prey", "predator"]
initial_state = [40.0, 9.0]

[parameters]
r = 1.0
K = 100.0
a = 0.1
h = 0.2
e = 0.5
m = 0.2

requires = ["simulation_world", "recorder"]

schedule = [
  { tick = 80, command = "apply_shock", target = "prey", strength = -15.0 }
]
```

Параметры RM:

- `r` - скорость роста prey.
- `K` - carrying capacity.
- `a` - attack rate.
- `h` - handling time.
- `e` - conversion efficiency.
- `m` - predator mortality.

## Поддерживаемые model id

- `glv`
- `generalized_lotka_volterra`
- `rosenzweig_macarthur`
- `rm`

## Поддерживаемые integrator

- `euler`
- `rk4`

## Команды schedule

Команды выполняет `ScenarioRunner`, а применяет `SimulationWorld`.

### spawn

```toml
{ tick = 1, command = "spawn", species = "rabbit", count = 3 }
```

Назначение: добавить особей/биомассу виду.

Параметры:

- `species` - имя вида.
- `count` - добавляемое значение.

Для подключенной математической модели команда работает как положительный shock.

### set_param

```toml
{ tick = 50, command = "set_param", name = "growth.rabbit", value = 0.1 }
```

Назначение: изменить параметр модели.

Параметры:

- `name` - имя параметра.
- `value` - новое численное значение.

Для gLV поддерживаются:

- `growth.<species>`
- `sensitivity.<species>`
- `interaction.<species_i>.<species_j>`
- `external_input.<species>`

Для RM поддерживаются:

- `r`, `growth`, `resource_growth`
- `K`, `carrying_capacity`
- `a`, `attack_rate`
- `h`, `handling_time`
- `e`, `conversion_efficiency`
- `m`, `mortality`

### apply_shock

```toml
{ tick = 80, command = "apply_shock", target = "prey", strength = -15.0 }
```

Назначение: мгновенно изменить состояние вида.

Параметры:

- `target` - имя вида.
- `strength` - изменение состояния. Отрицательные значения уменьшают популяцию; состояние clamp-ится к нулю.

### stop.at_tick

Эта команда обычно выставляется автоматически из `stop_at_tick`.

```toml
{ tick = 0, command = "stop.at_tick", value = 100 }
```

Назначение: остановить tick-loop при достижении указанного tick.

## CSV вывод

`RecorderCsv` подписывается на `world.tick`.

Для расширенного события CSV содержит:

- `tick`
- `time`
- `dt`
- `scenario_id`
- `model_id`
- `seed`
- `integrator`
- `checksum`
- `flags`
- `state.<species>`
- `metric.<name>`

Пример пути вывода:

```text
output/examples/simulation.csv
```

Для legacy-событий сохраняется старый CSV формат:

```text
tick,seed,energy_total
```

## Запуск тестов

Все интеграционные тесты собраны в один executable: `ecosim_integration_tests`.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Для Visual Studio:

```bash
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Для NMake build:

```powershell
cmake --build build\nmake_release
ctest --test-dir build\nmake_release --output-on-failure
```

Запуск runner напрямую:

```bash
./build/ecosim_integration_tests
```

На Windows:

```powershell
.\build\nmake_release\ecosim_integration_tests.exe
```

## Текущие группы тестов

- Dependency resolution.
- Spawn pre-tick phase.
- Event buffering.
- Stop condition.
- Recorder isolation.
- Reproducibility.
- Scenario parsing.
- World tick contract.
- Model dynamics base.
- gLV derivatives/Euler/RK4/Jacobian/set_param/shock/checksum.
- Rosenzweig-MacArthur derivatives/Euler/RK4/set_param/shock/Jacobian/equilibrium/checksum.
- Model dynamics factory.
- SimulationWorld + dynamics.
- ScenarioRunner end-to-end.
- RecorderCsv extended output.
- Example TOML to CSV.

## Установка

```bash
cmake --install build --prefix install
```

Для multi-config:

```bash
cmake --install build --prefix install --config Debug
```

## Упаковка

```bash
cmake --build build --target package
```

CPack собирает ZIP/TGZ с бинарником, конфигами, модулями и документацией.

