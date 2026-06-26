# Zynq-7000 Bitstream Loading Flow

## Bitstream formats

| Format | Extension | Description |
|--------|-----------|-------------|
| Xilinx bitstream | `.bit` | Has 64-byte header; accepted by Xilinx FPGA manager |
| Raw binary | `.bin` | Header stripped; used by some mainline flows |

Convert with `bootgen` if needed:
```
bootgen -image design.bif -arch zynq -o design.bin -w
```

---

## Method 1 — sysfs (Xilinx BSP / PetaLinux)

Supported on kernels shipped by Xilinx (4.14, 5.4, 5.15 xlnx branches).

```
/sys/class/fpga_manager/fpga0/
├── flags          # rw: load flags (0 = full reconfig)
├── firmware_name  # rw: write filename to trigger load  (mainline >= 4.12)
├── firmware       # rw: same, older Xilinx BSP attribute name
├── name           # ro: "Xilinx Zynq FPGA Manager"
└── state          # ro: unknown | charge pump enabled | programming |
                   #     initialized | operating
```

### Steps

1. Copy bitstream to `/lib/firmware/`:
   ```sh
   cp design_1.bit /lib/firmware/
   ```
2. Set flags (0 = full reconfiguration):
   ```sh
   echo 0 > /sys/class/fpga_manager/fpga0/flags
   ```
3. Write firmware name to trigger programming:
   ```sh
   echo "design_1.bit" > /sys/class/fpga_manager/fpga0/firmware_name
   ```
4. Poll state:
   ```sh
   cat /sys/class/fpga_manager/fpga0/state
   # → operating
   ```

### `fpga-loader` command

```sh
fpga-loader design_1.bit
```

---

## Method 2 — configfs + device-tree overlay (mainline)

Requires `CONFIG_OF_CONFIGFS=y` in kernel config.

The overlay (`.dtbo`) must contain an `fpga-region` node that references the
bitstream via the `firmware-name` property.

### Minimal overlay source (`fpga.dts`)

```dts
/dts-v1/;
/plugin/;

/ {
    fragment@0 {
        target = <&fpga_full>;
        __overlay__ {
            firmware-name = "design_1.bin";
        };
    };
};
```

Compile:
```sh
dtc -I dts -O dtb -o fpga.dtbo fpga.dts
```

### Steps

```sh
# 1. Mount configfs (once per boot)
./scripts/mount-configfs.sh

# 2. Copy bitstream to firmware search path
cp design_1.bin /lib/firmware/

# 3. Apply overlay
fpga-loader --method overlay --dtbo fpga.dtbo design_1.bin

# 4. Remove overlay when done
fpga-loader --method overlay --remove
```

---

## Partial reconfiguration

Pass `--flags 0x1` to set `FPGA_MGR_PARTIAL_RECONFIG`:

```sh
fpga-loader --flags 0x1 partial_module.bin
```
