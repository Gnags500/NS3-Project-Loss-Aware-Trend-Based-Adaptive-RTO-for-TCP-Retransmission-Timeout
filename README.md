# Loss-Aware, Trend-Based Adaptive RTO for TCP Retransmission Timeout

**NS-3 Simulation Project**  
Dhruba Kishore Nag (Student ID: 2105062)  
Supervisor: Ishrat Jahan Maam  
Department of Computer Science and Engineering, BUET  
April 2026

---

## Overview

This project implements and evaluates two adaptive RTO (Retransmission Timeout) estimators for TCP in the ns-3.45 network simulator:

1. **Paper algorithm (AMEII 2015)** – adapts the Jacobson/Karels smoothing factors α and β based on the instantaneous rate of RTT change.
2. **Proposed algorithm** – extends the paper algorithm with:
   - A *windowed trend* \(k_{\mathrm{trend}}\) over the last \(W\) RTT samples (default \(W=2\))
   - A *loss‑aware scaling factor* \(\gamma\) (default \(\gamma=0.25\))

The project validates the paper algorithm against its published results and then compares the proposed algorithm against the paper algorithm in both wired (dumbbell) and wireless (802.11g Ad‑Hoc) topologies.

---

## Repository Structure

```
.
├── src/improved-rto/               # Proposed RTO estimator module
│   └── model/
│       ├── rtt-proposed.h
│       └── rtt-proposed.cc
├── contrib/improved-rto2/          # Paper algorithm module (AMEII 2015)
│   └── model/
│       ├── rtt-improved-estimator.h
│       └── rtt-improved-estimator.cc
├── scratch/online/                 # Online reproduction scripts
│   └── paper-rto-dual.cc
├── scratch/proposed/               # Window size sweep (W=1..4)
│   └── paper-rto-proposed.cc
├── scratch/submission_checklist/
│   ├── wired/
│   │   └── wired-sim.cc            # Full wired parameter sweep
│   └── wireless/
│       └── wireless-sim.cc         # Full wireless parameter sweep
├── run_all.sh                      # Orchestrates all simulation runs
└── results/                        # CSV output files (auto‑generated)
```

---

## Requirements

- **ns-3.45** (or later, with minimal changes)
- C++17 compatible compiler (g++ or clang++)
- CMake (≥ 3.10)
- Python 3 (for the `run_all.sh` helper and result analysis)

The project adds two new modules to ns-3:  
`src/improved-rto/` (proposed estimator) and `contrib/improved-rto2/` (paper estimator).

---

## Building

1. **Clone ns-3.45** and copy the project files into the source tree:
   ```bash
   git clone https://gitlab.com/nsnam/ns-3-dev.git ns-3.45
   cd ns-3.45
   # Copy src/improved-rto/ and contrib/improved-rto2/ into the tree
   # Copy scratch scripts into scratch/
   ```

2. **Configure with examples and tests** (optional but recommended):
   ```bash
   ./ns3 configure --enable-examples --enable-tests
   ```

3. **Build the new modules and scripts**:
   ```bash
   ./ns3 build
   ```

---

## Running Simulations

All simulation scripts accept command‑line arguments to sweep parameters. The main comparison scripts are:

### 1. Online reproduction (paper algorithm validation)

```bash
./ns3 run scratch/online/paper-rto-dual.cc
```
Generates RTO vs. RTT plots for increasing/decreasing load phases (9 flows, 180s). Matches Figure 2 of the AMEII 2015 paper.

### 2. Window size sweep (W = 1..4)

```bash
./ns3 run scratch/proposed/paper-rto-proposed.cc
```
Produces RTO envelopes for different window sizes (Figures 3.2–3.4).

### 3. Full wired parameter sweep

```bash
./ns3 run scratch/submission_checklist/wired/wired-sim.cc -- --algo=paper   # or proposed
```
Additional parameters (example):
```bash
--nodes=60 --flows=30 --pps=300 --simTime=60
```

### 4. Full wireless parameter sweep

```bash
./ns3 run scratch/submission_checklist/wireless/wireless-sim.cc -- --algo=paper --coverage=750
```
Wireless‑specific parameters: `--coverage`, `--simTime=60`.

### 5. Automated sweep (all combinations)

```bash
chmod +x run_all.sh
./run_all.sh
```
Runs every combination of parameters listed in the report (nodes, flows, pps, coverage) for both algorithms, appends results to CSV files in `results/`.

---

## Key Parameters

| Parameter          | Wired default | Wireless default | Swept values                     |
|--------------------|---------------|------------------|----------------------------------|
| Number of nodes    | 60            | 60               | 20, 40, 60, 80, 100              |
| Number of flows    | 30            | 30               | 10, 20, 30, 40, 50               |
| Packets per second | 300           | 300              | 100, 200, 300, 400, 500          |
| Coverage side      | N/A           | 750 m            | 250, 500, 750, 1000, 1250 m      |
| Algorithm          | paper/proposed| paper/proposed   | –                                |
| Simulation time    | 60 s          | 60 s             | (fixed after post‑evaluation)    |

**Measured metrics:**  
- Throughput (Mbps)  
- End‑to‑end delay (ms)  
- Packet Delivery Ratio (PDR)  
- Packet drop ratio  
- (Wireless only) Total energy consumed (J)

---

## Results Summary

### Wired topology
- Throughput and delay behave as expected, with small differences between the two algorithms.
- The proposed algorithm shows slight throughput improvements at higher flow counts (e.g., 50 flows: 3.83 vs. 3.71 Mbps).
- Overall means: Throughput 2.35 Mbps (proposed) vs. 2.36 Mbps (paper); delay ~12.1 ms for both.

### Wireless topology
- The proposed algorithm consistently achieves higher throughput and lower delay across many configurations.
- Example: 20 nodes, 30 flows – throughput 0.435 vs. 0.422 Mbps; delay 148.1 vs. 141.6 ms.
- Energy consumption remains nearly identical (difference < 1 J over 60 s).

Detailed graphs and tables are available in the project report.

---

## Important Notes

- **Simulation time** was increased from 5 s (initial evaluation) to 60 s to allow RTO differences to become visible. Longer runs (>60 s) exhausted memory on the test machine.
- **Wireless simulations use UDP** instead of TCP because AODV route discovery could not complete in time for dense TCP scenarios, triggering ns-3 assertions. UDP still measures network performance without RTO effects – a known limitation noted in the report.
- The proposed algorithm’s best configuration is **\(W=2\), \(\gamma=0.25\)**. These defaults are hard‑coded in the scripts when `--algo=proposed` is selected.

---

## Troubleshooting

| Issue                          | Likely solution                                                                 |
|--------------------------------|---------------------------------------------------------------------------------|
| Build errors about missing headers | Check that `src/improved-rto/model/rtt-proposed.h` is correctly included in `CMakeLists.txt`. |
| Memory exhausted during sweep  | Reduce simulation time to 30 s, or run fewer parameter combinations at once.    |

---

## License

This project is released under the same terms as ns-3: **GPL‑2.0‑only**. See the `LICENSE` file in the ns-3 distribution.

---

## Acknowledgments

- Xiao Jianliang and Zhang Kun for the original AMEII 2015 paper.
- The ns-3 development community for the simulator and documentation.
- Supervisor Ishrat Jahan Maam for guidance and feedback.

---

## Contact

Dhruba Kishore Nag – [2105062@cse.buet.ac.bd]  
Department of CSE, Bangladesh University of Engineering and Technology (BUET), Dhaka, Bangladesh.

For questions or to report issues with the code, please open an issue in the project repository (if public) or contact the author directly.
