# Архитектурный обзор EcoSim (без бизнес-логики)

Ниже собрана только архитектура, контракты и связи между подсистемами.

## 1) Общая структура проекта

### Дерево каталогов (без build)

```text
EcoSim/
├── CMakeLists.txt
├── configs/
│   ├── app.toml
│   └── scenario.toml
├── docs/
│   ├── architectural_constraints.md
│   ├── full_program_structure.md
│   ├── modules_and_functions.md
│   └── architecture_overview.md
├── modules/
│   ├── agent_behavoir/manifest.toml
│   ├── recorder/manifest.toml
│   ├── scenario/manifest.toml
│   └── simulation_world/manifest.toml
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── app.h/.cpp
│   │   ├── config.h/.cpp
│   │   ├── event_bus.h/.cpp
│   │   ├── module.h/.cpp
│   │   ├── module_manager.h/.cpp
│   │   └── module_registry.h/.cpp
│   └── modules/
│       ├── world_port.h
│       ├── simulation_world.h/.cpp
│       ├── scenario_runner.h/.cpp
│       ├── recorder_csv.h/.cpp
│       └── agent_behavoir.h/.cpp
└── tests/
    ├── test_event_delivery.cpp
    ├── test_modules_start.cpp
    ├── test_scenario_results.cpp
    ├── test_start_order.cpp
    └── data/...
```

### Где находится ядро
- Ядро находится в `src/core/`.
- Точка входа приложения: `src/main.cpp`.
- Оркестрация ядра: `src/core/app.cpp` (`Application`).

### Где находятся модули
- Манифесты модулей: `modules/*/manifest.toml`.
- C++-реализации модулей: `src/modules/`.

### Где реализован EventBus
- Интерфейс + структура: `src/core/event_bus.h`.
- Реализация: `src/core/event_bus.cpp`.

### Где реализован порт доступа к миру
- Контракт порта: `src/modules/world_port.h` (`IWorldPort`).
- Реализация порта (через модуль мира): `src/modules/simulation_world.h/.cpp` (`SimulationWorld : IWorldPort`).

---

## 2) Контракт модуля

### Интерфейс IModule (аналог)
`src/core/module.h`:

```cpp
class IModule {
public:
    virtual ~IModule() = default;

    virtual const std::string &typeId() const = 0;
    virtual const std::string &instanceId() const = 0;

    virtual void onInit() {}
    virtual void onStart() {}
    virtual void onStop() {}

    virtual void onPreTick() {}
    virtual void onTick() {}
    virtual void onPostTick() {}
    virtual void onDeliverBufferedEvents() {}
};
```

### Все методы жизненного цикла
- Инициализация: `onInit`
- Старт: `onStart`
- Перед тиком: `onPreTick`
- Тик: `onTick`
- После тика: `onPostTick`
- После доставки буферизованных событий: `onDeliverBufferedEvents`
- Остановка: `onStop`

### Где в ядре вызываются onPreTick/onTick/onPostTick/onStop
- `onPreTick`, `onTick`, `onPostTick`, `onDeliverBufferedEvents` вызываются в `Application::runHeadless()` (`src/core/app.cpp`) как последовательные проходы по `module_manager_.modules()`.
- `onStop` вызывается в `ModuleManager::stopModules()` (`src/core/module_manager.cpp`) в обратном порядке (`rbegin() -> rend()`).

---

## 3) Реализация основного tick-цикла

### Где начинается цикл
- Основной цикл стартует в `Application::runHeadless()` (`src/core/app.cpp`).
- Вызов `runHeadless()` инициируется из `main()` (`src/main.cpp`) при `mode == "headless"`.

### Порядок фаз
Внутри каждого тика:
1. `onPreTick()` для всех модулей.
2. `onTick()` для всех модулей.
3. `onPostTick()` для всех модулей.
4. `event_bus_.deliverBuffered()`.
5. `onDeliverBufferedEvents()` для всех модулей.
6. Проверка условий остановки.

### Где вызывается доставка событий
- В `Application::runHeadless()`:

```cpp
event_bus_.deliverBuffered();
```

### Где проверяется условие остановки
- В `Application::runHeadless()`:
  - `world->shouldStop()` (через `IWorldPort`).
  - Лимит `max_ticks` из `AppConfig`.

---

## 4) Порт доступа к миру

### Интерфейс порта (IWorldPort)
`src/modules/world_port.h`:

```cpp
class IWorldPort {
public:
    virtual ~IWorldPort() = default;

    virtual void enqueueCommand(const std::string &command,
                                const std::map<std::string, std::string> &params) = 0;
    virtual const ReadModel &readModel() const = 0;
    virtual bool shouldStop() const = 0;
};
```

### Список методов
- `enqueueCommand(command, params)`
- `readModel() const`
- `shouldStop() const`

### Где хранится очередь команд
- В `SimulationWorld` (`src/modules/simulation_world.h`):

