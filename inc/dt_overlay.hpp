#pragma once

#include <filesystem>
#include <string>

namespace fpga {

// Device tree overlay loader via Linux configfs
// (/sys/kernel/config/device-tree/overlays/).
//
// Requires:
//   - configfs mounted (see scripts/mount-configfs.sh)
//   - kernel built with CONFIG_OF_CONFIGFS=y
//   - a .dtbo containing an fpga-region node referencing the bitstream
class DtOverlay {
public:
    explicit DtOverlay(std::filesystem::path configfs = "/sys/kernel/config");

    // Apply overlay: creates <name> dir and writes dtbo blob.
    // name must be unique; use remove() first if reloading.
    bool apply(const std::string& name, const std::filesystem::path& dtbo_path);

    // Remove a previously applied overlay
    bool remove(const std::string& name);

    // Read overlay status ("applied", "loading", "removing", or empty)
    std::string status(const std::string& name) const;

    bool is_mounted() const;

private:
    std::filesystem::path overlay_dir(const std::string& name) const;

    std::filesystem::path configfs_;
};

} // namespace fpga
