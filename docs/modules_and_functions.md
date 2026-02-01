# Описание модулей и функций EcoSim

Документ перечисляет основные модули и функции кода EcoSim и кратко описывает их назначение, входные данные, выходные данные, а также взаимодействия с интерфейсами, портами и другими модулями.

## Общие замечания о портах и интерфейсах
- Сетевых портов в кодовой базе нет: приложение работает в headless-режиме и не открывает TCP/UDP порты.
- Основные интерфейсы взаимодействия между модулями:
  - `IModule` (жизненный цикл модулей).
  - `EventBus` (публикация/подписка событий).
  - `Console` (регистрация команд и их выполнение).
  - `ModuleRegistry` и `ModuleManager` (реестр/жизненный цикл модулей).
  - `ScenarioTimeline`/`ScenarioRunner` (сценарии и расписание действий).

## src/main.cpp
### `int main(int argc, char **argv)`
- **Назначение:** точка входа приложения; загружает конфиг, инициализирует и запускает модули, выполняет headless-симуляцию.
- **Входные данные:** аргументы командной строки (первый аргумент — путь к `configs/app.toml`, по умолчанию).
- **Выходные данные:** код завершения `0` при успехе; `1` при ошибке инициализации или запуска модулей.
- **Интерфейсы/модули:** использует `Logger`, `Application`.
- **Порты:** не использует.

## src/core/app.h / src/core/app.cpp
### Класс `Application`
- **Назначение:** координация запуска приложения, загрузка конфигов, реестр/создание модулей, headless-цикл симуляции.
- **Входные данные:** зависимости передаются через конструктор (`Logger &`).
- **Выходные данные:** возвращаемые значения методов и обновление состояния приложения.

#### `Application(Logger &logger)`
- **Назначение:** конструирует контекст и менеджер модулей.
- **Вход:** ссылка на `Logger`.
- **Выход:** инициализированный объект `Application`.
- **Интерфейсы:** связывает `Logger`, `EventBus`, `AppConfig`, `ModuleRegistry`, `ModuleManager`, `Console`.

#### `bool initialize(const std::string &config_path)`
- **Назначение:** загрузка `AppConfig`, нормализация путей, загрузка манифестов, регистрация фабрик модулей, сборка модулей, подготовка сценария.
- **Вход:** путь к `app.toml`.
- **Выход:** `true` при успешной инициализации, `false` при ошибке.
- **Интерфейсы/модули:** `ConfigLoader`, `ModuleRegistry`, `ModuleManager`, `SimulationWorld`, `ScenarioRunner`, `RecorderCsv`, `Console`.
- **Порты:** не использует.

#### `bool startModules()`
- **Назначение:** запускает модули через `ModuleManager`.
- **Вход:** нет.
- **Выход:** `true` при успехе, `false` при ошибке.
- **Интерфейсы:** `ModuleManager`.

#### `void runHeadless()`
- **Назначение:** основной headless-цикл симуляции: `onPreTick` → `onTick` → `onPostTick` → доставка событий.
- **Вход:** нет.
- **Выход:** нет; обновляет состояние мира и модулей.
- **Интерфейсы/модули:** `ModuleManager`, `EventBus`, `SimulationWorld`.
- **Порты:** не использует.

#### `void shutdown()`
- **Назначение:** останавливает модули в обратном порядке.
- **Вход/выход:** нет.
- **Интерфейсы:** `ModuleManager`.

#### `void registerCoreCommands()`
- **Назначение:** регистрирует системные команды консоли (`module.list`, `sim.run`, `sys.quit` и т. д.).
- **Вход/выход:** нет.
- **Интерфейсы:** `Console`, `Logger`, `ModuleManager`.

## src/core/module.h
### Класс `ModuleContext`
- **Назначение:** общие зависимости для модулей (логгер, события, конфиг).
- **Вход:** `Logger &`, `EventBus &`, `AppConfig &`.
- **Выход:** доступ через геттеры.
- **Интерфейсы:** `Logger`, `EventBus`, `AppConfig`.

### Интерфейс `IModule`
- **Назначение:** базовый интерфейс жизненного цикла модулей.
- **Ключевые методы:**
  - `typeId()` / `instanceId()` — идентификация.
  - `onInit()`, `onStart()`, `onStop()` — жизненный цикл.
  - `onPreTick()`, `onTick()`, `onPostTick()`, `onDeliverBufferedEvents()` — обработка тиков.
