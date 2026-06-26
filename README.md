# fpga-loader

Утилита командной строки для загрузки bitstream в PL-часть Zynq SoC из Linux user-space
через стандартный Linux FPGA Manager subsystem.

Поддерживает два метода:

| Метод | Ядро | Интерфейс |
|-------|------|-----------|
| `bitstream` | Xilinx BSP / PetaLinux | `/sys/class/fpga_manager/` |
| `overlay` | Mainline ≥ 4.12 | configfs + device-tree overlay |

---

## Требования

### Общие

- `CONFIG_FPGA=y` — FPGA Manager subsystem
- `CONFIG_OF_CONFIGFS=y` — только для метода `overlay`
- Компилятор с поддержкой C++17
- CMake ≥ 3.16

### Zynq-7000 (PS: Cortex-A9)

- `CONFIG_FPGA_MGR_ZYNQ_FPGA=y` — драйвер `zynq-fpga`
- Формат bitstream: `.bit` (Xilinx) или `.bin` (raw)
- FPGA Manager: `/sys/class/fpga_manager/fpga0` (driver: `Xilinx Zynq FPGA Manager`)

### ZynqMP / UltraScale+ (PS: Cortex-A53)

- `CONFIG_FPGA_MGR_ZYNQMP_FPGA=y` — драйвер `zynqmp-fpga`
- Формат bitstream: `.bin` (raw, без заголовка `.bit`)
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


### Установка

```sh
cmake --install build --prefix /usr/local
```

Устанавливает `fpga-loader` и `mount-configfs.sh` в `<prefix>/bin/`.

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
fpga-loader design_1.bit

# С подробным выводом
fpga-loader --verbose design_1.bit

# Частичная реконфигурация
fpga-loader --flags 0x1 partial.bin

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

| Расширение | Описание | Zynq-7000 | ZynqMP |
|-----------|----------|-----------|--------|
| `.bit` | Xilinx bitstream с заголовком | да | нет |
| `.bin` | Raw binary (без заголовка) | да | да |

ZynqMP не принимает `.bit` напрямую — драйвер `zynqmp-fpga` ожидает raw binary.

Конвертация `.bit` → `.bin` через `bootgen`:

```sh
# design.bif:
# the_ROM_image: { [destination_device=pl] design_1.bit }

# Zynq-7000
bootgen -image design.bif -arch zynq -o design_1.bin -w

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
