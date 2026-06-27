# fpga-loader

Утилита для загрузки bitstream в PL-часть Zynq SoC из Linux user-space
через стандартный Linux FPGA subsystem.
---

## Требования

### Общие

- `CONFIG_FPGA=y` — FPGA Manager subsystem
- Компилятор с поддержкой C++17
- CMake ≥ 3.16

Для метода `overlay` дополнительно:

- `CONFIG_CONFIGFS_FS=y` — поддержка файловой системы configfs
- `CONFIG_OF_OVERLAY=y` — поддержка device-tree overlays
- наличие интерфейса `/sys/kernel/config/device-tree/overlays/` в ядре

> Некоторые vendor-ядра (Xilinx BSP) собирают этот интерфейс через
> `CONFIG_OF_CONFIGFS=y`. В mainline-ядрах такого символа может не быть:
> `CONFIG_OF_OVERLAY=y` включает overlay-подсистему, но не гарантирует
> наличие configfs-интерфейса пространства пользователя — это зависит от
> версии и конфигурации ядра. Проверить: `ls /sys/kernel/config/device-tree/overlays`.

### Zynq-7000 (PS: Cortex-A9)

- `CONFIG_FPGA_MGR_ZYNQ_FPGA=y` — драйвер `zynq-fpga`
- Формат bitstream: `.bin` (raw)
- FPGA Manager: `/sys/class/fpga_manager/fpga0` (driver: `Xilinx Zynq FPGA Manager`)

### ZynqMP / UltraScale+ (PS: Cortex-A53)

- `CONFIG_FPGA_MGR_ZYNQMP_FPGA=y` — драйвер `zynqmp-fpga`
- Формат bitstream: `.bin`
- FPGA Manager: `/sys/class/fpga_manager/fpga0` (driver: `Xilinx ZynqMP FPGA Manager`)
- Secure bitstream требует дополнительно `CONFIG_ZYNQMP_FIRMWARE=y`

---

## Сборка

### Buildroot SDK

Активировать окружение SDK, затем вызвать CMake как обычно:

```sh
source /path/to/sdk/environment-setup-<tuple>
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

После `source` CMake автоматически подхватывает `CC`, `CXX`, `CFLAGS`,
`CXXFLAGS`, `LDFLAGS` и `--sysroot` из окружения.

---

## Использование как библиотеки (git submodule)

Библиотека `fpga::loader` предоставляет `FpgaManager` и `DtOverlay`
без CLI-зависимостей. При подключении через `add_subdirectory`
исполняемый файл `fpga-loader` не собирается. Может быть полезным, если хочется использовать fpga_loader как часть своего проекта.

### Подключение

```sh
git submodule add <url> third_party/fpga_loader
```

```cmake
# CMakeLists.txt вашего проекта
add_subdirectory(third_party/fpga_loader)
target_link_libraries(my_app PRIVATE fpga::loader)
```

Заголовки (`fpga_manager.hpp`, `dt_overlay.hpp`, `file_utils.hpp`)
подтягиваются автоматически через PUBLIC include директории таргета.

### Пример

```cpp
#include "fpga_manager.hpp"

fpga::FpgaManagerConfig cfg;
cfg.manager_path = "/sys/class/fpga_manager/fpga0";
cfg.verbose      = true;

fpga::FpgaManager mgr(cfg);
if (!mgr.load("design_1.bit.bin"))
    return -1;
