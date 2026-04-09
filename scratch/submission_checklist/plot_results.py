#!/usr/bin/env python3

import os
import sys
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# ── Paths ─────────────────────────────────────────────────────────────────────
SCRIPT_DIR   = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR     = os.path.dirname(os.path.dirname(SCRIPT_DIR))   # ns-3 root
CSV_DIR      = os.path.join(ROOT_DIR, "report", "csv")
GRAPHS_DIR   = os.path.join(SCRIPT_DIR, "graphs")
os.makedirs(GRAPHS_DIR, exist_ok=True)

WIRED_CSV    = os.path.join(CSV_DIR, "wired-results.csv")
WIRELESS_CSV = os.path.join(CSV_DIR, "wireless-results.csv")

# ── Plot style ────────────────────────────────────────────────────────────────
COLORS  = {"paper": "#e53935", "proposed": "#1e88e5"}
MARKERS = {"paper": "o",       "proposed": "s"}
LABELS  = {"paper": "Paper Alg. (AMEII 2015)",
           "proposed": "Proposed (Loss-Aware Trend)"}

WIRED_METRICS    = [
    ("throughput_mbps", "Throughput (Mbps)",     "higher is better"),
    ("delay_ms",        "End-to-End Delay (ms)", "lower is better"),
    ("pdr",             "Packet Delivery Ratio",  "higher is better"),
    ("drop_ratio",      "Packet Drop Ratio",      "lower is better"),
]
WIRELESS_METRICS = WIRED_METRICS + [
    ("energy_j", "Energy Consumed (J)", "lower is better"),
]

WIRED_PARAMS    = {"nodes": "Number of Nodes",
                   "flows": "Number of Flows",
                   "pps":   "Packets per Second"}
WIRELESS_PARAMS = {**WIRED_PARAMS, "coverage_m": "Coverage Area Side (m)"}


# ── Helpers ───────────────────────────────────────────────────────────────────
def load_csv(path: str) -> pd.DataFrame | None:
    if not os.path.exists(path):
        print(f"  [SKIP] {path} not found.")
        return None
    df = pd.read_csv(path)
    print(f"  Loaded {len(df)} rows from {os.path.basename(path)}")
    return df


def save_fig(fig, filename: str):
    path = os.path.join(GRAPHS_DIR, filename)
    fig.savefig(path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"  [OK] {path}")


def plot_metric_vs_param(df: pd.DataFrame, param_col: str, param_label: str,
                         metric_col: str, metric_label: str, note: str,
                         title_prefix: str, out_filename: str):
    """
    Line graph: metric_col vs param_col, one line per algo.
    Aggregates (mean) over runs that share the same (algo, param_col) values.
    """
    fig, ax = plt.subplots(figsize=(7, 4.5))

    for algo in ["paper", "proposed"]:
        sub = df[df["algo"] == algo].copy()
        if sub.empty:
            continue
        grp = sub.groupby(param_col)[metric_col].mean().reset_index()
        grp.sort_values(param_col, inplace=True)
        ax.plot(grp[param_col], grp[metric_col],
                color=COLORS[algo],
                marker=MARKERS[algo],
                linewidth=2,
                markersize=7,
                label=LABELS[algo])

    ax.set_xlabel(param_label, fontsize=12)
    ax.set_ylabel(metric_label, fontsize=12)
    ax.set_title(f"{title_prefix}\n{metric_label} vs {param_label}", fontsize=12)
    ax.legend(fontsize=10)
    ax.grid(True, linestyle="--", alpha=0.5)
    ax.annotate(f"({note})", xy=(0.01, 0.01), xycoords="axes fraction",
                fontsize=8, color="grey")
    fig.tight_layout()
    save_fig(fig, out_filename)


def plot_bar_comparison(df: pd.DataFrame, metric_col: str, metric_label: str,
                        title_prefix: str, out_filename: str):
    """
    Single grouped-bar chart showing mean metric for paper vs proposed
    across all runs (overall comparison).
    """
    means = df.groupby("algo")[metric_col].mean()
    fig, ax = plt.subplots(figsize=(5, 4))
    bars = ax.bar(
        [LABELS.get(a, a) for a in means.index],
        means.values,
        color=[COLORS.get(a, "grey") for a in means.index],
        edgecolor="black",
        width=0.5
    )
    for bar, val in zip(bars, means.values):
        ax.text(bar.get_x() + bar.get_width() / 2.0, bar.get_height() + 0.01 * abs(bar.get_height()),
                f"{val:.3f}", ha="center", va="bottom", fontsize=9, fontweight="bold")
    ax.set_ylabel(metric_label, fontsize=11)
    ax.set_title(f"{title_prefix}\nOverall Mean: {metric_label}", fontsize=11)
    ax.tick_params(axis="x", labelsize=9)
    ax.grid(axis="y", linestyle="--", alpha=0.4)
    fig.tight_layout()
    save_fig(fig, out_filename)


