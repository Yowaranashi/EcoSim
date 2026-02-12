# EcoSim

## Сборка

Требования: CMake 3.16+, компилятор C++17.

```bash
cmake -S . -B build
cmake --build build
```

## Запуск

Запуск с конфигом по умолчанию:

```bash
./build/ecosim configs/app.toml
```

Можно передать свой путь к конфигу первым аргументом:

```bash
./build/ecosim /path/to/app.toml
```

Режим запуска задаётся в `app.toml` через поле `mode`:
- `headless` — сразу выполняет сценарий и завершает работу.
- `console` — ожидает команды в консоли (для запуска сценария используйте `sim.run`).

## Запуск тестов

Интеграционные тесты собраны в один раннер: `ecosim_integration_tests` (6 сценариев 5.4.1–5.4.6).

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

При необходимости можно запустить раннер напрямую:

```bash
./build/ecosim_integration_tests
```

### Где лежат тестовые данные

- Тесты **не требуют** ручного копирования `tests/data` в директорию сборки.
- Во время выполнения раннер сам генерирует временные `app_*.toml` и `scenario_*.toml` в `build/test_runtime_data/`.
- Манифесты модулей читаются из `modules/` репозитория через абсолютный путь, поэтому запуск корректен и через `ctest`, и напрямую.

Для генераторов с несколькими конфигурациями (Visual Studio/MSBuild):

```bash
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Установка и упаковка

Установка в директорию (переносит бинарник и данные в дерево установки):

```bash
cmake -S . -B build
cmake --build build
cmake --install build --prefix install
```

Для Visual Studio/MSBuild нужно указать конфигурацию, совпадающую с билдом:

```bash
cmake --build build --config Debug
cmake --install build --prefix install --config Debug
```

Упаковка (ZIP/TGZ с бинарником, конфигами, модулями и документацией):

```bash
cmake -S . -B build
cmake --build build
cmake --build build --target package
```
'console' - чтобы запустить в виде консоли в config

## Команды консоли

Команды регистрируются в `Application::registerCoreCommands` и доступны для выполнения через консольный интерфейс:

- `module.list` — вывести список загруженных модулей.
- `module.start` — не поддерживается в текущем MVP (статические модули).
- `module.stop` — не поддерживается в текущем MVP (статические модули).
- `sim.run` — запустить headless-симуляцию.
- `sim.start` — синоним `sim.run`.
- `sim.pause` — no-op в headless MVP.
- `sim.resume` — no-op в headless MVP.
- `sys.quit` — завершить выполнение (остановить цикл).
