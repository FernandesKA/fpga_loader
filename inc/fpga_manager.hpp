/**
 * @file fpga_manager.hpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief FPGA Manager interface: loads a bitstream via the Linux
 *        fpga_manager sysfs subsystem (/sys/class/fpga_manager/).
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <chrono>
#include <filesystem>
#include <string>

namespace fpga {

// Userspace mirror of FPGA_MGR_* flags from the Linux kernel header:
// include/linux/fpga/fpga-mgr.h
// Keep in sync with the target kernel.
enum FpgaFlags : uint32_t {
    FpgaFlagNone                = 0,
    FpgaFlagPartialReconfig     = 1u << 0, // FPGA_MGR_PARTIAL_RECONFIG
    FpgaFlagExternalConfig      = 1u << 1, // FPGA_MGR_EXTERNAL_CONFIG
    FpgaFlagEncryptedBitstream  = 1u << 2, // FPGA_MGR_ENCRYPTED_BITSTREAM
    FpgaFlagBitstreamLsbFirst   = 1u << 3, // FPGA_MGR_BITSTREAM_LSB_FIRST
    FpgaFlagCompressedBitstream = 1u << 4, // FPGA_MGR_COMPRESSED_BITSTREAM
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
    bool load(const std::filesystem::path& bitstream, uint32_t flags = FpgaFlagNone);

    // Raw state string from sysfs (e.g. "operating", "programming")
    std::string state() const;

    // Driver name string from sysfs (e.g. "Xilinx Zynq FPGA Manager")
    std::string name() const;

    // True when the fpga_manager sysfs entry exists
    bool available() const;

private:
    bool write_flags(uint32_t flags);
    bool trigger(const std::string& firmware_name);
    bool wait_operating();

    FpgaManagerConfig cfg_;
};

} // namespace fpga
