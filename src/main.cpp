#include "dt_overlay.hpp"
#include "fpga_manager.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static void usage(const char* prog)
{
    std::fprintf(stderr,
        "Usage: %s -m bitstream [OPTIONS] <bitstream.bit|.bin>\n"
        "       %s -m overlay  [OPTIONS] --dtbo <file.dtbo>\n"
        "\n"
        "Methods (-m / --method):\n"
        "  bitstream         Trigger load via fpga_manager sysfs (default)\n"
        "                    Xilinx BSP / mainline kernels with firmware_name attr\n"
        "  overlay           Apply device-tree overlay via configfs\n"
        "                    Requires --dtbo and a .dtbo with fpga-region node\n"
        "\n"
        "Options:\n"
        "  -m, --method <method>  Loading method: bitstream|overlay\n"
        "  --manager <path>  Path to fpga_manager sysfs dir\n"
        "                    [default: /sys/class/fpga_manager/fpga0]\n"
        "  --firmware <dir>  Firmware search directory\n"
        "                    [default: /lib/firmware]\n"
        "  --dtbo <path>     Device tree overlay blob (.dtbo) [overlay method only]\n"
        "  --name <name>     Overlay name in configfs [default: fpga0]\n"
        "  --flags <hex>     FPGA manager flags (e.g. 0x1 = partial reconfig)\n"
        "  --timeout <ms>    State poll timeout in ms [default: 5000]\n"
        "  --remove          Remove named overlay and exit [overlay method only]\n"
        "  --verbose         Print progress messages\n"
        "  --help            Show this help\n"
        "\n"
        "Examples:\n"
        "  # Bitstream via fpga_manager (Xilinx BSP):\n"
        "  %s design_1.bit\n"
        "\n"
        "  # ConfigFS overlay (mainline):\n"
        "  %s -m overlay --dtbo fpga.dtbo\n",
        prog, prog, prog, prog);
}

enum class Method { Bitstream, Overlay };

struct Args {
    Method      method          = Method::Bitstream;
    std::string bitstream;
    std::string dtbo;
    std::string manager_path    = "/sys/class/fpga_manager/fpga0";
    std::string firmware_dir    = "/lib/firmware";
    std::string overlay_name    = "fpga0";
    uint32_t    flags           = 0;
    int         timeout_ms      = 5000;
    bool        verbose         = false;
    bool        remove_overlay  = false;
};

static bool parse_args(int argc, char** argv, Args& a)
{
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        auto next = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: %s requires an argument\n", argv[i]);
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h") {
            return false;
        } else if (arg == "--method" || arg == "-m") {
            const char* m = next(); if (!m) return false;
            if (std::strcmp(m, "bitstream") == 0)    a.method = Method::Bitstream;
            else if (std::strcmp(m, "overlay") == 0) a.method = Method::Overlay;
            else { std::fprintf(stderr, "error: unknown method '%s'\n", m); return false; }
        } else if (arg == "--manager") {
            const char* v = next(); if (!v) return false;
            a.manager_path = v;
        } else if (arg == "--firmware") {
            const char* v = next(); if (!v) return false;
            a.firmware_dir = v;
        } else if (arg == "--dtbo") {
            const char* v = next(); if (!v) return false;
            a.dtbo = v;
        } else if (arg == "--name") {
            const char* v = next(); if (!v) return false;
            a.overlay_name = v;
        } else if (arg == "--flags") {
            const char* v = next(); if (!v) return false;
            a.flags = static_cast<uint32_t>(std::strtoul(v, nullptr, 0));
        } else if (arg == "--timeout") {
            const char* v = next(); if (!v) return false;
            a.timeout_ms = std::atoi(v);
        } else if (arg == "--verbose") {
            a.verbose = true;
        } else if (arg == "--remove") {
            a.remove_overlay = true;
        } else if (arg[0] != '-') {
            if (!a.bitstream.empty()) {
                std::fprintf(stderr, "error: multiple bitstream paths given\n");
                return false;
            }
            a.bitstream = arg;
        } else {
            std::fprintf(stderr, "error: unknown option '%s'\n", arg.c_str());
            return false;
        }
    }
    return true;
}

int main(int argc, char** argv)
{
    Args a;
    if (!parse_args(argc, argv, a)) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (a.method == Method::Overlay && a.remove_overlay) {
        fpga::DtOverlay overlay;
        if (!overlay.remove(a.overlay_name))
            return EXIT_FAILURE;
        std::printf("overlay '%s' removed\n", a.overlay_name.c_str());
        return EXIT_SUCCESS;
    }

    if (a.method == Method::Bitstream) {
        if (a.bitstream.empty()) {
            std::fprintf(stderr, "error: bitstream path required for bitstream method\n");
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        fpga::FpgaManagerConfig cfg;
        cfg.manager_path = a.manager_path;
        cfg.firmware_dir = a.firmware_dir;
        cfg.timeout      = std::chrono::milliseconds(a.timeout_ms);
        cfg.verbose      = a.verbose;

        fpga::FpgaManager mgr(cfg);
        if (!mgr.load(a.bitstream, a.flags))
            return EXIT_FAILURE;

        std::printf("FPGA programmed: state=%s\n", mgr.state().c_str());

    } else {
        if (a.dtbo.empty()) {
            std::fprintf(stderr, "error: --dtbo is required for overlay method\n");
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        fpga::DtOverlay overlay;
        if (!overlay.apply(a.overlay_name, a.dtbo))
            return EXIT_FAILURE;

        std::printf("overlay '%s' applied: status=%s\n",
                    a.overlay_name.c_str(), overlay.status(a.overlay_name).c_str());
    }

    return EXIT_SUCCESS;
}
