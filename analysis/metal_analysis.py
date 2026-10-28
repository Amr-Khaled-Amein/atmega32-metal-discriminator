"""
Metal Discrimination System - Data Analysis Script
Authors: Seif eldin Eslam Mohamed 8735 | Omar Tamer Ezz 8726 | Amr Khaled 8911

Run:      python analysis/metal_analysis.py
Requires: pip install -r analysis/requirements.txt
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from scipy import stats
import warnings
from pathlib import Path
warnings.filterwarnings('ignore')

HERE = Path(__file__).resolve().parent
PLOT_PATH = HERE.parent / 'docs' / 'images' / 'analysis_plots.png'

# ── Load ──────────────────────────────────────
df = pd.read_csv(HERE / 'metal_dataset.csv')

METAL_ORDER = [
    'Unknown Non-Ferrous','Copper','Stainless Steel','Silver',
    'Gold','Aluminum','Steel (Iron)','Unknown Ferrous'
]
COLORS = {
    'Unknown Non-Ferrous':'#95a5a6','Copper':'#e67e22',
    'Stainless Steel':'#3498db','Silver':'#bdc3c7',
    'Gold':'#f1c40f','Aluminum':'#9b59b6',
    'Steel (Iron)':'#e74c3c','Unknown Ferrous':'#1abc9c',
}
RANGES = {
    'Unknown Non-Ferrous':( 30, 80, 90),
    'Copper'             :( 90,103,115),
    'Stainless Steel'    :(115,125,140),
    'Silver'             :(140,155,190),
    'Gold'               :(190,220,235),
    'Aluminum'           :(235,255,270),
    'Steel (Iron)'       :(270,295,340),
    'Unknown Ferrous'    :(340,400,500),
}

# ── Classifier ────────────────────────────────
def firmware_classify(diff):
    for name,(lo,_,hi) in RANGES.items():
        if lo < diff <= hi:
            return name
    return 'No Metal'

df['predicted'] = df['final_diff'].apply(firmware_classify)
df['correct']   = df['predicted'] == df['metal_label']

# ── Console output ────────────────────────────
print("="*62)
print("  METAL DISCRIMINATION SYSTEM — STATISTICAL ANALYSIS")
print("  Embedded Systems Project #2 — Spring 2026")
print("="*62)

print("\n[1] Statistical Features per Metal (final_diff ADC units)")
print("-"*62)
print(f"{'Metal':<22}{'Mean':>8}{'Std':>7}{'Min':>8}{'Max':>8}{'RMS':>8}")
print("-"*62)
stats_rows = []
for m in METAL_ORDER:
    s = df[df['metal_label']==m]['final_diff']
    row = dict(metal=m, mean=s.mean(), std=s.std(),
               mn=s.min(), mx=s.max(), rms=np.sqrt((s**2).mean()))
    stats_rows.append(row)
    print(f"{m:<22}{row['mean']:>8.1f}{row['std']:>7.2f}"
          f"{row['mn']:>8.1f}{row['mx']:>8.1f}{row['rms']:>8.1f}")
stats_df = pd.DataFrame(stats_rows).set_index('metal')
print("-"*62)

overall = df['correct'].mean()*100
print(f"\n[2] Classification Accuracy")
print("-"*62)
print(f"{'Metal':<22}{'Samples':>8}{'Correct':>9}{'Accuracy':>10}")
print("-"*62)
for m in METAL_ORDER:
    sub = df[df['metal_label']==m]
    acc = sub['correct'].mean()*100
    print(f"{m:<22}{len(sub):>8}{sub['correct'].sum():>9}{acc:>9.0f}%")
print("-"*62)
print(f"{'OVERALL':<22}{len(df):>8}{df['correct'].sum():>9}{overall:>9.1f}%")

# ── Plots ─────────────────────────────────────
fig = plt.figure(figsize=(18,13))
fig.suptitle(
    'Metal Discrimination System — Pulse Induction | Feature Analysis\n'
    'Seif eldin Eslam Mohamed 8735  |  Omar Tamer Ezz 8726  |  Amr Khaled 8911',
    fontsize=11, fontweight='bold', y=0.99)

colors_list = [COLORS[m] for m in METAL_ORDER]

# Plot 1 — Box plot
ax1 = fig.add_subplot(2,3,1)
data_by_metal = [df[df['metal_label']==m]['final_diff'].values for m in METAL_ORDER]
bp = ax1.boxplot(data_by_metal, patch_artist=True)
for patch,col in zip(bp['boxes'], colors_list):
    patch.set_facecolor(col); patch.set_alpha(0.85)
ax1.set_xticklabels([m.replace(' ','\n') for m in METAL_ORDER], fontsize=7)
ax1.set_ylabel('Final Diff (ADC units)')
ax1.set_title('Signal Distribution per Metal')
ax1.grid(True, alpha=0.3)

# Plot 2 — Mean ± std bar
ax2 = fig.add_subplot(2,3,2)
means = [stats_df.loc[m,'mean'] for m in METAL_ORDER]
stds  = [stats_df.loc[m,'std']  for m in METAL_ORDER]
x = np.arange(len(METAL_ORDER))
ax2.bar(x, means, yerr=stds, color=colors_list, alpha=0.85,
        capsize=5, edgecolor='black', linewidth=0.5)
for m,lo,ce,hi in [(m,)+RANGES[m] for m in METAL_ORDER]:
    i = METAL_ORDER.index(m)
    ax2.axhspan(lo,hi,alpha=0.07,color=colors_list[i])
ax2.set_xticks(x)
ax2.set_xticklabels([m.replace(' ','\n') for m in METAL_ORDER], fontsize=7)
ax2.set_ylabel('Mean Final Diff ± Std')
ax2.set_title('Mean Signal with Firmware Bands')
ax2.grid(True, alpha=0.3, axis='y')

# Plot 3 — Histogram with classifier bands
ax3 = fig.add_subplot(2,3,3)
for m,col in zip(METAL_ORDER, colors_list):
    sub = df[df['metal_label']==m]['final_diff']
    ax3.hist(sub, bins=10, color=col, alpha=0.6, label=m,
             edgecolor='white', linewidth=0.4)
for m,col in zip(METAL_ORDER, colors_list):
    lo,ce,hi = RANGES[m]
    ax3.axvspan(lo,hi,alpha=0.06,color=col)
    ax3.axvline(ce,color=col,linestyle='--',linewidth=0.9,alpha=0.7)
ax3.set_xlabel('Final Diff (ADC units)')
ax3.set_ylabel('Count')
ax3.set_title('Diff Distribution with Classifier Bands')
ax3.legend(fontsize=5, ncol=2)
ax3.grid(True, alpha=0.3)

# Plot 4 — Scatter baseline vs diff
ax4 = fig.add_subplot(2,3,4)
for m,col in zip(METAL_ORDER, colors_list):
    sub = df[df['metal_label']==m]
    ax4.scatter(sub['baseline_mean'], sub['final_diff'],
                color=col, label=m, alpha=0.75, s=25, edgecolors='none')
ax4.set_xlabel('Baseline Mean (ADC)')
ax4.set_ylabel('Final Diff (ADC units)')
ax4.set_title('Baseline Stability vs Metal Response')
ax4.legend(fontsize=5)
ax4.grid(True, alpha=0.3)

# Plot 5 — Confidence CDF
ax5 = fig.add_subplot(2,3,5)
for m,col in zip(METAL_ORDER, colors_list):
    sub = sorted(df[df['metal_label']==m]['confidence_pct'].values)
    ax5.plot(sub, np.linspace(0,1,len(sub)), color=col, label=m, linewidth=1.5)
ax5.axvline(10, color='red', linestyle='--', linewidth=1.2, label='Min threshold (10%)')
ax5.set_xlabel('Confidence Score (%)')
ax5.set_ylabel('Cumulative Fraction')
ax5.set_title('Confidence Score CDF per Metal')
ax5.legend(fontsize=5)
ax5.grid(True, alpha=0.3)

# Plot 6 — Accuracy bar
ax6 = fig.add_subplot(2,3,6)
acc_vals = [df[df['metal_label']==m]['correct'].mean()*100 for m in METAL_ORDER]
bars = ax6.bar(range(len(METAL_ORDER)), acc_vals,
               color=colors_list, alpha=0.88,
               edgecolor='black', linewidth=0.5)
ax6.set_xticks(range(len(METAL_ORDER)))
ax6.set_xticklabels([m.replace(' ','\n') for m in METAL_ORDER], fontsize=7)
ax6.set_ylabel('Classification Accuracy (%)')
ax6.set_title(f'Per-Metal Accuracy  (Overall: {overall:.1f}%)')
ax6.set_ylim(0,115)
ax6.axhline(100, color='green', linestyle='--', linewidth=1, alpha=0.5)
ax6.axhline(75,  color='orange', linestyle=':', linewidth=1, alpha=0.7,
            label='75% target')
ax6.legend(fontsize=7)
ax6.grid(True, alpha=0.3, axis='y')
for bar,val in zip(bars, acc_vals):
    ax6.text(bar.get_x()+bar.get_width()/2, bar.get_height()+1,
             f'{val:.0f}%', ha='center', va='bottom', fontsize=8, fontweight='bold')

plt.tight_layout(rect=[0,0,1,0.94])
plt.savefig(PLOT_PATH, dpi=150, bbox_inches='tight')
plt.close()
print("\n[3] Plots saved → analysis_plots.png")

print(f"\n[4] Feature Template Vectors")
print("-"*62)
print(f"{'Metal':<22}{'Mean':>10}{'Std':>8}{'RMS':>8}{'Variance':>12}")
print("-"*62)
for m in METAL_ORDER:
    s = df[df['metal_label']==m]['final_diff']
    print(f"{m:<22}{s.mean():>10.2f}{s.std():>8.3f}"
          f"{np.sqrt((s**2).mean()):>8.2f}{s.var():>12.4f}")
print("-"*62)
print(f"\nTotal samples : {len(df)}")
print(f"Metals        : {df['metal_label'].nunique()}")
print(f"Overall acc.  : {overall:.1f}%")
print("Done.")