```cpp
std::vector<std::pair<std::string, std::map<std::string, std::string>>> pending_commands_;
```

### Где применяется очередь команд
- В `SimulationWorld::onPreTick()` (`src/modules/simulation_world.cpp`):
  - Проход по `pending_commands_`
  - Вызов `applyCommand(...)`
  - Очистка `pending_commands_.clear()`

### Где вызывается shouldStop
- В `Application::runHeadless()` (`src/core/app.cpp`) через указатель `IWorldPort* world`:

```cpp
if (world->shouldStop()) { ... }
```

---

## 5) EventBus

### Интерфейс
`src/core/event_bus.h`:

```cpp
class EventBus {
public:
    using Handler = std::function<void(const SimulationEvent &)>;

    void subscribe(const std::string &event_type, Handler handler);
    void emit(const SimulationEvent &event);
    void deliverBuffered();
    void clear();

    std::size_t bufferedCount() const;

private:
    std::unordered_map<std::string, std::vector<Handler>> subscribers_;
    std::vector<SimulationEvent> buffer_;
};
```

### Реализация publish
- Метод публикации называется `emit` (`src/core/event_bus.cpp`):

```cpp
void EventBus::emit(const SimulationEvent &event) {
    buffer_.push_back(event);
}
```

### Реализация subscribe
- `EventBus::subscribe` добавляет обработчик в `subscribers_[event_type]`.

### Механизм буферизации событий
- Все события сначала пишутся в `buffer_` через `emit`.
- Буфер хранит события до явной доставки (`deliverBuffered`).

### Механизм доставки событий
- `deliverBuffered()`:
  1. Копирует `buffer_` во временный `to_deliver`.
  2. Очищает `buffer_`.
  3. Для каждого события находит подписчиков по `event.type`.
  4. Вызывает каждый `handler(event)`.

Это обеспечивает пакетную доставку между фазами тика.

---

## 6) Интеграция модулей

### Как модуль сценария получает доступ к порту
- В `Application::initialize()` (`src/core/app.cpp`):
  - `simulation_world` ищется и приводится к `IWorldPort*`.
  - `scenario` ищется и приводится к `ScenarioRunner*`.
  - Затем вызывается `scenario->setWorld(world)`.

### Как модуль записи подписывается на события
- В `RecorderCsv::onStart()` (`src/modules/recorder_csv.cpp`):

```cpp
context_.eventBus().subscribe("world.tick", [this](const SimulationEvent &event) { handleEvent(event); });
```

### Как передаётся ModuleContext / зависимости
- `ModuleContext` содержит ссылки на:
  - `Logger`
  - `EventBus`
  - `AppConfig`
- Передача идёт через фабрики `ModuleRegistry::Factory`:

```cpp
using Factory = std::function<ModulePtr(const ModuleInstanceConfig &, ModuleContext &)>;
```

- `Application` создаёт `ModuleContext context_(logger_, event_bus_, app_config_)` и передаёт его в `ModuleManager`, который вызывает `registry_.create(instance, context_)`.

---

## 7) Конфигурация

### Пример app.toml
`configs/app.toml`:

```toml
mode = "headless"
error_policy = "fail-fast"
modules_dir = "../modules"
scenario_path = "scenario.toml"
output_dir = "../output"
dt = 1.0
max_ticks = 5
instances = [
  { type = "simulation_world", id = "default", enable = true },
  { type = "scenario", id = "default", enable = true },
  { type = "recorder", id = "csv", enable = true, params = { sink = "csv" } }
]
```

### Пример scenario.toml
`configs/scenario.toml`:

```toml
seed = 42
stop_at_tick = 5
requires = ["simulation_world", "recorder"]
schedule = [
  { tick = 1, command = "spawn", species = "rabbit", count = 3 },
  { tick = 2, command = "spawn", species = "fox", count = 1 }
]
```

### Где происходит регистрация модулей
Два канала регистрации:
1. **Статическая регистрация в приложении** (`Application::initialize`, `src/core/app.cpp`):
   - `simulation_world`
   - `scenario`
   - `agent_behavoir`
2. **Динамическая регистрация из библиотеки** (`ModuleRegistry::loadLibrary`, `src/core/module_registry.cpp`):
   - загрузка `.so/.dll/.dylib`
   - вызов экспортируемой функции `ecosimRegisterModule(registry)`
   - пример: `src/modules/recorder_csv.cpp` регистрирует `recorder`

### Как определяется порядок загрузки
- `ModuleManager::startModules()` (`src/core/module_manager.cpp`) строит граф зависимостей из манифестов (`dependencies`).
- Затем вычисляет топологический порядок (`dependencyOrder`).
- В этом порядке вызывает `onInit()` и `onStart()` у модулей.
- При отсутствующих зависимостях поведение зависит от `criticality` модуля и `error_policy` приложения.

