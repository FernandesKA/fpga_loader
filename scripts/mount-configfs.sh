#!/bin/sh
# Mount configfs if not already mounted.
# Required for the --method overlay flow.

MOUNTPOINT="${1:-/sys/kernel/config}"

if mountpoint -q "$MOUNTPOINT" 2>/dev/null; then
    echo "configfs already mounted at $MOUNTPOINT"
    exit 0
fi

mkdir -p "$MOUNTPOINT"
mount -t configfs configfs "$MOUNTPOINT" || {
    echo "error: failed to mount configfs at $MOUNTPOINT" >&2
    exit 1
}

echo "configfs mounted at $MOUNTPOINT"
