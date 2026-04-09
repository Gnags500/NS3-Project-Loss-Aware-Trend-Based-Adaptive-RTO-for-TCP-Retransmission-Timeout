#!/usr/bin/env python3


import csv
import os
import sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from scipy.interpolate import make_interp_spline

script_dir = os.path.dirname(os.path.abspath(__file__))
root_dir = os.path.dirname(os.path.dirname(script_dir))
os.chdir(root_dir)

GRAPHS_DIR = os.path.join(script_dir, 'graphs')
os.makedirs(GRAPHS_DIR, exist_ok=True)


def load_csv(fname):
    """Load a CSV with columns: seq,rtt_ms,rto_trad_ms,rto_impr_ms,gamma_trad,gamma_impr."""
    data = {'seq': [], 'rtt': [], 'rto_trad': [], 'rto_impr': [],
            'gamma_trad': [], 'gamma_impr': []}
    if not os.path.exists(fname):
        print(f"[ERROR] {fname} not found. Run the simulation first:")
        print("  ./ns3 run paper-rto-dual")
        sys.exit(1)
    with open(fname, newline='') as f:
        for r in csv.DictReader(f):
            data['seq'].append(int(r['seq']))
            data['rtt'].append(float(r['rtt_ms']))
            data['rto_trad'].append(float(r['rto_trad_ms']))
            data['rto_impr'].append(float(r['rto_impr_ms']))
            data['gamma_trad'].append(float(r['gamma_trad']))
            data['gamma_impr'].append(float(r['gamma_impr']))
    return data


def smooth(x, y, num=200):
    """Cubic B-spline interpolation for visual smoothing."""
    xa = np.array(x, dtype=float)
    ya = np.array(y, dtype=float)
    if len(xa) < 4:
        return xa, ya
    xs = np.linspace(xa.min(), xa.max(), num)
    spl = make_interp_spline(xa, ya, k=3)
    return xs, spl(xs)


def plot_figure(data, title, y_max, y_step, out_png):
    """
    Plot RTT, RTO (traditional), RTO' (improved) on one graph.
    data: dict with keys seq, rtt, rto_trad, rto_impr
    """
    x     = data['seq']
    rtt   = data['rtt']
    rto_t = data['rto_trad']
    rto_i = data['rto_impr']

    fig, ax = plt.subplots(figsize=(9, 5.5))

    xs, rtt_s   = smooth(x, rtt)
    xs, rto_ts  = smooth(x, rto_t)
    xs, rto_is  = smooth(x, rto_i)

    ax.plot(xs, rtt_s,  color='#2020A0', linewidth=2.2, label='RTT')
    ax.plot(xs, rto_ts, color='#FF00FF', linewidth=2.2, label='RTO (Traditional)')
    ax.plot(xs, rto_is, color='#DAA520', linewidth=2.2, label="RTO' (Improved)")

    ax.plot(x, rtt,   'o', color='#2020A0', markersize=3, zorder=5)
    ax.plot(x, rto_t, 's', color='#FF00FF', markersize=3, zorder=5)
    ax.plot(x, rto_i, '^', color='#DAA520', markersize=3, zorder=5)

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
    ax.set_title(title, fontsize=13, fontweight='bold')
    ax.legend(fontsize=10, loc='best')

    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.spines['left'].set_color('#888888')
    ax.spines['bottom'].set_color('#888888')
    ax.tick_params(colors='#333333', labelsize=9)

    plt.tight_layout()
    plt.savefig(out_png, dpi=150, bbox_inches='tight', facecolor=fig.get_facecolor())
    plt.close(fig)
    print(f"[OK] Saved: {out_png}")


def plot_gamma_table(inc, dec, out_png):
    """Render a publication-style table image of gamma values."""
    n = min(len(inc['gamma_trad']), len(dec['gamma_trad']))

    avg = lambda lst: np.mean(lst[:n])

    col_labels = [
        'Seq',
        'RTT Rapid Increase\nTraditional (γ)',
        'RTT Rapid Increase\nImproved (γ\')',
        'RTT Rapid Decrease\nTraditional (γ)',
        'RTT Rapid Decrease\nImproved (γ\')',
    ]

    cell_data = []
    for i in range(n):
        cell_data.append([
            str(i + 1),
            f"{inc['gamma_trad'][i]:.2f}%",
            f"{inc['gamma_impr'][i]:.2f}%",
            f"{dec['gamma_trad'][i]:.2f}%",
            f"{dec['gamma_impr'][i]:.2f}%",
        ])
    
    cell_data.append([
        'Avg',
        f"{avg(inc['gamma_trad']):.2f}%",
        f"{avg(inc['gamma_impr']):.2f}%",
        f"{avg(dec['gamma_trad']):.2f}%",
        f"{avg(dec['gamma_impr']):.2f}%",
    ])

    n_rows = len(cell_data)
    n_cols = len(col_labels)

    fig_h = 0.45 * n_rows + 1.4
    fig, ax = plt.subplots(figsize=(13, fig_h))
    ax.axis('off')

    tbl = ax.table(
        cellText=cell_data,
        colLabels=col_labels,
        loc='center',
        cellLoc='center',
    )
    tbl.auto_set_font_size(False)
    tbl.set_fontsize(9)
    tbl.scale(1, 1.6)

    header_colors = ['#4A4A8A', '#7B2C8B', '#7B2C8B', '#1A6B3C', '#1A6B3C']
    for col, color in enumerate(header_colors):
        cell = tbl[0, col]
        cell.set_facecolor(color)
        cell.set_text_props(color='white', fontweight='bold', fontsize=8.5)

    for row in range(1, n_rows + 1):
        is_avg = (row == n_rows)
        for col in range(n_cols):
            cell = tbl[row, col]
            if is_avg:
                cell.set_facecolor('#D4EDDA')
                cell.set_text_props(fontweight='bold')
            elif row % 2 == 0:
                cell.set_facecolor('#F0F0F8')
            else:
                cell.set_facecolor('#FFFFFF')
            cell.set_edgecolor('#CCCCCC')

    fig.patch.set_facecolor('#F7F7F7')
    ax.set_title(
        'Table 1: The Percentage Gamma (γ) of RTT Variety',
        fontsize=12, fontweight='bold', pad=14, color='#222222'
    )

    plt.tight_layout()
    plt.savefig(out_png, dpi=150, bbox_inches='tight', facecolor=fig.get_facecolor())
    plt.close(fig)
    print(f"[OK] Saved: {out_png}")


