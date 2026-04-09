#!/usr/bin/env python3
"""
paper-rto-proposed-plot.py

Reads all 40 CSV files produced by paper-rto-proposed.cc (multi-config sweep)
and generates:

  Figures 2a-2d : One per window size W, increase phase
                  (RTT + Paper + 5 gamma variants of Proposed)
  Figures 3a-3d : Same for decrease phase
  Figure 4      : Grouped bar chart — avg γ by window size and gamma
  Table 1       : PNG table of avg γ for all 20 configs + paper

All outputs saved to scratch/proposed/graphs/.

Prerequisites:  pip3 install numpy matplotlib scipy
"""

import csv
import os
import sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from scipy.interpolate import make_interp_spline

script_dir = os.path.dirname(os.path.abspath(__file__))
root_dir   = os.path.dirname(os.path.dirname(script_dir))
os.chdir(root_dir)

GRAPHS_DIR = os.path.join(script_dir, 'graphs')
os.makedirs(GRAPHS_DIR, exist_ok=True)

# ── Config space ───────────────────────────────────────────────────────────────
WINDOW_SIZES = [1, 2, 3, 4]
GAMMA_VALUES = [0.0, 0.25, 0.5, 0.75, 1.0]

# Visual style
GAMMA_COLORS = {
    0.00: '#888888',   # grey
    0.25: '#00AAFF',   # sky blue
    0.50: '#FF8C00',   # orange
    0.75: '#E83030',   # red
    1.00: '#DAA520',   # gold
}
PAPER_COLOR = '#FF00FF'   # magenta
RTT_COLOR   = '#1C1C8A'   # dark navy

GAMMA_LABELS = {
    0.00: 'Proposed γ=0.00 (no loss)',
    0.25: 'Proposed γ=0.25',
    0.50: 'Proposed γ=0.50',
    0.75: 'Proposed γ=0.75',
    1.00: 'Proposed γ=1.00',
}


# ── CSV loader ─────────────────────────────────────────────────────────────────
def load_csv(fname):
    if not os.path.exists(fname):
        print(f"[ERROR] {fname} not found. Run ./ns3 run scratch/proposed/paper-rto-proposed first.")
        sys.exit(1)
    data = {'seq': [], 'rtt': [], 'rto_paper': [], 'rto_prop': [],
            'gamma_paper': [], 'gamma_prop': []}
    with open(fname, newline='') as f:
        for r in csv.DictReader(f):
            data['seq'].append(int(r['seq']))
            data['rtt'].append(float(r['rtt_ms']))
            data['rto_paper'].append(float(r['rto_paper_ms']))
            data['rto_prop'].append(float(r['rto_prop_ms']))
            data['gamma_paper'].append(float(r['gamma_paper']))
            data['gamma_prop'].append(float(r['gamma_prop']))
    return data


def load_all():
    """Return data[w][g_idx]['inc'/'dec']."""
    data = {}
    for w in WINDOW_SIZES:
        data[w] = {}
        for g in GAMMA_VALUES:
            inc = load_csv(f'prop-W{w}-G{g:.2f}-inc.csv')
            dec = load_csv(f'prop-W{w}-G{g:.2f}-dec.csv')
            data[w][g] = {'inc': inc, 'dec': dec}
    return data


# ── Smooth helper ──────────────────────────────────────────────────────────────
def smooth(x, y, num=200):
    xa, ya = np.array(x, dtype=float), np.array(y, dtype=float)
    if len(xa) < 4:
        return xa, ya
    xs = np.linspace(xa.min(), xa.max(), num)
    return xs, make_interp_spline(xa, ya, k=3)(xs)


# ── Per-window line figure (one phase) ────────────────────────────────────────
def plot_window_figure(data, w, phase_key, title, y_max, y_step, out_png):
    """
    One figure for a given W showing RTT, Paper, and all 5 γ variants.
    phase_key: 'inc' or 'dec'
    """
    # All gamma configs share the same RTT and paper RTO (use G=0 as reference)
    ref = data[w][0.0][phase_key]
    x       = ref['seq']
    rtt     = ref['rtt']
    rto_pap = ref['rto_paper']

    fig, ax = plt.subplots(figsize=(10, 5.5))

    # RTT
    xs, ys = smooth(x, rtt)
    ax.plot(xs, ys, color=RTT_COLOR, linewidth=2.5, label='RTT')
    ax.plot(x, rtt, 'o', color=RTT_COLOR, markersize=3, zorder=5)

    # Paper
    xs, ys = smooth(x, rto_pap)
    ax.plot(xs, ys, color=PAPER_COLOR, linewidth=2.2, linestyle='--',
            label='RTO (Paper Alg.)')
    ax.plot(x, rto_pap, 's', color=PAPER_COLOR, markersize=3, zorder=5)

    # Each gamma variant
    for g in GAMMA_VALUES:
        d = data[w][g][phase_key]
        col = GAMMA_COLORS[g]
        xs, ys = smooth(x, d['rto_prop'])
        ax.plot(xs, ys, color=col, linewidth=1.8, label=GAMMA_LABELS[g])
        ax.plot(x, d['rto_prop'], '^', color=col, markersize=2.5, zorder=4)

    ax.set_facecolor('#C8C8C8')
    fig.set_facecolor('#C8C8C8')
    ax.grid(True, axis='y', color='white', linewidth=0.7)
    ax.grid(False, axis='x')
    ax.set_ylim(0, y_max)
    ax.set_yticks(range(0, y_max + 1, y_step))
    ax.set_xlim(0.5, max(x) + 0.5)
    ax.set_xticks(x)
    ax.set_xlabel('Sampling Sequence (times)', fontsize=11)
    ax.set_ylabel('ms', fontsize=11)
    ax.set_title(title, fontsize=12, fontweight='bold')
    ax.legend(fontsize=8.5, loc='upper left', ncol=2)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.tick_params(colors='#333333', labelsize=9)

    plt.tight_layout()
    plt.savefig(out_png, dpi=150, bbox_inches='tight', facecolor=fig.get_facecolor())
    plt.close(fig)
    print(f"[OK] Saved: {out_png}")