- **Вход/выход:** зависимость от конкретной реализации.
- **Интерфейсы:** используется `ModuleManager`.

## src/core/module_manager.h / src/core/module_manager.cpp
### Класс `ModuleManager`
- **Назначение:** создание модулей и управление их жизненным циклом.

#### `ModuleManager(ModuleRegistry &registry, ModuleContext &context)`
- **Вход:** реестр модулей и контекст.
- **Выход:** инициализированный менеджер.

#### `bool buildModules(const std::vector<ModuleInstanceConfig> &instances, ErrorPolicy policy, Logger &logger)`
- **Назначение:** создает модули по конфигу, учитывая критичность и политику ошибок.
- **Вход:** список инстансов, политика ошибок, логгер.
- **Выход:** `true` при успехе; `false` при критической ошибке.
- **Интерфейсы/модули:** `ModuleRegistry`, `ModuleManifest`.

#### `bool startModules(ErrorPolicy policy, Logger &logger)`
- **Назначение:** проверка зависимостей и запуск модулей в порядке зависимостей.
- **Вход:** политика ошибок, логгер.
- **Выход:** `true`/`false`.
- **Интерфейсы:** `ModuleRegistry`, `IModule`.

#### `void stopModules()`
- **Назначение:** останавливает модули в обратном порядке.
- **Вход/выход:** нет.

#### `std::vector<IModule *> modules() const`
- **Назначение:** возвращает список модулей как указатели.
- **Выход:** массив `IModule *`.

#### `IModule *findModule(const std::string &type_id, const std::string &instance_id = "default") const`
- **Назначение:** поиск модуля по типу и instance-id.
- **Вход:** тип и идентификатор.
- **Выход:** указатель или `nullptr`.

#### `std::vector<std::string> dependencyOrder(const std::map<std::string, std::vector<std::string>> &deps) const`
- **Назначение:** топологическая сортировка зависимостей модулей.
- **Вход:** карта зависимостей.
- **Выход:** порядок типов модулей.

## src/core/module_registry.h / src/core/module_registry.cpp
### Класс `ModuleRegistry`
- **Назначение:** хранит манифесты и фабрики модулей.

#### `void loadManifests(const std::filesystem::path &modules_dir)`
- **Назначение:** загрузка `manifest.toml` для каждого подкаталога.
- **Вход:** путь к каталогу модулей.
- **Выход:** заполняет внутреннюю карту манифестов.
- **Интерфейсы:** `ConfigLoader`.

#### `void registerFactory(const std::string &type_id, Factory factory)`
- **Назначение:** регистрация фабрики модуля по type-id.
- **Вход:** type-id и фабрика.

#### `const ModuleManifest *findManifest(const std::string &type_id) const`
- **Выход:** указатель на манифест или `nullptr`.

#### `ModulePtr create(const ModuleInstanceConfig &instance, ModuleContext &context) const`
- **Назначение:** создание модуля через фабрику.
- **Вход:** конфиг инстанса, контекст.
- **Выход:** `std::unique_ptr<IModule>` или `nullptr`.

## src/core/config.h / src/core/config.cpp
### Структуры конфигов
- **`ModuleManifest`:** метаданные модуля (`type_id`, `version`, `dependencies`, `criticality`).
- **`ModuleInstanceConfig`:** конфиг инстанса (`type_id`, `instance_id`, `enabled`, `params`).
- **`AppConfig`:** глобальные настройки (`mode`, `error_policy`, `modules_dir`, `scenario_path`, `output_dir`, `dt`, `max_ticks`, `instances`).
- **`ScenarioConfig`:** параметры сценария (`seed`, `stop_at_tick`, `requires`, `schedule`).

### Внутренние парсеры (namespace-локальные функции)
#### `std::string trim(const std::string &value)`
- **Назначение:** удаляет пробелы по краям строки.
- **Вход/выход:** строка → строка.

#### `std::string stripQuotes(const std::string &value)`
- **Назначение:** убирает кавычки по краям строки.
- **Вход/выход:** строка → строка.

#### `std::string loadFile(const std::string &path)`
- **Назначение:** читает файл целиком.
- **Вход:** путь к файлу.
- **Выход:** содержимое файла.
- **Интерфейсы:** файловая система.

#### `std::string removeComments(const std::string &input)`
- **Назначение:** удаляет комментарии `#` из содержимого TOML.
- **Вход/выход:** строка → строка.