def print_and_save_table(inc, dec, csv_out):
    """Print and save gamma comparison table."""
    n = min(len(inc['gamma_trad']), len(dec['gamma_trad']))

    print(f"\n{'='*90}")
    print("Table 1: The percentage gamma (γ) of RTT variety")
    print(f"{'='*90}")
    print(f"{'Seq':>6} | {'RTT Rapid Increase':^36} | {'RTT Rapid Decrease':^36}")
    print(f"{'':>6} | {'Traditional':>17}  {'Improved':>17} | {'Traditional':>17}  {'Improved':>17}")
    print("-" * 90)

    rows = []
    for i in range(n):
        gt_i = inc['gamma_trad'][i]
        gi_i = inc['gamma_impr'][i]
        gt_d = dec['gamma_trad'][i]
        gi_d = dec['gamma_impr'][i]
        print(f"{i+1:>6} | {gt_i:>16.2f}%  {gi_i:>16.2f}% | {gt_d:>16.2f}%  {gi_d:>16.2f}%")
        rows.append((i + 1, gt_i, gi_i, gt_d, gi_d))

    print(f"{'='*90}")

    # Averages
    avg_fn = lambda lst: np.mean(lst[:n])
    print(f"{'Avg':>6} | {avg_fn(inc['gamma_trad']):>16.2f}%  {avg_fn(inc['gamma_impr']):>16.2f}% "
          f"| {avg_fn(dec['gamma_trad']):>16.2f}%  {avg_fn(dec['gamma_impr']):>16.2f}%")
    print(f"{'='*90}")

    with open(csv_out, 'w') as f:
        f.write("seq,inc_gamma_trad,inc_gamma_impr,dec_gamma_trad,dec_gamma_impr\n")
        for r in rows:
            f.write(f"{r[0]},{r[1]:.2f},{r[2]:.2f},{r[3]:.2f},{r[4]:.2f}\n")
    print(f"[OK] Saved: {csv_out}")


# ── Main ──────────────────────────────────────────────────────────────────────
def main():
    print("=" * 60)
    print("Paper RTO Reproduction — Online Dual-Estimator Plots")
    print("=" * 60)

    inc = load_csv('paper-dual-increase-trad.csv')
    dec = load_csv('paper-dual-decrease-trad.csv')

    print(f"  Increase: {len(inc['seq'])} pts")
    print(f"  Decrease: {len(dec['seq'])} pts")

    # Figure 2
    print("\nGenerating Figure 2 (RTT rapid increase)...")
    plot_figure(inc,
                'Figure 2: RTT Rapid Increase Situation',
                y_max=400, y_step=50,
                out_png=os.path.join(GRAPHS_DIR, 'paper-dual-fig2.png'))

    # Figure 3
    print("\nGenerating Figure 3 (RTT rapid decrease)...")
    plot_figure(dec,
                'Figure 3: RTT Rapid Decrease Situation',
                y_max=700, y_step=100,
                out_png=os.path.join(GRAPHS_DIR, 'paper-dual-fig3.png'))

    
    print_and_save_table(inc, dec, os.path.join(GRAPHS_DIR, 'paper-dual-table1.csv'))

    print("\nGenerating Table 1 image...")
    plot_gamma_table(inc, dec,
                     out_png=os.path.join(GRAPHS_DIR, 'paper-dual-table1.png'))

    print("\nDone! Output files:")
    print("  paper-dual-fig2.png    (Figure 2)")
    print("  paper-dual-fig3.png    (Figure 3)")
    print("  paper-dual-table1.png  (Table 1 image)")
    print("  paper-dual-table1.csv  (Table 1 data)")


if __name__ == '__main__':
    main()