# ── Summary bar chart ──────────────────────────────────────────────────────────
def plot_bar_summary(data, out_png):
    """
    Grouped bar chart: avg γ per (W, γ) config for increase and decrease phases.
    Two panels side by side.
    """
    n_w = len(WINDOW_SIZES)
    n_g = len(GAMMA_VALUES)

    # Compute avg gamma for each config
    inc_paper_avg = []  # one value per W (paper is same for all γ given same W)
    dec_paper_avg = []
    inc_prop_avg  = {g: [] for g in GAMMA_VALUES}  # list over W
    dec_prop_avg  = {g: [] for g in GAMMA_VALUES}

    for w in WINDOW_SIZES:
        # Paper is same regardless of gamma; pick G=0 as representative
        ref = data[w][0.0]
        inc_paper_avg.append(np.mean(ref['inc']['gamma_paper']))
        dec_paper_avg.append(np.mean(ref['dec']['gamma_paper']))
        for g in GAMMA_VALUES:
            inc_prop_avg[g].append(np.mean(data[w][g]['inc']['gamma_prop']))
            dec_prop_avg[g].append(np.mean(data[w][g]['dec']['gamma_prop']))

    x_base = np.arange(n_w)
    total_bars = n_g + 1  # 5 gamma + 1 paper
    bar_w = 0.8 / total_bars

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5.5), sharey=False)

    for ax, paper_vals, prop_vals, phase_title in [
        (ax1, inc_paper_avg, inc_prop_avg, 'RTT Rapid Increase — Avg γ'),
        (ax2, dec_paper_avg, dec_prop_avg, 'RTT Rapid Decrease — Avg γ'),
    ]:
        # Paper bars
        offsets = np.linspace(-(total_bars - 1) / 2, (total_bars - 1) / 2, total_bars) * bar_w
        ax.bar(x_base + offsets[0], paper_vals, bar_w, color=PAPER_COLOR,
               label='Paper Alg.', alpha=0.9, edgecolor='white', linewidth=0.5)

        for idx, g in enumerate(GAMMA_VALUES):
            ax.bar(x_base + offsets[idx + 1], prop_vals[g], bar_w,
                   color=GAMMA_COLORS[g],
                   label=f'γ={g:.2f}', alpha=0.9, edgecolor='white', linewidth=0.5)

        ax.set_xticks(x_base)
        ax.set_xticklabels([f'W={w}' for w in WINDOW_SIZES], fontsize=10)
        ax.set_ylabel('Average γ (%)', fontsize=10)
        ax.set_title(phase_title, fontsize=11, fontweight='bold')
        ax.legend(fontsize=8, ncol=2)
        ax.grid(True, axis='y', alpha=0.4)
        ax.set_facecolor('#F5F5F5')

    fig.suptitle('Paper Algorithm vs Proposed — Average Gamma by Window & Loss Factor',
                 fontsize=12, fontweight='bold', y=1.01)
    plt.tight_layout()
    plt.savefig(out_png, dpi=150, bbox_inches='tight')
    plt.close(fig)
    print(f"[OK] Saved: {out_png}")


