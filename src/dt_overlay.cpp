#include "dt_overlay.hpp"
#include "file_utils.hpp"

#include <cstdio>
#include <fstream>
#include <vector>

namespace fpga {

DtOverlay::DtOverlay(std::filesystem::path configfs)
    : configfs_(std::move(configfs)) {}

std::filesystem::path DtOverlay::overlay_dir(const std::string& name) const
{
    return configfs_ / "device-tree" / "overlays" / name;
}

bool DtOverlay::is_mounted() const
{
    std::error_code ec;
    return std::filesystem::is_directory(configfs_ / "device-tree", ec);
}

bool DtOverlay::apply(const std::string& name, const std::filesystem::path& dtbo_path)
{
    if (!is_mounted()) {
        std::fprintf(stderr,
            "error: configfs not mounted at %s\n"
            "       run: mount -t configfs configfs /sys/kernel/config\n",
            configfs_.c_str());
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(dtbo_path, ec)) {
        std::fprintf(stderr, "error: overlay not found: %s\n", dtbo_path.c_str());
        return false;
    }

    auto dir = overlay_dir(name);

    // Remove stale overlay from a previous run
    if (std::filesystem::exists(dir, ec)) {
        if (!remove(name))
            return false;
    }

    if (!std::filesystem::create_directory(dir, ec)) {
        std::fprintf(stderr, "error: cannot create %s: %s\n",
                     dir.c_str(), ec.message().c_str());
        return false;
    }

    // Read dtbo file
    std::ifstream in(dtbo_path, std::ios::binary | std::ios::ate);
    if (!in) {
        std::fprintf(stderr, "error: cannot open %s\n", dtbo_path.c_str());
        return false;
    }
    auto size = in.tellg();
    in.seekg(0);
    std::vector<uint8_t> blob(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(blob.data()), size);

    // Writing to dtbo node triggers overlay application
    if (!utils::write_binary(dir / "dtbo", blob)) {
        std::filesystem::remove(dir, ec);
        return false;
    }

    std::string st = status(name);
    if (st != "applied") {
        std::fprintf(stderr, "error: overlay status after apply: '%s'\n", st.c_str());
        std::filesystem::remove(dir, ec);
        return false;
    }

    return true;
}

bool DtOverlay::remove(const std::string& name)
{
    auto dir = overlay_dir(name);
    std::error_code ec;

    if (!std::filesystem::exists(dir, ec))
        return true;

    // Removing the directory triggers kernel overlay teardown
    std::filesystem::remove(dir, ec);
    if (ec) {
        std::fprintf(stderr, "error: cannot remove overlay %s: %s\n",
                     name.c_str(), ec.message().c_str());
        return false;
    }
    return true;
}

std::string DtOverlay::status(const std::string& name) const
{
    return utils::read_sysfs(overlay_dir(name) / "status");
}

} // namespace fpga
