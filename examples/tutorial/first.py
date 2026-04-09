#!/usr/bin/env python3
"""
Adaptive RTO — Algorithm Comparison Plotter
============================================
Reads two separate CSV files (one per algorithm) and produces
comparison plots + a text summary.

Usage:
    python3 plot_results.py results_proposed.csv results_paper.csv
"""

import sys
import csv
import os

# ─────────────────────────────────────────────
# LOADER
# ─────────────────────────────────────────────
def load_csv(filename):
    d = {'time':[], 'rtt':[], 'srtt':[], 'rttvar':[],
         'rto':[], 'loss':[], 'ktrend':[], 'tput':[], 'retx':[]}
    with open(filename) as f:
        for row in csv.DictReader(f):
            d['time'].append(float(row['time_s']))
            d['rtt'].append(float(row['measured_rtt_ms']))
            d['srtt'].append(float(row['srtt_ms']))
            d['rttvar'].append(float(row['rttvar_ms']))
            d['rto'].append(float(row['rto_ms']))
            d['loss'].append(float(row['loss_rate']))
            d['ktrend'].append(float(row['ktrend']))
            d['tput'].append(float(row.get('throughput_mbps', 0)))
            d['retx'].append(int(row.get('retransmissions', 0)))
    return d

# ─────────────────────────────────────────────
# METRICS
# ─────────────────────────────────────────────
def metrics(d):
    n   = len(d['time'])
    rtt = d['rtt']
    if n == 0:
        return {}
    dev = [abs(d['rto'][i] - rtt[i]) / rtt[i] * 100
           for i in range(n) if rtt[i] > 0]
    conv = next((i for i in range(n)
                 if rtt[i] > 0 and
                 abs(d['rto'][i]-rtt[i])/rtt[i] < 0.10), n)
    return {
        'n'        : n,
        'avg_rtt'  : sum(rtt) / n,
        'avg_srtt' : sum(d['srtt']) / n,
        'avg_rttvar': sum(d['rttvar']) / n,
        'avg_rto'  : sum(d['rto']) / n,
        'rto_dev'  : sum(dev) / len(dev) if dev else 0,
        'convergence': conv,
        'avg_tput' : sum(d['tput']) / n,
        'total_retx': d['retx'][-1] if d['retx'] else 0,
        'avg_loss' : sum(d['loss']) / n * 100,
    }

# ─────────────────────────────────────────────
# TEXT SUMMARY
# ─────────────────────────────────────────────
def print_summary(mp, mk):
    print("\n" + "═"*65)
    print("   ALGORITHM COMPARISON SUMMARY")
    print("═"*65)
    print(f"\n{'Metric':<35} {'Paper':>12} {'Proposed':>12}")
    print("─"*61)
    rows = [
        ('Samples',              'n',           ''),
        ('Avg Measured RTT (ms)','avg_rtt',     '.2f'),
        ('Avg SRTT (ms)',        'avg_srtt',    '.2f'),
        ('Avg RTTVAR (ms)',      'avg_rttvar',  '.2f'),
        ('Avg RTO (ms)',         'avg_rto',     '.2f'),
        ('RTO Dev from RTT (%)','rto_dev',      '.2f'),
        ('Convergence (sample)', 'convergence', ''),
        ('Avg Throughput (Mbps)','avg_tput',    '.4f'),
        ('Total Retransmissions','total_retx',  ''),
        ('Avg Loss Rate (%)',    'avg_loss',     '.3f'),
    ]
    for label, key, fmt in rows:
        pv = mp.get(key, 0)
        kv = mk.get(key, 0)
        if fmt:
            print(f"{label:<35} {pv:>12{fmt}} {kv:>12{fmt}}")
        else:
            print(f"{label:<35} {pv:>12} {kv:>12}")

    print("\n── Key Improvements (Proposed vs Paper) ──")
    dev_imp = mp['rto_dev'] - mk['rto_dev']
    rto_dif = mk['avg_rto'] - mp['avg_rto']
    tpt_dif = mk['avg_tput'] - mp['avg_tput']
    retx_dif= mp['total_retx'] - mk['total_retx']

    if dev_imp > 0:
        print(f"  ✔ RTO deviation REDUCED by {dev_imp:.2f}% → tracks RTT better")
    else:
        print(f"  ~ RTO deviation similar (Δ{abs(dev_imp):.2f}%)")

    if rto_dif > 0:
        print(f"  ✔ RTO {rto_dif:.2f}ms higher → loss-aware inflation prevents")
        print(f"    spurious retransmissions during lossy periods")
    else:
        print(f"  ~ RTO {abs(rto_dif):.2f}ms lower → more aggressive recovery")

    if tpt_dif > 0:
        print(f"  ✔ Throughput improved by {tpt_dif:.4f} Mbps")
    elif tpt_dif < 0:
        print(f"  ~ Throughput {abs(tpt_dif):.4f} Mbps lower (loss-conservative)")

    if retx_dif < 0:
        print(f"  ✔ {abs(retx_dif)} fewer retransmissions")
    elif retx_dif > 0:
        print(f"  ~ {retx_dif} more retransmissions (loss-aware delay)")

    print("═"*65)