# ── Summary table PNG ──────────────────────────────────────────────────────────
def plot_summary_table(data, out_png):
    """
    Render a table: rows = 20 configs + paper, cols = inc avg γ, dec avg γ,
    improvement vs paper (inc), improvement vs paper (dec).
    """
    # Paper reference (use W=1, G=0 since paper is independent of proposed config)
    paper_inc_avg = np.mean(data[1][0.0]['inc']['gamma_paper'])
    paper_dec_avg = np.mean(data[1][0.0]['dec']['gamma_paper'])

    col_labels = ['Config (W, γ)', 'Inc avg γ', 'Dec avg γ',
                  'Δ vs Paper (Inc)', 'Δ vs Paper (Dec)']

    rows = []
    header_color = '#3A3A7A'
    row_colors = []

    # Paper row
    rows.append(['Paper Alg. (baseline)',
                 f'{paper_inc_avg:.2f}%', f'{paper_dec_avg:.2f}%',
                 '—', '—'])
    row_colors.append(['#CCCCEE'] * 5)

    for w in WINDOW_SIZES:
        for g in GAMMA_VALUES:
            inc_avg = np.mean(data[w][g]['inc']['gamma_prop'])
            dec_avg = np.mean(data[w][g]['dec']['gamma_prop'])
            d_inc = inc_avg - paper_inc_avg
            d_dec = dec_avg - paper_dec_avg
            d_inc_str = f"{d_inc:+.2f}%"
            d_dec_str = f"{d_dec:+.2f}%"
            rows.append([f'W={w}, γ={g:.2f}',
                         f'{inc_avg:.2f}%', f'{dec_avg:.2f}%',
                         d_inc_str, d_dec_str])
            # Green if better (lower), red if worse
            ci = '#D4EDDA' if d_inc <= 0 else '#FADBD8'
            cd = '#D4EDDA' if d_dec <= 0 else '#FADBD8'
            row_colors.append(['#FFFFFF', '#FFFFFF', '#FFFFFF', ci, cd])

    n_rows = len(rows)
    fig_h = 0.38 * n_rows + 1.4
    fig, ax = plt.subplots(figsize=(12, fig_h))
    ax.axis('off')

    tbl = ax.table(cellText=rows, colLabels=col_labels,
                   loc='center', cellLoc='center')
    tbl.auto_set_font_size(False)
    tbl.set_fontsize(8.5)
    tbl.scale(1, 1.55)

    for col in range(len(col_labels)):
        cell = tbl[0, col]
        cell.set_facecolor(header_color)
        cell.set_text_props(color='white', fontweight='bold', fontsize=8.5)

    for row in range(1, n_rows + 1):
        for col in range(len(col_labels)):
            cell = tbl[row, col]
            cell.set_facecolor(row_colors[row - 1][col])
            cell.set_edgecolor('#CCCCCC')
            if row == 1:   # paper row bold
                cell.set_text_props(fontweight='bold')

    fig.patch.set_facecolor('#F7F7F7')
    ax.set_title(
        'Table: Average γ — Paper Algorithm vs Proposed Configurations\n'
        '(Green Δ = proposed outperforms paper, Red Δ = paper is better)',
        fontsize=10, fontweight='bold', pad=12, color='#222222'
    )
    plt.tight_layout()
    plt.savefig(out_png, dpi=150, bbox_inches='tight', facecolor=fig.get_facecolor())
    plt.close(fig)
    print(f"[OK] Saved: {out_png}")


# ── Main ───────────────────────────────────────────────────────────────────────
def main():
    print("=" * 65)
    print("Multi-Config Sweep — Paper vs Proposed: Plotting")
    print("=" * 65)
    print("Loading 40 CSV files...")
    data = load_all()
    print("  Done.\n")

    # ── Increase phase: one figure per window size ─────────────────────────
    for w in WINDOW_SIZES:
        print(f"Figure: W={w} increase phase...")
        plot_window_figure(
            data, w, 'inc',
            title=f'RTT Rapid Increase — Proposed W={w} (all γ) vs Paper',
            y_max=400, y_step=50,
            out_png=os.path.join(GRAPHS_DIR, f'prop-W{w}-increase.png')
        )

    # ── Decrease phase: one figure per window size ─────────────────────────
    for w in WINDOW_SIZES:
        print(f"Figure: W={w} decrease phase...")
        plot_window_figure(
            data, w, 'dec',
            title=f'RTT Rapid Decrease — Proposed W={w} (all γ) vs Paper',
            y_max=700, y_step=100,
            out_png=os.path.join(GRAPHS_DIR, f'prop-W{w}-decrease.png')
        )

    # ── Bar chart summary ──────────────────────────────────────────────────
    print("\nGenerating bar chart summary...")
    plot_bar_summary(data, os.path.join(GRAPHS_DIR, 'prop-bar-summary.png'))

    # ── Summary table ──────────────────────────────────────────────────────
    print("\nGenerating summary table...")
    plot_summary_table(data, os.path.join(GRAPHS_DIR, 'prop-summary-table.png'))

    print(f"\n{'='*65}")
    print("Done! All outputs in:", GRAPHS_DIR)
    print(f"{'='*65}")
    print("  prop-W{{1-4}}-increase.png  (4 files — increase phase per W)")
    print("  prop-W{{1-4}}-decrease.png  (4 files — decrease phase per W)")
    print("  prop-bar-summary.png        (avg γ bar chart)")
    print("  prop-summary-table.png      (full config comparison table)")


if __name__ == '__main__':
    main()
