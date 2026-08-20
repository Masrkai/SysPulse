#!/usr/bin/env bash

# Store original values to restore on exit
export ORIG_KPTR_RESTRICT=$(cat /proc/sys/kernel/kptr_restrict 2>/dev/null || echo "unknown")
export ORIG_PERF_PARANOID=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo "unknown")

# Set kernel parameters for profiling
echo "Setting kernel parameters for profiling..."
echo "Original kptr_restrict: $ORIG_KPTR_RESTRICT"
echo "Original perf_event_paranoid: $ORIG_PERF_PARANOID"

sudo sysctl -w kernel.kptr_restrict=0
sudo sysctl -w kernel.perf_event_paranoid=0

echo "Kernel parameters set. Run 'exit' to restore original values."

# Set up exit trap to restore values
trap 'echo "Restoring kernel parameters..."; sudo sysctl -w kernel.kptr_restrict=$ORIG_KPTR_RESTRICT 2>/dev/null || true; sudo sysctl -w kernel.perf_event_paranoid=$ORIG_PERF_PARANOID 2>/dev/null || true' EXIT