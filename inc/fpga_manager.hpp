#pragma once

#include <chrono>
#include <filesystem>
#include <string>

namespace fpga {

// FPGA_MGR_* flags from <linux/fpga/fpga-mgr.h>
enum FpgaFlags : uint32_t {
    FlagNone            = 0,
    FlagPartialReconfig = 1u << 0,
    FlagExternalConfig  = 1u << 1,
    FlagEncrypted       = 1u << 2,
    FlagBitSwap         = 1u << 3,
};

struct FpgaManagerConfig {
    std::filesystem::path manager_path = "/sys/class/fpga_manager/fpga0";
    std::filesystem::path firmware_dir = "/lib/firmware";
    std::chrono::milliseconds timeout{5000};
    bool verbose = false;
};

class FpgaManager {
public:
    explicit FpgaManager(FpgaManagerConfig cfg = {});

    // Copy bitstream to /lib/firmware and trigger load via sysfs.
    // Supported on Xilinx BSP kernels and mainline >=4.12 with fpga-region sysfs.
    bool load(const std::filesystem::path& bitstream, uint32_t flags = FlagNone);

    // Raw state string from sysfs (e.g. "operating", "programming")
    std::string state() const;

    // True when the fpga_manager sysfs entry exists
    bool available() const;

private:
    bool write_flags(uint32_t flags);
    bool trigger(const std::string& firmware_name);
    bool wait_operating();

    FpgaManagerConfig cfg_;
};

} // namespace fpga
