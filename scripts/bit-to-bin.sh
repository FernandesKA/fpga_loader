#!/bin/bash

show_usage() {
    echo "Usage: $0 [options] <input.bit> [output.bin]"
    echo ""
    echo "Options:"
    echo "  -a, --arch <arch>    Архитектура: zynq, zynqmp (по умолчанию: zynqmp)"
    echo "  -h, --help           Показать эту справку"
    echo ""
    echo "Arguments:"
    echo "  input.bit            Входной bitstream файл"
    echo "  output.bin           (опционально) Имя выходного файла"
    echo ""
    echo "Examples:"
    echo "  $0 design.bit                          # ZynqMP, output: design.bit.bin"
    echo "  $0 design.bit fpga.bin                 # ZynqMP, output: fpga.bin"
    echo "  $0 --arch zynq design.bit              # Zynq-7000, output: design.bit.bin"
    echo "  $0 -a zynq design.bit output.bin       # Zynq-7000, output: output.bin"
    exit 0
}

ARCH="zynqmp"

while [[ $# -gt 0 ]]; do
    case $1 in
        -a|--arch)
            ARCH="$2"
            shift 2
            ;;
        -h|--help)
            show_usage
            ;;
        -*)
            echo "Error: Unknown option $1"
            show_usage
            ;;
        *)
            break
            ;;
    esac
done

if [ $# -lt 1 ]; then
    echo "Error: Missing input file"
    echo ""
    show_usage
fi

INPUT_BIT="$1"

if [ ! -f "$INPUT_BIT" ]; then
    echo "Error: File '$INPUT_BIT' not found"
    exit 1
fi

if [ "$ARCH" != "zynq" ] && [ "$ARCH" != "zynqmp" ]; then
    echo "Error: Invalid architecture '$ARCH'"
    echo "Supported architectures: zynq, zynqmp"
    exit 1
fi

if [ $# -ge 2 ]; then
    OUTPUT_BIN="$2"
else
    OUTPUT_BIN="${INPUT_BIT}.bin"
fi

echo "Converting bitstream:"
echo "  Input:       $INPUT_BIT"
echo "  Output:      $OUTPUT_BIN"
echo "  Target arch: $ARCH"

BIF_FILE="bitstream_temp.bif"
echo "all : { $INPUT_BIT }" > "$BIF_FILE"

# Путь к bootgen (настройте под вашу установку Vivado)
BOOTGEN="bootgen"
# Если bootgen не в PATH, раскомментируйте и настройте путь:
BOOTGEN="/home/fka/tools/Xilinx/2025.1/Vitis/bin/bootgen"

if ! command -v $BOOTGEN &> /dev/null; then
    echo "Error: bootgen not found in PATH"
    echo "Please install Xilinx Vivado or set BOOTGEN variable to bootgen path"
    rm -f "$BIF_FILE"
    exit 1
fi

echo "Running bootgen with architecture: $ARCH"
$BOOTGEN -image "$BIF_FILE" -arch "$ARCH" -process_bitstream bin

if [ $? -ne 0 ]; then
    echo "Error: bootgen conversion failed"
    rm -f "$BIF_FILE"
    exit 1
fi

TEMP_BIN="${INPUT_BIT}.bin"

if [ "$TEMP_BIN" != "$OUTPUT_BIN" ]; then
    if [ -f "$TEMP_BIN" ]; then
        mv "$TEMP_BIN" "$OUTPUT_BIN"
    else
        echo "Error: Expected output file '$TEMP_BIN' not found"
        rm -f "$BIF_FILE"
        exit 1
    fi
fi

rm -f "$BIF_FILE"

echo "Conversion completed successfully: $OUTPUT_BIN"
echo "Target architecture: $ARCH"
