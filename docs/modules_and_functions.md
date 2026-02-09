# Описание модулей и функций EcoSim

Документ описывает все модули из `src/modules/` и перечисляет файлы в `src/core`, которые с ними взаимодействуют. Также включены сведения о tick-системе и назначении `src/core/scenario.cpp`.

## Модули в `src/modules/`

### `src/modules/agent_behavoir.h` / `src/modules/agent_behavoir.cpp`
**Модуль:** `AgentBehavoir` (заглушка поведения агента).
- **Назначение:** демонстрационный модуль, который пишет в лог при инициализации и на каждом тике.
- **Ключевые функции:**
  - `AgentBehavoir::AgentBehavoir(...)` — сохраняет тип/инстанс и контекст модуля.
  - `onInit()` — пишет системный лог, что модуль инициализирован.
  - `onTick()` — пишет системный лог на каждом тике.
- **Взаимодействия:** использует `ModuleContext::logger()` для логирования.

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

### `src/core/config.h` / `src/core/config.cpp`
- **Что делает:** описывает конфигурации (`AppConfig`, `ModuleManifest`, `ModuleInstanceConfig`, `ScenarioConfig`) и читает TOML-файлы.
- **Взаимодействия с модулями:**
  - задает список инстансов модулей, пути и параметры;
  - `ScenarioRunner` получает сценарий через `ConfigLoader::loadScenario(...)`.

### `src/core/event_bus.h` / `src/core/event_bus.cpp`
- **Что делает:** шина событий для симуляции.
- **Взаимодействия с модулями:**
  - `SimulationWorld` публикует `world.tick` через `emit()`;
  - `RecorderCsv` подписывается на события через `subscribe()`;
  - доставка буфера (`deliverBuffered()`) синхронизирована с tick-циклом в `Application`.

### `src/core/logger.h` / `src/core/logger.cpp`
- **Что делает:** логирует сообщения с меткой времени и каналом.
- **Взаимодействия с модулями:**
  - все модули используют `ModuleContext::logger()` для системных и симуляционных логов.

### `src/core/scenario.h` / `src/core/scenario.cpp`
- **Что делает:** хранит и предоставляет расписание сценария.
- **Взаимодействия с модулями:**
  - `ScenarioRunner` строит `ScenarioTimeline` и запрашивает действия через `actionsForTick()`.

### `src/core/console.h` / `src/core/console.cpp`
- **Что делает:** регистрирует и исполняет команды.
- **Взаимодействия с модулями:**
  - `Application` регистрирует команды, которые оперируют модулями (`module.list`, `sim.run`, `sys.quit`).

## Как реализована tick-система

Tick-цикл реализован в `Application::runHeadless()` и использует единый порядок вызовов для всех модулей:
1. `onPreTick()` — подготовка к тику (например, `SimulationWorld` применяет накопленные команды, `ScenarioRunner` планирует команды на следующий тик).
2. `onTick()` — основная работа тика (в `SimulationWorld` инкрементируется `ReadModel::tick`, обновляются популяции, публикуется событие `world.tick`).
3. `onPostTick()` — постобработка (при необходимости в будущих модулях).
4. `EventBus::deliverBuffered()` — доставка всех буферизованных событий.
5. `onDeliverBufferedEvents()` — хук на модулях для реакции после доставки.

Завершение цикла происходит, когда `SimulationWorld::shouldStop()` возвращает `true` (например, после команды `stop.at_tick`) или когда достигнут `AppConfig::max_ticks`.

## Для чего нужен `src/core/scenario.cpp`
`src/core/scenario.cpp` реализует класс `ScenarioTimeline`, который:
- принимает `ScenarioConfig`;
- сортирует расписание действий (`schedule`) по номеру тика;
- возвращает действия для конкретного тика через `actionsForTick()`.

Именно этот класс используется `ScenarioRunner` для быстрого получения списка команд, которые нужно применить в конкретный тик.