```

### Опции CMake

| Переменная | По умолчанию | Описание |
|-----------|--------------|----------|
| `FPGA_LOADER_BUILD_EXECUTABLE` | `ON` (top-level) / `OFF` (submodule) | Собирать CLI |
| `BUILD_SHARED_LIBS` | `OFF` | Собирать как shared library |

---

## Использование

```
fpga-loader [OPTIONS] <bitstream.bit|.bin>
```

### Опции

| Опция | По умолчанию | Описание |
|-------|-------------|----------|
| `--method sysfs\|overlay` | `sysfs` | Метод загрузки |
| `--manager <path>` | `/sys/class/fpga_manager/fpga0` | Путь к fpga_manager |
| `--firmware <dir>` | `/lib/firmware` | Директория firmware |
| `--dtbo <path>` | — | Overlay-файл `.dtbo` (только `overlay`) |
| `--name <name>` | `fpga0` | Имя overlay в configfs |
| `--flags <hex>` | `0x0` | Флаги fpga_manager (см. ниже) |
| `--timeout <ms>` | `5000` | Таймаут ожидания состояния |
| `--remove` | — | Удалить overlay и выйти |
| `--verbose` | — | Подробный вывод |

### Флаги (`--flags`)

| Значение | Константа ядра | Описание |
|----------|---------------|----------|
| `0x1` | `FPGA_MGR_PARTIAL_RECONFIG` | Частичная реконфигурация |
| `0x2` | `FPGA_MGR_EXTERNAL_CONFIG` | Внешний интерфейс конфигурации |
| `0x4` | `FPGA_MGR_ENCRYPTED_BITSTREAM` | Зашифрованный bitstream |
| `0x8` | `FPGA_MGR_BITFILE_SWAP_BITS` | Зеркалирование бит в байте |

---

## Примеры

### Метод sysfs (Xilinx BSP)

```sh
# Полная реконфигурация
fpga-loader design_1.bit.bin

# С подробным выводом
fpga-loader --verbose design_1.bit.bin

# Частичная реконфигурация
fpga-loader --flags 0x1 partial.bit.bin

# Статус менеджера и активных оверлеев
fpga-loader status

# Другой fpga_manager
fpga-loader --manager /sys/class/fpga_manager/fpga1 design_1.bit
```

### Метод overlay (mainline)

```sh
# 1. Монтировать configfs (один раз при загрузке системы)
sudo mount-configfs.sh

# 2. Загрузить bitstream через overlay
fpga-loader -m overlay --dtbo fpga.dtbo

# 3. Удалить overlay
fpga-loader --method overlay --remove
```

---

## Форматы bitstream

Протестированный flow для Zynq-7000 использует `.bin`, подготовленный
через `bootgen`.

Raw `.bit` напрямую может приниматься некоторыми vendor-драйверами.

Конвертация через `bootgen`:

```sh
# design.bif:
# the_ROM_image: { [destination_device=pl] design_1.bit }

# Zynq-7000
bootgen -image design.bif -arch zynq -o design_1.bit.bin -w

# ZynqMP
bootgen -image design.bif -arch zynqmp -o design_1.bin -w
```

---

## Структура проекта

```
fpga-loader/
├── CMakeLists.txt
├── inc/
│   ├── dt_overlay.hpp       # ConfigFS overlay API
│   ├── file_utils.hpp       # Утилиты: копирование, sysfs I/O
│   └── fpga_manager.hpp     # Sysfs FPGA manager API
├── src/
│   ├── main.cpp
│   ├── dt_overlay.cpp
│   ├── file_utils.cpp
│   └── fpga_manager.cpp
├── docs/
│   ├── zynq7000-flow.md     # Детальный flow загрузки
│   ├── sysfs.md             # Описание sysfs-атрибутов
│   └── configfs.md          # Описание configfs-интерфейса
└── scripts/
    └── mount-configfs.sh    # Монтирование configfs
```

---

## Диагностика

```sh
# Проверить наличие fpga_manager
ls /sys/class/fpga_manager/

# Текущее состояние
cat /sys/class/fpga_manager/fpga0/state

# Лог ядра при загрузке
dmesg | grep -i fpga

# Проверить монтирование configfs
mountpoint /sys/kernel/config
```

Возможные состояния (`state`):

| Состояние | Значение |
|-----------|----------|
| `unknown` | Состояние не определено |
| `charge pump enabled` | Инициализация питания |
| `programming` | Передача bitstream |
| `initialized` | Программирование завершено |
| `operating` | FPGA работает (успех) |

---

## Лицензия

См. [LICENSE](LICENSE).
