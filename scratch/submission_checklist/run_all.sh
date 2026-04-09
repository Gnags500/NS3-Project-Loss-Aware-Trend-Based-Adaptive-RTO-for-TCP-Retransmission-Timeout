#!/usr/bin/env bash

set -e

NS3_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$NS3_ROOT"

SIMTIME="${SIMTIME:-60}"
QUICK="${QUICK:-0}"

WIRED_CSV="report/csv/wired-results.csv"
WIRELESS_CSV="report/csv/wireless-results.csv"
GRAPHS_DIR="scratch/submission_checklist/graphs"

mkdir -p report/csv "$GRAPHS_DIR"

# ── Decide parameter sweep width ──────────────────────────────────────────────
if [[ "$QUICK" == "1" ]]; then
    SIMTIME=5
    NODES_LIST="20 60 100"
    FLOWS_LIST="10 30 50"
    PPS_LIST="100 300 500"
    COV_LIST="1 3 5"
    echo "[INFO] QUICK mode: 3 values per parameter, simtime=${SIMTIME}s"
else
    NODES_LIST="20 40 60 80 100"
    FLOWS_LIST="10 20 30 40 50"
    PPS_LIST="100 200 300 400 500"
    COV_LIST="1 2 3 4 5"
    echo "[INFO] FULL mode: 5 values per parameter, simtime=${SIMTIME}s"
fi

# Default values 
DEF_NODES=60
DEF_FLOWS=30
DEF_PPS=300
DEF_COV=3    # coverageMult: 1–5, coverage = mult × 250 m

ALGOS="paper proposed"

WIRED_BIN="build/scratch/submission_checklist/wired/ns3.45-wired-sim-default"
WIRELESS_BIN="build/scratch/submission_checklist/wireless/ns3.45-wireless-sim-default"

echo "========================================"
echo "Running ALL experiments"
echo "========================================"

# ── Build ──────────────────────────────────────────────────────────────────────
echo "[Build] Building wired-sim..."
./ns3 build 2>&1 | tail -3
echo "[Build] Done."

# Verify binaries exist
if [[ ! -x "$WIRED_BIN" ]]; then
    echo "[ERROR] Wired binary not found: $WIRED_BIN"
    exit 1
fi
if [[ ! -x "$WIRELESS_BIN" ]]; then
    echo "[ERROR] Wireless binary not found: $WIRELESS_BIN"
    exit 1
fi

# Fresh CSV files (header written by simulation on first row)
rm -f "$WIRED_CSV" "$WIRELESS_CSV"

# ── WIRED experiments ──────────────────────────────────────────────────────────
echo ""
echo "========================================"
echo "WIRED EXPERIMENTS"
echo "========================================"

# Vary nNodes
echo "[Wired] Varying nNodes ($NODES_LIST)..."
for n in $NODES_LIST; do
    for algo in $ALGOS; do
        echo "  nNodes=$n algo=$algo ..."
        "$WIRED_BIN" --nodes="$n" --flows=$DEF_FLOWS --pps=$DEF_PPS \
            --algo="$algo" --output="$WIRED_CSV" --simtime="$SIMTIME" 2>/dev/null
    done
done

# Vary nFlows
echo "[Wired] Varying nFlows ($FLOWS_LIST)..."
for f in $FLOWS_LIST; do
    for algo in $ALGOS; do
        echo "  nFlows=$f algo=$algo ..."
        "$WIRED_BIN" --nodes=$DEF_NODES --flows="$f" --pps=$DEF_PPS \
            --algo="$algo" --output="$WIRED_CSV" --simtime="$SIMTIME" 2>/dev/null
    done
done

# Vary packetsPerSec
echo "[Wired] Varying packetsPerSec ($PPS_LIST)..."
for p in $PPS_LIST; do
    for algo in $ALGOS; do
        echo "  pps=$p algo=$algo ..."
        "$WIRED_BIN" --nodes=$DEF_NODES --flows=$DEF_FLOWS --pps="$p" \
            --algo="$algo" --output="$WIRED_CSV" --simtime="$SIMTIME" 2>/dev/null
    done
done

echo "[Wired] Done. Results in $WIRED_CSV"

# ── WIRELESS 802.11 (STATIC) experiments ──────────────────────────────────────
echo ""
echo "========================================"
echo "WIRELESS 802.11 (STATIC) EXPERIMENTS"
echo "========================================"

# Vary nNodes
echo "[Wireless] Varying nNodes ($NODES_LIST)..."
for n in $NODES_LIST; do
    for algo in $ALGOS; do
        echo "  nNodes=$n algo=$algo ..."
        cov_m=$(( DEF_COV * 250 ))
        "$WIRELESS_BIN" --nodes="$n" --flows=$DEF_FLOWS --pps=$DEF_PPS \
            --coverage="$cov_m" --algo="$algo" --output="$WIRELESS_CSV" --simtime="$SIMTIME" 2>/dev/null
    done
done

# Vary nFlows
echo "[Wireless] Varying nFlows ($FLOWS_LIST)..."
for f in $FLOWS_LIST; do
    for algo in $ALGOS; do
        echo "  nFlows=$f algo=$algo ..."
        cov_m=$(( DEF_COV * 250 ))
        "$WIRELESS_BIN" --nodes=$DEF_NODES --flows="$f" --pps=$DEF_PPS \
            --coverage="$cov_m" --algo="$algo" --output="$WIRELESS_CSV" --simtime="$SIMTIME" 2>/dev/null
    done
done

# Vary packetsPerSec
echo "[Wireless] Varying packetsPerSec ($PPS_LIST)..."
for p in $PPS_LIST; do
    for algo in $ALGOS; do
        echo "  pps=$p algo=$algo ..."
        cov_m=$(( DEF_COV * 250 ))
        "$WIRELESS_BIN" --nodes=$DEF_NODES --flows=$DEF_FLOWS --pps="$p" \
            --coverage="$cov_m" --algo="$algo" --output="$WIRELESS_CSV" --simtime="$SIMTIME" 2>/dev/null
    done
done

# Vary coverageMult (static only) — 1×…5× Tx_range (Tx_range = 250 m)
echo "[Wireless] Varying coverageMult ($COV_LIST)..."
for c in $COV_LIST; do
    for algo in $ALGOS; do
        echo "  coverageMult=$c ($(( c * 250 ))m) algo=$algo ..."
        cov_m=$(( c * 250 ))
        "$WIRELESS_BIN" --nodes=$DEF_NODES --flows=$DEF_FLOWS --pps=$DEF_PPS \
            --coverage="$cov_m" --algo="$algo" --output="$WIRELESS_CSV" --simtime="$SIMTIME" 2>/dev/null
    done
done

echo "[Wireless] Done. Results in $WIRELESS_CSV"

# ── Generate plots ─────────────────────────────────────────────────────────────
echo ""
echo "========================================"
echo "Generating graphs..."
echo "========================================"
python3 scratch/submission_checklist/plot_results.py

echo ""
echo "========================================"
echo "ALL EXPERIMENTS COMPLETE"
echo "Output files:"
echo "  $WIRED_CSV"
echo "  $WIRELESS_CSV"
echo "  Graphs: $GRAPHS_DIR/"
echo ""
echo "Next: python3 scratch/submission_checklist/plot_results.py"
echo "========================================"