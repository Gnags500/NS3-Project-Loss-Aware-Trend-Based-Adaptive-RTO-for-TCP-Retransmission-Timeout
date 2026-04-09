#!/usr/bin/env bash
# run.sh — Build and run paper-rto-proposed (multi-config sweep), then generate plots
# Must be called from anywhere; navigates to ns-3 root automatically.

set -e

NS3_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$NS3_ROOT"

echo "=== [1/2] Running simulation: paper-rto-proposed (W×γ sweep) ==="
./ns3 run scratch/proposed/paper-rto-proposed

echo ""
echo "=== [2/2] Generating plots ==="
python3 scratch/proposed/paper-rto-proposed-plot.py

echo ""
echo "Done. Graphs saved in scratch/proposed/graphs/"
