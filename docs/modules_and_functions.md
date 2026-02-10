# Описание модулей и функций EcoSim

### `src/modules/recorder_csv.h` / `src/modules/recorder_csv.cpp`
**Модуль:** `RecorderCsv` (запись событий в CSV и/или память).
- **Назначение:** подписывается на события `world.tick` и сохраняет их в памяти или CSV-файл.
- **Ключевые функции:**
  - `RecorderCsv::RecorderCsv(...)` — читает параметры `sink` и `path`, определяет режим записи.
  - `onStart()` — открывает CSV-файл (если не `sink=memory`), пишет заголовок, подписывается на `world.tick`.
  - `onStop()` — закрывает файл.
  - `events()` — возвращает накопленные события.
  - `handleEvent(...)` — добавляет событие в память и при необходимости пишет строку в CSV.
- **Взаимодействия:**
  - подписывается на `EventBus` через `ModuleContext::eventBus()`;
  - использует `AppConfig::output_dir` для дефолтного пути;
  - экспортирует `ecosimRegisterModule` для регистрации фабрики в `ModuleRegistry` (динамическая загрузка).

### `src/modules/scenario_runner.h` / `src/modules/scenario_runner.cpp`
**Модуль:** `ScenarioRunner` (исполнение сценария).
- **Назначение:** читает сценарий, проверяет зависимости и переводит расписанные действия в команды `SimulationWorld`.
- **Ключевые функции:**
  - `ScenarioRunner::ScenarioRunner(...)` — создает пустой `ScenarioTimeline`.
  - `setAvailableModules(...)` — сохраняет набор доступных модулей для проверки `requires`.
  - `setWorld(IWorldPort *world)` — связывает runner с миром.
  - `onStart()` — загружает `scenario.toml`, проверяет `requires`, строит `ScenarioTimeline`, отправляет команды `world.reset` и `stop.at_tick`.
  - `onPreTick()` — смотрит следующий тик (`readModel().tick + 1`) и запускает действия этого тика.
  - `dispatchAction(...)` — переводит `ScheduledAction` в команды `spawn`, `set_param`, `apply_shock`.
- **Взаимодействия:**
  - использует `ConfigLoader::loadScenario(...)` и `ScenarioTimeline`;
  - вызывает `IWorldPort::enqueueCommand(...)` у мира;
  - использует `ModuleContext::logger()` для ошибок сценария.

### `src/modules/simulation_world.h` / `src/modules/simulation_world.cpp`
**Модуль:** `SimulationWorld` (базовый симулятор).
- **Назначение:** хранит состояние, обрабатывает команды и публикует события тика.
- **Ключевые функции:**
  - `SimulationWorld::SimulationWorld(...)` — сохраняет type/instance, контекст.
  - `onInit()` — сброс состояния мира.
  - `enqueueCommand(...)` — ставит команды в очередь на следующий `onPreTick()`.
  - `onPreTick()` — применяет накопленные команды (`applyCommand`).
  - `onTick()` — увеличивает счетчик тиков, обновляет популяции/энергию, вызывает `emitTickEvent()`.
  - `shouldStop()` — проверяет стоп-условие `stop_at_tick_`.
  - `checksum()` — вычисляет контрольную сумму по состоянию.
- **Внутренние функции:**
  - `applyCommand(...)` — обрабатывает `world.reset`, `spawn`, `set_param`, `apply_shock`, `stop.at_tick`.
  - `emitTickEvent()` — публикует `SimulationEvent` типа `world.tick` через `EventBus`.
- **Взаимодействия:**
  - публикует события в `EventBus` и пишет логи;
  - предоставляет `ReadModel` и интерфейс `IWorldPort` для `ScenarioRunner` и других модулей.

### `src/modules/world_port.h`
**Интерфейс:** `IWorldPort` и модель чтения `ReadModel`.
- **Назначение:** контракт, через который внешние модули (например, `ScenarioRunner`) управляют миром и читают состояние.
- **Ключевые элементы:**
  - `ReadModel` — текущий тик, seed, популяции, энергия.
  - `enqueueCommand(...)`, `readModel()`, `shouldStop()` — минимальный API для работы с миром.

## Файлы `src/core`, взаимодействующие с модулями

### `src/core/app.h` / `src/core/app.cpp`
- **Что делает:** координирует жизненный цикл модулей, собирает зависимости и запускает headless-симуляцию.
- **Взаимодействия с модулями:**
  - регистрирует фабрики `simulation_world`, `scenario`, `agent_behavoir` в `ModuleRegistry`;
  - создает и запускает модули через `ModuleManager`;
  - связывает `ScenarioRunner` с `IWorldPort` (`simulation_world`) и передает список доступных типов модулей;
  - управляет tick-циклом (`onPreTick` → `onTick` → `onPostTick` → доставка событий);
  - проверяет стоп-условия через `IWorldPort::shouldStop()`.

### `src/core/module.h`
- **Что делает:** задает базовый контракт модулей (`IModule`) и общий контекст (`ModuleContext`).
- **Взаимодействия с модулями:**
  - все модули наследуются от `IModule` и реализуют lifecycle-методы;
  - `ModuleContext` передает модулям `Logger`, `EventBus`, `AppConfig`.

### `src/core/module_manager.h` / `src/core/module_manager.cpp`
- **Что делает:** создает модули, упорядочивает запуск по зависимостям и вызывает lifecycle-методы.
- **Взаимодействия с модулями:**
  - строит модули на основе `ModuleInstanceConfig` и фабрик из `ModuleRegistry`;
  - вызывает `onInit()`/`onStart()` в порядке зависимостей;
  - на `stopModules()` вызывает `onStop()` в обратном порядке.

### `src/core/module_registry.h` / `src/core/module_registry.cpp`
- **Что делает:** хранит манифесты и фабрики, при необходимости грузит динамические библиотеки модулей.
- **Взаимодействия с модулями:**
  - читает `manifest.toml` каждого модуля;
  - вызывает экспорт `ecosimRegisterModule` из динамической библиотеки (например, `recorder_csv`) для регистрации фабрики.


### `src/core/event_bus.h` / `src/core/event_bus.cpp`
- **Что делает:** шина событий для симуляции.
- **Взаимодействия с модулями:**
  - `SimulationWorld` публикует `world.tick` через `emit()`;
  - `RecorderCsv` подписывается на события через `subscribe()`;
  - доставка буфера (`deliverBuffered()`) синхронизирована с tick-циклом в `Application`.

