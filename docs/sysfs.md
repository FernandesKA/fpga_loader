# FPGA Manager sysfs Interface

Kernel subsystem: `drivers/fpga/fpga-mgr.c`  
Sysfs root: `/sys/class/fpga_manager/fpga<N>/`

## Attributes

### `name` (read-only)

Human-readable name of the low-level driver, e.g.:
```
Xilinx Zynq FPGA Manager
```

### `state` (read-only)

Current state of the FPGA manager state machine:

| Value | Meaning |
|-------|---------|
| `unknown` | Driver initialised, FPGA state undetermined |
| `charge pump enabled` | Power ramp-up (Zynq-specific) |
| `programming` | Bitstream transfer in progress |
| `initialized` | Programming complete, FPGA initialising |
| `operating` | FPGA running; programming succeeded |
| `fw_upload in progress` | Firmware upload via `fw_upload` interface |

### `status` (read-only)

Bitmask of `enum fpga_mgr_states` status flags.  
Consult `<linux/fpga/fpga-mgr.h>`.

### `flags` (read/write)

Bitmask written before triggering load.  
Values from `<linux/fpga/fpga-mgr.h>`:

| Bit | Constant | Meaning |
|-----|----------|---------|
| 0 | `FPGA_MGR_PARTIAL_RECONFIG` | Partial reconfiguration |
| 1 | `FPGA_MGR_EXTERNAL_CONFIG` | External configuration interface |
| 2 | `FPGA_MGR_ENCRYPTED_BITSTREAM` | Encrypted bitstream |
| 3 | `FPGA_MGR_BITFILE_SWAP_BITS` | Swap bit order in each byte |

### `firmware_name` (read/write) — mainline ≥4.12

Write a filename relative to `/lib/firmware/` to trigger loading.  
Reading returns the last-used filename.

### `firmware` (read/write) — older Xilinx BSP

Same semantics as `firmware_name`.  
Present on Xilinx-patched kernels prior to upstream merge.

## Sequence diagram

```
user-space                      kernel (fpga-mgr)
    |                                |
    |--- write flags --------------->|
    |--- write firmware_name ------->|  request_firmware()
    |                                |--- state = programming
    |                                |--- fpga_mgr_buf_load()
    |                                |--- state = initialized
    |                                |--- state = operating
    |<-- poll state = "operating" ---|
```

## Verifying the driver is loaded

```sh
ls /sys/class/fpga_manager/
# fpga0

cat /sys/class/fpga_manager/fpga0/name
# Xilinx Zynq FPGA Manager
```
