/**
 * @file fpga_manager.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief FPGA Manager implementation: firmware copy, sysfs trigger,
 *        and state polling until the PL reaches 'operating'.
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "fpga_manager.hpp"
#include "file_utils.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

namespace fpga {

static constexpr const char* kStateOperating = "operating";

FpgaManager::FpgaManager(FpgaManagerConfig cfg) : cfg_(std::move(cfg)) {}

bool FpgaManager::available() const
{
    std::error_code ec;
    return std::filesystem::exists(cfg_.manager_path, ec);
}

std::string FpgaManager::state() const
{
    return utils::read_sysfs(cfg_.manager_path / "state");
}

std::string FpgaManager::name() const
{
    return utils::read_sysfs(cfg_.manager_path / "name");
}

bool FpgaManager::load(const std::filesystem::path& bitstream, uint32_t flags)
{
    if (!available()) {
        std::fprintf(stderr, "error: fpga manager not found at %s\n",
                     cfg_.manager_path.c_str());
        return false;
    }

    if (cfg_.verbose)
        std::printf("[fpga] manager: %s\n", cfg_.manager_path.c_str());

    std::string firmware_name;
    if (!utils::copy_firmware(bitstream, cfg_.firmware_dir, firmware_name))
        return false;

    if (cfg_.verbose)
        std::printf("[fpga] firmware: %s/%s\n",
                    cfg_.firmware_dir.c_str(), firmware_name.c_str());

    if (!write_flags(flags))
        return false;

    if (!trigger(firmware_name))
        return false;

    return wait_operating();
}

bool FpgaManager::write_flags(uint32_t flags)
{
    auto node = cfg_.manager_path / "flags";
    // Node may not exist on all kernel versions; skip if absent
    std::error_code ec;
    if (!std::filesystem::exists(node, ec))
        return true;

    return utils::write_sysfs(node, std::to_string(flags));
}

bool FpgaManager::trigger(const std::string& firmware_name)
{
    // Mainline kernels >=4.12 expose "firmware_name"; older Xilinx BSPs use "firmware"
    for (const char* attr : {"firmware_name", "firmware"}) {
        auto node = cfg_.manager_path / attr;
        std::error_code ec;
        if (!std::filesystem::exists(node, ec)) continue;

        if (cfg_.verbose)
            std::printf("[fpga] writing '%s' to %s\n", firmware_name.c_str(), node.c_str());

        if (!utils::write_sysfs(node, firmware_name)) {
            std::fprintf(stderr, "error: failed to trigger FPGA load via %s\n", node.c_str());
            return false;
        }
        return true;
    }

    std::fprintf(stderr,
        "error: no firmware trigger attribute found under %s\n"
        "       (tried 'firmware_name' and 'firmware')\n"
        "       Use --method overlay with a .dtbo for mainline kernels.\n",
        cfg_.manager_path.c_str());
    return false;
}

bool FpgaManager::wait_operating()
{
    using clock = std::chrono::steady_clock;
    auto deadline = clock::now() + cfg_.timeout;

    while (clock::now() < deadline) {
        std::string s = state();

        if (cfg_.verbose)
            std::printf("[fpga] state: %s\n", s.c_str());

        if (s == kStateOperating)
            return true;

        if (s.find("error") != std::string::npos) {
            std::fprintf(stderr, "error: FPGA manager entered error state: '%s'\n", s.c_str());
            return false;
        }
        if (s == "unknown") {
            std::fprintf(stderr,
                "error: FPGA manager state is 'unknown' after programming request\n");
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::fprintf(stderr, "error: timeout waiting for FPGA state 'operating' (last: %s)\n",
                 state().c_str());
    return false;
}

} // namespace fpga