#### `std::map<std::string, std::string> parseInlineMap(const std::string &input)`
- **Назначение:** парсит inline-таблицы `{ key = value }`.
- **Вход:** строка.
- **Выход:** `map` строк.

#### `std::vector<std::string> parseArrayStrings(const std::string &input)`
- **Назначение:** парсит массив строк `["a", "b"]`.
- **Вход/выход:** строка → `vector<string>`.

#### `std::vector<std::map<std::string, std::string>> parseArrayOfTables(const std::string &input)`
- **Назначение:** парсит массив inline-таблиц в `vector<map>`.

#### `std::optional<std::string> findRawValue(const std::string &input, const std::string &key)`
- **Назначение:** ищет значение по ключу в строке TOML (грубый парсер).
- **Вход:** контент и ключ.
- **Выход:** `optional<string>`.

### Публичные функции
#### `Criticality parseCriticality(const std::string &value)`
- **Назначение:** переводит строку в `Criticality`.
- **Вход:** строка `Critical`/`Important`/другое.
- **Выход:** значение enum.

#### `AppConfig ConfigLoader::loadAppConfig(const std::string &path)`
- **Назначение:** загрузка настроек приложения.
- **Вход:** путь к `app.toml`.
- **Выход:** `AppConfig`.

#### `ModuleManifest ConfigLoader::loadManifest(const std::string &path)`
- **Назначение:** загрузка манифеста модуля.
- **Вход:** путь к `manifest.toml`.
- **Выход:** `ModuleManifest`.

#### `ScenarioConfig ConfigLoader::loadScenario(const std::string &path)`
- **Назначение:** загрузка сценария.
- **Вход:** путь к `scenario.toml`.
- **Выход:** `ScenarioConfig`.

## src/core/logger.h / src/core/logger.cpp
### Класс `Logger`
#### `Logger(std::ostream &output)`
- **Назначение:** логгер с выводом в поток.
- **Вход:** поток `std::ostream`.

#### `void log(LogChannel channel, const std::string &message)`
- **Назначение:** логирование с меткой времени и каналом.
- **Вход:** канал (`System`/`Simulation`) и строка.
- **Выход:** запись в поток.

### Внутренние функции
- `channelLabel(LogChannel channel)` — строковая метка канала.
- `timestamp()` — форматирует текущее время.

## src/core/event_bus.h / src/core/event_bus.cpp
### `struct SimulationEvent`
- **Назначение:** модель события симуляции.
- **Поля:** `type`, `tick`, `payload`.

### Класс `EventBus`
#### `void subscribe(const std::string &event_type, Handler handler)`
- **Назначение:** подписка на события типа `event_type`.
- **Вход:** строка типа события и handler.

#### `void emit(const SimulationEvent &event)`
- **Назначение:** буферизует событие.
- **Вход:** событие.

#### `void deliverBuffered()`
- **Назначение:** доставляет буферизованные события подписчикам.
- **Вход/выход:** нет.

#### `void clear()`
- **Назначение:** очищает буфер событий.

#### `std::size_t bufferedCount() const`
- **Назначение:** размер буфера событий.
- **Выход:** количество.

## src/core/console.h / src/core/console.cpp
### Класс `Console`
#### `void registerCommand(const std::string &name, CommandHandler handler)`
- **Назначение:** регистрация команды консоли.
- **Вход:** имя команды и handler.

#### `bool execute(const std::string &line)`
- **Назначение:** парсит строку и выполняет команду.
- **Вход:** строка.
- **Выход:** `true` если команда найдена и выполнена.

## src/core/scenario.h / src/core/scenario.cpp
### Класс `ScenarioTimeline`
#### `ScenarioTimeline(ScenarioConfig config)`
- **Назначение:** хранит сценарий и сортирует расписание по тикам.
- **Вход:** `ScenarioConfig`.

#### `std::vector<ScenarioConfig::ScheduledAction> actionsForTick(int tick) const`
- **Назначение:** возвращает список действий для конкретного тика.
- **Вход:** номер тика.
- **Выход:** список `ScheduledAction`.

## src/modules/simulation_world.h / src/modules/simulation_world.cpp
### Структура `ReadModel`
- **Назначение:** read-model мира для чтения модулей.
- **Поля:** `tick`, `seed`, `population_by_species`, `energy_total`.

### Класс `SimulationWorld`
- **Назначение:** базовый симулятор; хранит состояние, исполняет команды, генерирует события.

