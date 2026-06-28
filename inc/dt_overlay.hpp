/**
 * @file dt_overlay.hpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief Device-tree overlay loader via Linux configfs
 *        (/sys/kernel/config/device-tree/overlays/).
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace fpga {

// Device tree overlay loader via Linux configfs
// (/sys/kernel/config/device-tree/overlays/).
//
// Requires:
//   - configfs mounted (see scripts/mount-configfs.sh)
//   - kernel with CONFIG_CONFIGFS_FS=y and CONFIG_OF_OVERLAY=y
//   - a .dtbo containing an fpga-region node referencing the bitstream
class DtOverlay {
public:
    explicit DtOverlay(std::filesystem::path configfs = "/sys/kernel/config");

    // Apply overlay: creates <name> dir and writes dtbo blob.
    // Fails if an overlay with that name already exists unless replace=true.
    bool apply(const std::string& name, const std::filesystem::path& dtbo_path,
               bool replace = false);

    // Remove a previously applied overlay
    bool remove(const std::string& name);

    // Read overlay status ("applied", "loading", "removing", or empty)
    std::string status(const std::string& name) const;

    // List active overlays as {name, status} pairs
    std::vector<std::pair<std::string, std::string>> list() const;

    bool is_mounted() const;

private:
    bool ensure_mounted();
    std::filesystem::path overlay_dir(const std::string& name) const;

    std::filesystem::path configfs_;
};

} // namespace fpga
