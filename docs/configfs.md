# ConfigFS Device-Tree Overlay Interface

Kernel subsystem: `drivers/of/configfs.c`  
Kernel config: `CONFIG_OF_CONFIGFS=y`  
Mount point: `/sys/kernel/config/device-tree/overlays/`

## Mounting

```sh
mount -t configfs configfs /sys/kernel/config
# or use scripts/mount-configfs.sh
```

Verify:
```sh
ls /sys/kernel/config/device-tree/overlays/
```

## Applying an overlay

```sh
# Create the overlay directory — this registers it with the kernel
mkdir /sys/kernel/config/device-tree/overlays/fpga0

# Write the compiled .dtbo blob; triggers overlay application
cat fpga.dtbo > /sys/kernel/config/device-tree/overlays/fpga0/dtbo

# Check result
cat /sys/kernel/config/device-tree/overlays/fpga0/status
# → applied
```

## Removing an overlay

```sh
rmdir /sys/kernel/config/device-tree/overlays/fpga0
```

## Overlay directory attributes

| File | Access | Description |
|------|--------|-------------|
| `dtbo` | write | Write compiled overlay blob to apply |
| `status` | read | `loading` / `applied` / `removing` |
| `path` | read | DT path where overlay was applied |

## Overlay source for FPGA loading

The overlay must include an `fpga-region` node.  
The `firmware-name` property is relative to `/lib/firmware/`.

```dts
/dts-v1/;
/plugin/;

/ {
    /* Overlay targets the fpga_full region defined in the base DT */
    fragment@0 {
        target = <&fpga_full>;
        __overlay__ {
            firmware-name = "design_1.bin";
            /* Optional: partial reconfig */
            /* partial-fpga-config; */
        };
    };
};
```

Compile overlay:
```sh
dtc -I dts -O dtb -@ -o fpga.dtbo fpga.dts
```

The `-@` flag generates symbols needed for overlay resolution.

## Base device-tree requirements

The base DT must define the `fpga_full` (or equivalent) fpga-region:

```dts
fpga_full: fpga-region0 {
    compatible = "fpga-region";
    fpga-mgr = <&devcfg>;          /* Zynq-7000 devcfg */
    /* fpga-mgr = <&fpga_mgr>; */ /* ZynqMP PSU */
    #address-cells = <1>;
    #size-cells = <1>;
    ranges;
};
```

## Troubleshooting

| Symptom | Likely cause |
|---------|--------------|
| `mkdir: cannot create directory` | configfs not mounted or `CONFIG_OF_CONFIGFS` not set |
| `status` reads empty after dtbo write | Overlay parse error; check `dmesg` |
| `status` never becomes `applied` | FPGA manager error; check `dmesg` and `/sys/class/fpga_manager/fpga0/state` |
| `rmdir` fails | Overlay has live consumers (devices that were probed against it) |
