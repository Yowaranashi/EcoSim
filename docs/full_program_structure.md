# Полная структура программы EcoSim

Ниже приведено описание структуры проекта **EcoSim**: назначения каталогов, ключевых файлов и того, как модули взаимодействуют друг с другом.

## 1. Корень проекта

- `CMakeLists.txt` — основной сценарий сборки CMake: объявляет проект, подключает исходники/тесты и настраивает цели сборки.
- `README.md` — обзор проекта, запуск и базовая документация.
- `LICENSE` — лицензия.
- `.github/workflows/ci.yml` — CI-пайплайн (автосборка/проверки).
- `.gitignore` — правила игнорирования файлов Git.

## 2. Конфигурация

Каталог: `configs/`

- `app.toml` — конфигурация приложения (параметры запуска, логирования и пр.).
- `scenario.toml` — описание сценария моделирования (входные данные для выполнения).

## 3. Документация

Каталог: `docs/`

- `modules_and_functions.md` — документация по модулям и функциям.
- `full_program_structure.md` — полная структура проекта и высокоуровневый runtime-flow.
- `architectural_constraints.md` — архитектурные ограничения существующей системы.

## 4. Манифесты модулей

Каталог: `modules/`

Каждый подкаталог содержит `manifest.toml` с метаданными модуля:

- `modules/scenario/manifest.toml` — модуль сценариев.
- `modules/simulation_world/manifest.toml` — модуль мира/моделирования.
- `modules/agent_behavoir/manifest.toml` — модуль поведения агентов.
- `modules/recorder/manifest.toml` — модуль записи результатов.

## 5. Исходный код

Каталог: `src/`

### 5.1 Точка входа

- `src/main.cpp` — старт программы, инициализация приложения и запуск основного потока выполнения.

### 5.2 Ядро (core)

Каталог: `src/core/`

#### Приложение
- `app.h` / `app.cpp` — класс приложения, оркестрация жизненного цикла.

#### Конфигурация
- `config.h` / `config.cpp` — чтение и управление конфигурацией.

#### Модули и реестр
- `module.h` / `module.cpp` — базовая абстракция/контракт модуля.
- `module_manager.h` / `module_manager.cpp` — управление загрузкой и порядком запуска модулей.
- `module_registry.h` / `module_registry.cpp` — реестр доступных модулей и их фабрик.

#### Событийная шина
- `event_bus.h` / `event_bus.cpp` — publish/subscribe-механизм обмена событиями между компонентами.

#### Служебные подсистемы
- `logger.h` / `logger.cpp` — логирование.
- `console.h` / `console.cpp` — консольный интерфейс/вывод.
- `scenario.h` / `scenario.cpp` — объект и логика сценария на уровне ядра.

### 5.3 Реализации модулей

Каталог: `src/modules/`

#### Scenario Runner
- `scenario_runner.h` / `scenario_runner.cpp` — управление выполнением сценария как процесса.

#### Simulation World
- `simulation_world.h` / `simulation_world.cpp` — состояние и динамика мира моделирования.
- `world_port.h` — интерфейс/порт доступа к миру для других модулей.

#### Agent Behaviour
- `agent_behavoir.h` / `agent_behavoir.cpp` — логика поведения агентов.

#### Recorder CSV
- `recorder_csv.h` / `recorder_csv.cpp` — запись результатов моделирования в CSV.

## 6. Тесты

Каталог: `tests/`

### 6.1 Unit/Integration тесты
- `test_modules_start.cpp` — проверка старта модулей.
- `test_event_delivery.cpp` — проверка доставки событий через Event Bus.
- `test_start_order.cpp` — проверка порядка запуска.
- `test_scenario_results.cpp` — проверка результатов выполнения сценария.
- `test_support.h` — вспомогательная тестовая инфраструктура.

### 6.2 Тестовые данные
Каталог: `tests/data/`
- `app_base.toml` — базовая конфигурация приложения для тестов.
- `app_missing_important.toml` — негативный кейс с неполной конфигурацией.
- `scenario.toml` — тестовый сценарий.

## 7. Поток выполнения программы (высокоуровнево)

1. Запуск из `main.cpp`.
2. Инициализация `core::App` и загрузка конфигурации (`config`).
3. Регистрация/создание модулей через `module_registry` и `module_manager`.
4. Запуск модулей в заданном порядке (`scenario_runner`, `simulation_world`, `agent_behavoir`, `recorder_csv`).
5. Обмен данными через `event_bus`.
6. Выполнение сценария и фиксация результатов в CSV.

## 8. Краткое дерево проекта

```text
EcoSim/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── configs/
│   ├── app.toml
│   └── scenario.toml
├── docs/
│   ├── architectural_constraints.md
│   ├── full_program_structure.md
│   └── modules_and_functions.md
├── modules/
│   ├── agent_behavoir/
│   │   └── manifest.toml
│   ├── recorder/
│   │   └── manifest.toml
│   ├── scenario/
│   │   └── manifest.toml
│   └── simulation_world/
│       └── manifest.toml
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── app.cpp/.h
│   │   ├── config.cpp/.h
│   │   ├── console.cpp/.h
│   │   ├── event_bus.cpp/.h
│   │   ├── logger.cpp/.h
│   │   ├── module.cpp/.h
│   │   ├── module_manager.cpp/.h
│   │   ├── module_registry.cpp/.h
│   │   └── scenario.cpp/.h
│   └── modules/
│       ├── agent_behavoir.cpp/.h
│       ├── recorder_csv.cpp/.h
│       ├── scenario_runner.cpp/.h
│       ├── simulation_world.cpp/.h
│       └── world_port.h
└── tests/
    ├── data/
    │   ├── app_base.toml
    │   ├── app_missing_important.toml
    │   └── scenario.toml
    ├── test_event_delivery.cpp
    ├── test_modules_start.cpp
    ├── test_scenario_results.cpp
    ├── test_start_order.cpp
    └── test_support.h
```