# ─────────────────────────────────────────────
# PLOTS
# ─────────────────────────────────────────────
def plot(prop, paper, mp, mk):
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        import matplotlib.gridspec as gridspec
    except ImportError:
        print("[INFO] matplotlib unavailable — text summary only.")
        print_summary(mp, mk)
        return

    C_RTT  = '#2196F3'   # blue   — measured RTT
    C_PROP = '#E53935'   # red    — proposed
    C_PAPR = '#43A047'   # green  — paper
    C_LOSS = '#FF6F00'   # amber
    C_BG   = '#0F1117'
    C_PANL = '#1A1D27'
    C_GRID = '#2A2D3A'
    C_TEXT = '#E0E0E0'
    C_SUB  = '#9E9E9E'

    plt.rcParams.update({
        'figure.facecolor': C_BG,  'axes.facecolor': C_PANL,
        'axes.edgecolor':   C_GRID,'axes.labelcolor': C_SUB,
        'axes.titlecolor':  C_TEXT,'xtick.color': C_SUB,
        'ytick.color':      C_SUB, 'grid.color':  C_GRID,
        'text.color':       C_TEXT,'legend.facecolor': '#22253A',
        'legend.edgecolor': C_GRID,'font.family': 'monospace',
    })

    fig = plt.figure(figsize=(16, 14))
    fig.patch.set_facecolor(C_BG)
    fig.suptitle('Adaptive RTO — Proposed vs Paper Algorithm',
                 fontsize=14, fontweight='bold', color=C_TEXT, y=0.98)
    gs = gridspec.GridSpec(3, 2, figure=fig, hspace=0.52, wspace=0.38,
                           left=0.07, right=0.96, top=0.94, bottom=0.05)

    def sa(ax, title, xl, yl):
        ax.set_title(title, fontsize=9, pad=7)
        ax.set_xlabel(xl, fontsize=8)
        ax.set_ylabel(yl, fontsize=8)
        ax.grid(True, alpha=0.22, linewidth=0.5)
        ax.tick_params(labelsize=7)
        [s.set_edgecolor(C_GRID) for s in ax.spines.values()]

    tp = prop['time']
    tk = paper['time']

    # ── 1: RTT + SRTT ──
    ax = fig.add_subplot(gs[0, 0])
    ax.plot(tp, prop['rtt'],  color=C_RTT,  alpha=0.4, lw=1.2, label='RTT (proposed run)')
    ax.plot(tk, paper['rtt'], color=C_RTT,  alpha=0.25,lw=1.0, ls=':',label='RTT (paper run)')
    ax.plot(tp, prop['srtt'], color=C_PROP, lw=2.0, label='SRTT — Proposed')
    ax.plot(tk, paper['srtt'],color=C_PAPR, lw=1.8, ls='--', label='SRTT — Paper')
    sa(ax, 'Smoothed RTT Comparison', 'Time (s)', 'ms')
    ax.legend(fontsize=6.5)

    # ── 2: RTO comparison (KEY PLOT) ──
    ax = fig.add_subplot(gs[0, 1])
    ax.plot(tp, prop['rtt'],  color=C_RTT,  alpha=0.35, lw=1.2, label='Measured RTT')
    ax.plot(tp, prop['rto'],  color=C_PROP, lw=2.2, label='RTO — Proposed (loss-aware)')
    ax.plot(tk, paper['rto'], color=C_PAPR, lw=1.8, ls='--', label='RTO — Paper (trend-only)')
    sa(ax, 'RTO: Proposed vs Paper  ← KEY COMPARISON', 'Time (s)', 'RTO (ms)')
    ax.legend(fontsize=6.5)

    # ── 3: ktrend ──
    ax = fig.add_subplot(gs[1, 0])
    ax.plot(tp, prop['ktrend'],  color=C_PROP, lw=1.8, label='ktrend — Proposed (W=4)')
    ax.plot(tk, paper['ktrend'], color=C_PAPR, lw=1.5, ls='--', label='ktrend — Paper (W=2)')
    ax.axhline(0, color=C_TEXT, lw=0.7, ls=':')
    sa(ax, 'RTT Trend (ktrend) — Both Algorithms', 'Time (s)', 'ktrend')
    ax.legend(fontsize=6.5)

    # ── 4: Loss rate ──
    ax = fig.add_subplot(gs[1, 1])
    lp = [l*100 for l in prop['loss']]
    lk = [l*100 for l in paper['loss']]
    ax.fill_between(tp, lp, color=C_PROP, alpha=0.25)
    ax.plot(tp, lp, color=C_PROP, lw=1.8, label='Proposed run')
    ax.fill_between(tk, lk, color=C_PAPR, alpha=0.18)
    ax.plot(tk, lk, color=C_PAPR, lw=1.5, ls='--', label='Paper run')
    sa(ax, 'Packet Loss Rate over Time', 'Time (s)', 'Loss (%)')
    ax.legend(fontsize=6.5)

    # ── 5: Throughput ──
    ax = fig.add_subplot(gs[2, 0])
    ax.fill_between(tp, prop['tput'], color=C_PROP, alpha=0.25)
    ax.plot(tp, prop['tput'], color=C_PROP, lw=1.8, label='Proposed')
    ax.fill_between(tk, paper['tput'], color=C_PAPR, alpha=0.18)
    ax.plot(tk, paper['tput'],color=C_PAPR, lw=1.5, ls='--', label='Paper')
    sa(ax, 'Throughput over Time', 'Time (s)', 'Mbps')
    ax.legend(fontsize=6.5)

    # ── 6: Performance bar chart ──
    ax = fig.add_subplot(gs[2, 1])
    cats  = ['Avg RTO\n(ms)', 'RTO Dev\n(%)', 'Avg RTTVAR\n(ms)', 'Retx']
    pvals = [mk['avg_rto'], mk['rto_dev'], mk['avg_rttvar'], mk['total_retx']]
    kvals = [mp['avg_rto'], mp['rto_dev'], mp['avg_rttvar'], mp['total_retx']]
    x = range(len(cats))
    w = 0.35
    b1 = ax.bar([i-w/2 for i in x], pvals, w, color=C_PAPR, alpha=0.85, label='Paper')
    b2 = ax.bar([i+w/2 for i in x], kvals, w, color=C_PROP, alpha=0.85, label='Proposed')
    for b, c in [(b1, C_PAPR), (b2, C_PROP)]:
        for bar in b:
            ax.text(bar.get_x()+bar.get_width()/2, bar.get_height()+0.3,
                    f'{bar.get_height():.1f}', ha='center', va='bottom',
                    fontsize=6, color=c)
    ax.set_xticks(list(x))
    ax.set_xticklabels(cats, fontsize=7.5)
    sa(ax, 'Performance Metrics Summary', '', 'Value')
    ax.legend(fontsize=7)

    out = 'adaptive_rto_comparison.png'
    plt.savefig(out, dpi=150, bbox_inches='tight', facecolor=C_BG)
    print(f"\n[INFO] Plot saved → {out}")
    print_summary(mp, mk)


# ─────────────────────────────────────────────
# MAIN
# ─────────────────────────────────────────────
def main():
    if len(sys.argv) < 3:
        print("Usage: python3 plot_results.py results_proposed.csv results_paper.csv")
        sys.exit(1)

    f_prop, f_paper = sys.argv[1], sys.argv[2]
    for f in [f_prop, f_paper]:
        if not os.path.exists(f):
            print(f"[ERROR] Not found: {f}")
            sys.exit(1)

    prop  = load_csv(f_prop)
    paper = load_csv(f_paper)
    mp    = metrics(prop)
    mk    = metrics(paper)
    plot(prop, paper, mp, mk)

if __name__ == '__main__':
    main()