/**
 * @file main.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief CLI entry point: argument parsing and dispatch for bitstream
 *        and overlay loading methods.
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "dt_overlay.hpp"
#include "fpga_manager.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <string>

static void usage(const char* prog)
{
    std::fprintf(stderr,
        "Usage: %s status\n"
        "       %s -m bitstream [OPTIONS] <bitstream.bit.bin>\n"
        "       %s -m overlay   [OPTIONS] --dtbo <file.dtbo>\n"
        "\n"
        "Commands:\n"
        "  status            Show FPGA manager state and active overlays\n"
        "\n"
        "Methods (-m / --method):\n"
        "  bitstream         Load via fpga_manager sysfs (default)\n"
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
        "  --replace         Replace existing overlay (implicit --remove + re-apply)\n"
        "                    Default: error if overlay already exists [overlay method only]\n"
        "  --verbose         Print progress messages\n"
        "  --help            Show this help\n"
        "\n"
        "Examples:\n"
        "  %s status\n"
        "  %s design_1.bit.bin\n"
        "  %s -m overlay --dtbo fpga.dtbo\n",
        prog, prog, prog, prog, prog, prog);
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
    bool        replace         = false;
};

static bool parse_args(int argc, char** argv, Args& a)
{
    enum {
        OPT_MANAGER = 256,
        OPT_FIRMWARE,
        OPT_DTBO,
        OPT_NAME,
        OPT_FLAGS,
        OPT_TIMEOUT,
        OPT_REMOVE,
        OPT_REPLACE,
    };

    static const option long_opts[] = {
        { "help",     no_argument,       nullptr, 'h'          },
        { "method",   required_argument, nullptr, 'm'          },
        { "verbose",  no_argument,       nullptr, 'v'          },
        { "manager",  required_argument, nullptr, OPT_MANAGER  },
        { "firmware", required_argument, nullptr, OPT_FIRMWARE },
        { "dtbo",     required_argument, nullptr, OPT_DTBO     },
        { "name",     required_argument, nullptr, OPT_NAME     },
        { "flags",    required_argument, nullptr, OPT_FLAGS    },
        { "timeout",  required_argument, nullptr, OPT_TIMEOUT  },
        { "remove",   no_argument,       nullptr, OPT_REMOVE   },
        { "replace",  no_argument,       nullptr, OPT_REPLACE  },
        { nullptr,    0,                 nullptr, 0            },
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hm:v", long_opts, nullptr)) != -1) {
        switch (opt) {
        case 'h': return false;
        case 'v': a.verbose = true; break;
        case 'm':
            if (std::strcmp(optarg, "bitstream") == 0)    a.method = Method::Bitstream;
            else if (std::strcmp(optarg, "overlay") == 0) a.method = Method::Overlay;
            else { std::fprintf(stderr, "error: unknown method '%s'\n", optarg); return false; }
            break;
        case OPT_MANAGER:  a.manager_path  = optarg; break;
        case OPT_FIRMWARE: a.firmware_dir  = optarg; break;
        case OPT_DTBO:     a.dtbo          = optarg; break;
        case OPT_NAME:     a.overlay_name  = optarg; break;
        case OPT_FLAGS:    a.flags         = static_cast<uint32_t>(std::strtoul(optarg, nullptr, 0)); break;
        case OPT_TIMEOUT:  a.timeout_ms    = std::atoi(optarg); break;
        case OPT_REMOVE:   a.remove_overlay = true; break;
        case OPT_REPLACE:  a.replace       = true; break;
        default: return false;
        }
    }

    if (optind < argc) {
        if (argc - optind > 1) {
            std::fprintf(stderr, "error: multiple bitstream paths given\n");
            return false;
        }
        a.bitstream = argv[optind];
    }

    return true;
}

static int cmd_status(const std::string& manager_path)
{
    fpga::FpgaManagerConfig cfg;
    cfg.manager_path = manager_path;
    fpga::FpgaManager mgr(cfg);

    std::printf("manager : %s\n", manager_path.c_str());
    if (mgr.available()) {
        std::printf("name    : %s\n", mgr.name().c_str());
        std::printf("state   : %s\n", mgr.state().c_str());
    } else {
        std::printf("name    : (not found)\n");
        std::printf("state   : (not found)\n");
    }

    fpga::DtOverlay overlay;
    std::printf("configfs: %s\n", overlay.is_mounted() ? "mounted" : "not mounted");

    if (overlay.is_mounted()) {
        auto overlays = overlay.list();
        if (overlays.empty()) {
            std::printf("overlays: (none)\n");
        } else {
            std::printf("overlays:\n");
            for (auto& [name, st] : overlays)
                std::printf("  %s: %s\n", name.c_str(), st.c_str());
        }
    }

    return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
    if (argc >= 2 && std::string(argv[1]) == "status") {
        std::string manager_path = "/sys/class/fpga_manager/fpga0";
        for (int i = 2; i < argc; ++i) {
            if (std::string(argv[i]) == "--manager" && i + 1 < argc)
                manager_path = argv[++i];
        }
        return cmd_status(manager_path);
    }

    Args a;
    if (!parse_args(argc, argv, a)) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (a.method == Method::Overlay && !a.bitstream.empty()) {
        std::fprintf(stderr,
            "error: unexpected positional argument '%s' with overlay method\n"
            "       the bitstream is referenced inside the .dtbo via firmware-name property\n",
            a.bitstream.c_str());
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
        auto result = mgr.load(a.bitstream, a.flags);
        if (!result) {
            std::fprintf(stderr, "error: %s\n", result.message.c_str());
            return EXIT_FAILURE;
        }

        std::printf("FPGA programmed: state=%s\n", mgr.state().c_str());

    } else {
        if (a.dtbo.empty()) {
            std::fprintf(stderr, "error: --dtbo is required for overlay method\n");
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        fpga::DtOverlay overlay;
        if (!overlay.apply(a.overlay_name, a.dtbo, a.replace))
            return EXIT_FAILURE;

        std::printf("overlay '%s' applied: status=%s\n",
                    a.overlay_name.c_str(), overlay.status(a.overlay_name).c_str());
    }

    return EXIT_SUCCESS;
}
