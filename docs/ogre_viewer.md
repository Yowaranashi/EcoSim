# OGRE Viewer

OGRE Viewer является отдельным слоем просмотра CSV-результатов. Он не считает модель, не подключается к `SimulationWorld`, не управляет `ScenarioRunner` и не меняет состояние симуляции.

## Запуск

Viewer читает расширенный CSV от `RecorderCsv` с колонками `state.<species>`:

```powershell
.\ecosim_ogre_viewer.exe output\simulation.csv
.\ecosim_ogre_viewer.exe --input output\simulation.csv
.\ecosim_ogre_viewer.exe --csv output\rm_run.csv
```

В console mode можно включить автоматический запуск после симуляции:

```text
ogre.enable
sim.run -s scenario-rm.toml --output rm_run
```

Если указан `sim.run --output <name>`, viewer получает именно этот CSV. Иначе используется стандартный `output/simulation.csv`.

## Что отображается

Главный экран viewer-а - 2D-график изменения состояния видов во времени:

- X axis: `Time`, если в CSV есть ненулевой `time`; иначе `Tick`;
- Y axis: `Population / Biomass`;
- отдельная цветная линия для каждого `state.<species>`;
- общая шкала Y для всех видов, чтобы значения можно было сравнивать;
- сетка по отметкам 0/25/50/75/100%;
- вертикальный marker текущего tick;
- маленькие маркеры текущего значения на каждой линии.

Справа отображаются:

- `Scenario`, `Model`, `Integrator`;
- текущий `Tick`, `Time`, номер кадра, скорость и checksum;
- текущие значения всех species;
- текущие metrics;
- активные flags;
- legend с текущим и максимальным значением каждого species.

Для Rosenzweig-MacArthur дополнительно выводится небольшой phase plot `Prey`/`Predator`, если в CSV есть species `prey` и `predator`.

## Управление

- `Space`: pause/resume.
- `Right arrow`: следующий tick.
- `Left arrow`: предыдущий tick.
- `Up`: увеличить скорость.
- `Down`: уменьшить скорость.
- `R`: сброс к первому tick и очистка уже прорисованной траектории.

После последнего кадра viewer остается открытым, пока окно не будет закрыто.

## Сборка

По умолчанию viewer не собирается, поэтому основной `ecosim` и тесты не зависят от OGRE:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Сборка с OGRE через vcpkg:

```powershell
cmake -S . -B build_ogre_vcpkg `
  -DECOSIM_BUILD_OGRE_VIEWER=ON `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows

cmake --build build_ogre_vcpkg --config Release
```

Если OGRE не найден, CMake выводит предупреждение и не создает `ecosim_ogre_viewer`; основная сборка не должна ломаться.