def generate_summary_table(df: pd.DataFrame, metrics: list, title: str, out_filename: str):
    """
    Table image: algo × metric with mean values; proposed Δ vs paper highlighted.
    """
    rows = []
    col_names = ["Metric"] + [LABELS[a] for a in ["paper", "proposed"]] + ["Δ (Proposed − Paper)"]
    for metric_col, metric_label, note in metrics:
        if metric_col not in df.columns:
            continue
        means = df.groupby("algo")[metric_col].mean()
        v_paper    = means.get("paper",    float("nan"))
        v_proposed = means.get("proposed", float("nan"))
        delta      = v_proposed - v_paper
        rows.append([metric_label,
                     f"{v_paper:.4f}",
                     f"{v_proposed:.4f}",
                     f"{delta:+.4f}"])

    if not rows:
        return

    n_rows = len(rows)
    fig, ax = plt.subplots(figsize=(11, 1.0 + n_rows * 0.55))
    ax.axis("off")
    tbl = ax.table(cellText=rows,
                   colLabels=col_names,
                   loc="center",
                   cellLoc="center")
    tbl.auto_set_font_size(False)
    tbl.set_fontsize(10)
    tbl.scale(1.2, 1.6)

    # Header style
    for j in range(len(col_names)):
        tbl[(0, j)].set_facecolor("#2c3e50")
        tbl[(0, j)].set_text_props(color="white", fontweight="bold")

    # Row colouring: green if Δ favours proposed, red if not
    # For throughput/PDR: positive Δ is good. For delay/drop/energy: negative Δ is good.
    good_if_positive = {"Throughput (Mbps)", "Packet Delivery Ratio"}
    for i, (metric_col, metric_label, _) in enumerate(metrics):
        if metric_col not in df.columns:
            continue
        delta_val = float(rows[i][3])
        good = (delta_val > 0) if (metric_label in good_if_positive) else (delta_val < 0)
        color = "#c8e6c9" if good else "#ffcdd2"  # light green / light red
        for j in range(len(col_names)):
            tbl[(i + 1, j)].set_facecolor(color)

    fig.suptitle(title, fontsize=12, fontweight="bold", y=0.98)
    fig.tight_layout()
    save_fig(fig, out_filename)


# ── Main ──────────────────────────────────────────────────────────────────────
def main():
    print("=" * 60)
    print("Generating comparison plots: Paper vs Proposed Algorithm")
    print("=" * 60)

    wired_df    = load_csv(WIRED_CSV)
    wireless_df = load_csv(WIRELESS_CSV)

    # ── Wired plots ───────────────────────────────────────────────────────────
    if wired_df is not None and not wired_df.empty:
        print("\n--- Wired topology graphs ---")
        prefix = "Wired Dumbbell Topology"

        for param_col, param_label in WIRED_PARAMS.items():
            # Only plot if this parameter was varied (i.e., has >1 distinct value)
            if wired_df[param_col].nunique() < 2:
                continue
            for metric_col, metric_label, note in WIRED_METRICS:
                fname = f"wired-{param_col}-vs-{metric_col}.png"
                plot_metric_vs_param(wired_df, param_col, param_label,
                                     metric_col, metric_label, note,
                                     prefix, fname)

        # Bar charts: overall comparison per metric
        for metric_col, metric_label, note in WIRED_METRICS:
            plot_bar_comparison(wired_df, metric_col, metric_label,
                                prefix, f"wired-bar-{metric_col}.png")

        # Summary table
        generate_summary_table(wired_df, WIRED_METRICS,
                                "Wired: Paper vs Proposed — Mean Metrics",
                                "wired-summary-table.png")

    # ── Wireless plots ────────────────────────────────────────────────────────
    if wireless_df is not None and not wireless_df.empty:
        print("\n--- Wireless 802.11g topology graphs ---")
        prefix = "Wireless 802.11g Static (AdHoc)"

        for param_col, param_label in WIRELESS_PARAMS.items():
            if wireless_df[param_col].nunique() < 2:
                continue
            for metric_col, metric_label, note in WIRELESS_METRICS:
                if metric_col not in wireless_df.columns:
                    continue
                fname = f"wireless-{param_col}-vs-{metric_col}.png"
                plot_metric_vs_param(wireless_df, param_col, param_label,
                                     metric_col, metric_label, note,
                                     prefix, fname)

        for metric_col, metric_label, note in WIRELESS_METRICS:
            if metric_col not in wireless_df.columns:
                continue
            plot_bar_comparison(wireless_df, metric_col, metric_label,
                                prefix, f"wireless-bar-{metric_col}.png")

        generate_summary_table(wireless_df, WIRELESS_METRICS,
                                "Wireless 802.11g: Paper vs Proposed — Mean Metrics",
                                "wireless-summary-table.png")

    print("\n" + "=" * 60)
    print(f"All graphs saved to: {GRAPHS_DIR}")
    print("=" * 60)


if __name__ == "__main__":
    main()
