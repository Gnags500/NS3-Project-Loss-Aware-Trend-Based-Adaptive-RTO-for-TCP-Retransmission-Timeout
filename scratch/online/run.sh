#!/usr/bin/env bash

set -e

NS3_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$NS3_ROOT"

echo "=== [1/2] Running simulation: paper-rto-dual ==="
./ns3 run paper-rto-dual

echo ""
echo "=== [2/2] Generating plots ==="
python3 scratch/online/paper-rto-dual-plot.py

echo ""
echo "Done. Graphs saved in scratch/online/graphs/"
