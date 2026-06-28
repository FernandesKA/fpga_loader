# fpga-loader

Загрузка bitstream в PL-часть Zynq SoC через Linux FPGA subsystem.

---

## Требования

- `CONFIG_FPGA=y`, `CONFIG_FPGA_MGR_ZYNQ_FPGA=y` (или `ZYNQMP`)
- Для overlay: `CONFIG_CONFIGFS_FS=y`, `CONFIG_OF_OVERLAY=y`
- C++17, CMake ≥ 3.16

---

## Сборка

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Кросс-компиляция через Buildroot SDK: `source environment-setup-<tuple>` перед вызовом CMake.

---

## Использование

```
fpga-loader status
fpga-loader [OPTIONS] <bitstream.bit.bin>
fpga-loader -m overlay --dtbo <file.dtbo> [--name <name>]
```

| Опция | По умолчанию | Описание |
|-------|-------------|----------|
| `-m bitstream\|overlay` | `bitstream` | Метод загрузки |
| `--manager <path>` | `/sys/class/fpga_manager/fpga0` | Путь к fpga_manager |
| `--firmware <dir>` | `/lib/firmware` | Директория firmware |
| `--dtbo <path>` | — | `.dtbo` файл (только `overlay`) |
| `--name <name>` | `fpga0` | Имя overlay в configfs |
| `--flags <hex>` | `0x0` | Флаги fpga_manager (см. таблицу ниже) |
| `--remove` | — | Удалить overlay |
| `--replace` | — | Заменить существующий overlay |
| `--verbose` | — | Подробный вывод |

### Флаги fpga_manager (`--flags`)

Соответствуют `FPGA_MGR_*` из `include/linux/fpga/fpga-mgr.h`. Можно комбинировать через OR.

| Значение | Константа (`FpgaFlags`) | Ядро (`FPGA_MGR_*`) | Описание |
|----------|------------------------|----------------------|----------|
| `0x01` | `FpgaFlagPartialReconfig` | `FPGA_MGR_PARTIAL_RECONFIG` | Частичная реконфигурация |
| `0x02` | `FpgaFlagExternalConfig` | `FPGA_MGR_EXTERNAL_CONFIG` | Конфигурация из внешнего источника |
| `0x04` | `FpgaFlagEncryptedBitstream` | `FPGA_MGR_ENCRYPTED_BITSTREAM` | Зашифрованный bitstream |
| `0x08` | `FpgaFlagBitstreamLsbFirst` | `FPGA_MGR_BITSTREAM_LSB_FIRST` | Биты передаются LSB first |
| `0x10` | `FpgaFlagCompressedBitstream` | `FPGA_MGR_COMPRESSED_BITSTREAM` | Сжатый bitstream |

---

## Лицензия

См. [LICENSE](LICENSE).
