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

## Запуск тестов

```bash
cmake --build build
ctest --test-dir build
```

## Установка и упаковка

Установка в директорию (переносит бинарник и данные в дерево установки):

```bash
cmake -S . -B build
cmake --build build
cmake --install build --prefix install
```

Упаковка (ZIP/TGZ с бинарником, конфигами, модулями и документацией):

```bash
cmake -S . -B build
cmake --build build
cmake --build build --target package
```

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