#### `SimulationWorld(const ModuleInstanceConfig &instance, ModuleContext &context)`
- **Вход:** конфиг инстанса, контекст.

#### `void onInit()`
- **Назначение:** сброс состояния мира.

#### `void onPreTick()`
- **Назначение:** применяет накопленные команды.

#### `void onTick()`
- **Назначение:** продвигает симуляцию на тик; инкрементирует популяции и энергию; эмитит событие.

#### `void enqueueCommand(const std::string &command, const std::map<std::string, std::string> &params)`
- **Назначение:** откладывает команду для применения на следующем `onPreTick`.
- **Вход:** имя команды и параметры.

#### `const ReadModel &readModel() const`
- **Назначение:** предоставляет read-model (доступ только для чтения).
- **Выход:** `ReadModel`.

#### `bool shouldStop() const`
- **Назначение:** проверка стоп-условия по `stop_at_tick_`.
- **Выход:** `true`/`false`.

#### `std::string checksum() const`
- **Назначение:** вычисляет простой checksum по состоянию мира.
- **Выход:** строка checksum.

##### Внутренние функции
- `applyCommand(...)` — применяет команды: `world.reset`, `spawn`, `set_param`, `apply_shock`, `stop.at_tick`.
- `emitTickEvent()` — отправляет событие `world.tick` в `EventBus` и пишет лог.

- **Интерфейсы/модули:**
  - Публикует события через `EventBus`.
  - Используется `ScenarioRunner` для выполнения команд.
  - Используется `RecorderCsv` через события `world.tick`.

## src/modules/scenario_runner.h / src/modules/scenario_runner.cpp
### Класс `ScenarioRunner`
- **Назначение:** исполняет сценарий, переводя запланированные действия в команды `SimulationWorld`.

#### `ScenarioRunner(const ModuleInstanceConfig &instance, ModuleContext &context)`
- **Вход:** конфиг инстанса, контекст.

#### `void onStart()`
- **Назначение:** загружает сценарий, проверяет зависимости `requires`, инициализирует timeline, отправляет команды reset/stop.
- **Вход:** читает `context_.config().scenario_path`.
- **Выход:** команда `world.reset`, `stop.at_tick` в `SimulationWorld` (если он доступен).

#### `void onPreTick()`
- **Назначение:** планирует команды на следующий тик по `ScenarioTimeline`.

#### `void setWorld(SimulationWorld *world)`
- **Назначение:** связывает с экземпляром мира.

#### `void setAvailableModules(const std::vector<std::string> &modules)`
- **Назначение:** сохраняет список доступных модулей для валидации `requires`.

##### Внутренняя функция
- `dispatchAction(...)` — переводит `ScheduledAction` в команды (`spawn`, `set_param`, `apply_shock`).

## src/modules/recorder_csv.h / src/modules/recorder_csv.cpp
### Класс `RecorderCsv`
- **Назначение:** подписывается на события `world.tick` и сохраняет их в CSV или в память.

#### `RecorderCsv(const ModuleInstanceConfig &instance, ModuleContext &context)`
- **Вход:** конфиг инстанса; параметры: `sink=memory` (буфер в памяти), `path` (путь к CSV).

#### `void onStart()`
- **Назначение:** открывает CSV-файл (если не `sink=memory`), подписывается на `world.tick`.
- **Интерфейсы:** `EventBus`, файловая система.

#### `void onStop()`
- **Назначение:** закрывает CSV-файл.

#### `const std::vector<SimulationEvent> &events() const`
- **Назначение:** возвращает накопленные события.

##### Внутренняя функция
- `handleEvent(const SimulationEvent &event)` — сохраняет событие в память и CSV (если доступно).

## tests/integration_tests.cpp
### (Описание тестов)
- Набор интеграционных тестов `T1`–`T5`:
  - `T1`: проверяет загрузку конфигов и чтение манифестов.
  - `T2`: проверяет поведение при отсутствующей фабрике Important-модуля и порядок запуска (ожидает `simulation_world` первым).
  - `T3`: проверяет буферизацию и доставку событий в `EventBus`.
  - `T4`: проверяет запуск headless, получение событий `RecorderCsv` и соответствие количества событий тикам мира.
  - `T5`: проверяет детерминизм по checksum для двух прогонов.
- Тесты используют `Application`, `SimulationWorld`, `RecorderCsv`, `EventBus` и ожидают корректные значения tick/events/checksum.
