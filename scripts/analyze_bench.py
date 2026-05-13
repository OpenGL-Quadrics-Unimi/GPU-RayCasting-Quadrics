#!/usr/bin/env python3
"""Process bench_*.csv files into LaTeX tables and PNG plots.

Run from the project root:
    python3 scripts/analyze_bench.py [--machine amd|m3]

Inputs:  build/test_results/<MACHINE>/bench_*.csv
Outputs: report/tables/*.tex   (LaTeX fragments, \\input{} into report.tex)
         report/plots/*.png    (matplotlib figures, \\includegraphics)
"""
import argparse
from pathlib import Path
import pandas as pd
import matplotlib.pyplot as plt

# Configuration
# Mid-size molecule used in the headline per-pass table and the
# resolution / ablation sweeps. 1HHP is small enough to fit in every
# resolution and large enough to exercise all passes.
HEADLINE_MOL = "1HHP  HIV-PR"
# Compact label used in captions (collapses internal whitespace).
HEADLINE_LABEL = " ".join(HEADLINE_MOL.split())

PASS_COLS = ["shadow_ms", "geometry_ms", "ssao_ms",
             "lighting_ms", "silhouette_ms", "composite_ms"]
PASS_LABELS = {
    "shadow_ms":     "Shadow map",
    "geometry_ms":   "Geometry (G-buffer)",
    "ssao_ms":       "SSAO + blur",
    "lighting_ms":   "Lighting",
    "silhouette_ms": "Silhouette (Sobel)",
    "composite_ms":  "Composite",
}

# Molecule order for the scaling table (sorted by atom count, ascending).
# Pulled from the data so we do not hard-code it twice.

# Helpers
def load_csv(path: Path) -> pd.DataFrame:
    return pd.read_csv(path)

def median_per_molecule(df: pd.DataFrame) -> pd.DataFrame:
    """Collapse 300 rows per molecule into one row of medians."""
    agg = {c: "median" for c in PASS_COLS + ["gpu_total_ms", "cpu_ms"]}
    agg.update({"atoms": "first", "bonds": "first",
                "fb_w": "first", "fb_h": "first"})
    return df.groupby("molecule").agg(agg)

def latex_header(caption: str, label: str, colspec: str) -> str:
    return (f"\\begin{{table}}[H]\n"
            f"    \\centering\n"
            f"    \\small\n"
            f"    \\begin{{tabular}}{{{colspec}}}\n"
            f"        \\toprule\n")

def latex_footer(caption: str, label: str) -> str:
    return (f"        \\bottomrule\n"
            f"    \\end{{tabular}}\n"
            f"    \\caption{{{caption}}}\n"
            f"    \\label{{{label}}}\n"
            f"\\end{{table}}\n")

def write_tex(path: Path, text: str) -> None:
    path.write_text(text)
    print(f"  wrote {path.relative_to(Path.cwd())}")

# Tables
def table_per_pass(df_full: pd.DataFrame, out: Path, machine: str) -> None:
    """Table 1: per-pass median time + share, for HEADLINE_MOL at 1080p."""
    med = median_per_molecule(df_full)
    if HEADLINE_MOL not in med.index:
        raise SystemExit(f"Molecule {HEADLINE_MOL!r} not found in CSV")
    row = med.loc[HEADLINE_MOL]
    total = sum(row[c] for c in PASS_COLS)
    fps   = 1000.0 / row["gpu_total_ms"]

    lines = [latex_header("", "", "lrr")]
    lines.append("        Pass & GPU time (ms) & Share (\\%) \\\\\n")
    lines.append("        \\midrule\n")
    for c in PASS_COLS:
        ms  = row[c]
        pct = 100.0 * ms / total
        lines.append(f"        {PASS_LABELS[c]} & {ms:.2f} & {pct:.1f} \\\\\n")
    lines.append("        \\midrule\n")
    lines.append(f"        \\textbf{{Total}} & \\textbf{{{total:.2f}}} & 100.0 \\\\\n")
    lines.append(f"        FPS (= 1000/total) & \\multicolumn{{2}}{{r}}{{\\textbf{{{fps:.1f}}}}} \\\\\n")
    cap = (f"Per-pass GPU time on the {machine.upper()} machine, "
           f"1080p framebuffer, molecule {HEADLINE_LABEL} "
           f"({int(row['atoms'])} atoms, {int(row['bonds'])} bonds). "
           f"Median over 300 frames.")
    lines.append(latex_footer(cap, f"tab:perpass_{machine}"))
    write_tex(out, "".join(lines))

def table_molecule_scaling(df_full: pd.DataFrame, out: Path, machine: str) -> None:
    """Table 2: median GPU/CPU time and FPS for all 11 molecules at 1080p."""
    med = median_per_molecule(df_full).sort_values("atoms")
    lines = [latex_header("", "", "lrrrrr")]
    lines.append("        Molecule & Atoms & Bonds & GPU (ms) & CPU (ms) & FPS \\\\\n")
    lines.append("        \\midrule\n")
    for label, row in med.iterrows():
        fps = 1000.0 / row["gpu_total_ms"]
        # Strip the trailing description so the label fits the column.
        short = label.split()[0]
        lines.append(f"        {short} & {int(row['atoms'])} & {int(row['bonds'])} "
                     f"& {row['gpu_total_ms']:.2f} & {row['cpu_ms']:.2f} & {fps:.1f} \\\\\n")
    cap = (f"Scaling with molecule size on the {machine.upper()} machine "
           f"at 1080p. Median GPU and CPU frame time over 300 frames per molecule.")
    lines.append(latex_footer(cap, f"tab:scaling_{machine}"))
    write_tex(out, "".join(lines))

def table_resolution(df_720, df_1080, df_native, out: Path, machine: str) -> None:
    """Table 3: per-pass GPU time at 3 resolutions, for HEADLINE_MOL."""
    rows = []
    for tag, df in [("720p", df_720), ("1080p", df_1080), ("native", df_native)]:
        med = median_per_molecule(df).loc[HEADLINE_MOL]
        fb = f"{int(med['fb_w'])}x{int(med['fb_h'])}"
        rows.append((tag, fb, med))

    lines = [latex_header("", "", "lr" + "r" * len(PASS_COLS) + "rr")]
    headers = "Res. & FB pixels & " + " & ".join(PASS_LABELS[c] for c in PASS_COLS) + " & Total & FPS"
    lines.append(f"        {headers} \\\\\n")
    lines.append("        \\midrule\n")
    for tag, fb, med in rows:
        cells = [f"{med[c]:.2f}" for c in PASS_COLS]
        total = med["gpu_total_ms"]
        fps = 1000.0 / total
        lines.append(f"        {tag} & {fb} & " + " & ".join(cells) +
                     f" & {total:.2f} & {fps:.1f} \\\\\n")
    cap = (f"Resolution scaling on the {machine.upper()} machine, "
           f"molecule {HEADLINE_LABEL}. Per-pass median GPU time "
           f"(ms) over 300 frames. Fill-bound passes (SSAO, lighting, silhouette) "
           f"scale with pixel count; geometry and shadow do not.")
    lines.append(latex_footer(cap, f"tab:resolution_{machine}"))
    write_tex(out, "".join(lines))

def table_ablation(csv_dir: Path, out: Path, machine: str) -> None:
    """Table 4: full vs no_shadows vs no_ssao vs no_outlines vs bare, at 1080p."""
    configs = [
        ("Full pipeline",  f"bench_full_1080p_{machine}.csv"),
        ("No shadows",     f"bench_no_shadows_{machine}.csv"),
        ("No SSAO",        f"bench_no_ssao_{machine}.csv"),
        ("No outlines",    f"bench_no_outlines_{machine}.csv"),
        ("Bare (none)",    f"bench_bare_{machine}.csv"),
    ]
    rows = []
    full_total = None
    for label, fname in configs:
        med = median_per_molecule(load_csv(csv_dir / fname)).loc[HEADLINE_MOL]
        total = med["gpu_total_ms"]
        if label == "Full pipeline":
            full_total = total
        delta = total - full_total if full_total is not None else 0.0
        rows.append((label, total, delta, 1000.0 / total))

    lines = [latex_header("", "", "lrrr")]
    lines.append("        Configuration & GPU total (ms) & $\\Delta$ vs full (ms) & FPS \\\\\n")
    lines.append("        \\midrule\n")
    for label, total, delta, fps in rows:
        dstr = "0.00" if delta == 0.0 else f"{delta:+.2f}"
        lines.append(f"        {label} & {total:.2f} & {dstr} & {fps:.1f} \\\\\n")
    cap = (f"Feature ablation on the {machine.upper()} machine at 1080p, "
           f"molecule {HEADLINE_LABEL}. $\\Delta$ is the change in "
           f"GPU frame time relative to the full pipeline. A negative value "
           f"means the configuration is faster.")
    lines.append(latex_footer(cap, f"tab:ablation_{machine}"))
    write_tex(out, "".join(lines))

# Table 5: PDB parse and bond detection time vs atom count.
# Also computes the theoretical O(N^2) predicted bond time relative to
# the smallest molecule, so the table makes the scaling argument explicit.
def table_load_times(load_csv_path: Path, out: Path, machine: str) -> None:
    df = pd.read_csv(load_csv_path).sort_values("atoms").reset_index(drop=True)

    # Reference point for the O(N^2) prediction: smallest molecule (first row).
    ref_atoms = df["atoms"].iloc[0]
    ref_bond  = df["bond_ms"].iloc[0]

    lines = [latex_header("", "", "lrrrrrr")]
    lines.append("        Molecule & Atoms & Bonds & "
                 "Parse (ms) & Bond detect (ms) & "
                 "Predicted $O(N^2)$ (ms) & Total (ms) \\\\\n")
    lines.append("        \\midrule\n")
    for _, row in df.iterrows():
        predicted = ref_bond * (row["atoms"] / ref_atoms) ** 2
        lines.append(
            f"        {row['molecule']} & {int(row['atoms'])} & {int(row['bonds'])} "
            f"& {row['parse_ms']:.2f} & {row['bond_ms']:.2f} "
            f"& {predicted:.2f} & {row['total_ms']:.2f} \\\\\n"
        )
    cap = (f"PDB parse and bond detection times on the {machine.upper()} machine "
           f"(median of 5 runs). The predicted $O(N^2)$ column scales the "
           f"bond detection time of the smallest molecule (1CRN) by $(N/N_{{1CRN}})^2$. "
           f"The close match confirms the quadratic growth of the pairwise distance check.")
    lines.append(latex_footer(cap, f"tab:load_{machine}"))
    write_tex(out, "".join(lines))

def plot_load_times(load_csv_path: Path, out: Path, machine: str) -> None:
    """Log-log plot of bond detection time vs atom count with O(N^2) reference line."""
    df = pd.read_csv(load_csv_path).sort_values("atoms")
    atoms = df["atoms"].values
    bond  = df["bond_ms"].values

    # O(N^2) reference line anchored at the smallest molecule.
    ref_a, ref_b = atoms[0], bond[0]
    ref_line = ref_b * (atoms / ref_a) ** 2

    fig, ax = plt.subplots(figsize=(6.0, 4.0))
    ax.loglog(atoms, bond,     marker="o", color="C0", linewidth=1.5, label="Measured")
    ax.loglog(atoms, ref_line, linestyle="--", color="C1", linewidth=1.2,
              label=r"$O(N^2)$ reference")
    for _, row in df.iterrows():
        ax.annotate(row["molecule"], (row["atoms"], row["bond_ms"]),
                    fontsize=7, xytext=(4, 4), textcoords="offset points")
    ax.set_xlabel("Atom count (log scale)")
    ax.set_ylabel("Bond detection time (ms, log scale)")
    ax.set_title(f"Bond detection scaling ({machine.upper()})")
    ax.legend(fontsize=8, frameon=False)
    ax.grid(True, which="both", alpha=0.3)
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)
    print(f"  wrote {out.relative_to(Path.cwd())}")

# Plots
def plot_scaling(df_full: pd.DataFrame, out: Path, machine: str) -> None:
    """GPU total frame time vs atom count, 11 molecules at 1080p."""
    med = median_per_molecule(df_full).sort_values("atoms")
    fig, ax = plt.subplots(figsize=(6.0, 4.0))
    ax.plot(med["atoms"], med["gpu_total_ms"], marker="o", color="C0", linewidth=1.5)
    for label, row in med.iterrows():
        ax.annotate(label.split()[0], (row["atoms"], row["gpu_total_ms"]),
                    fontsize=7, xytext=(4, 4), textcoords="offset points")
    ax.set_xscale("log")
    ax.set_xlabel("Atom count (log scale)")
    ax.set_ylabel("GPU frame time (ms, median)")
    ax.set_title(f"Scaling with molecule size ({machine.upper()}, 1080p)")
    ax.grid(True, which="both", alpha=0.3)
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)
    print(f"  wrote {out.relative_to(Path.cwd())}")

def plot_pass_breakdown(df_full: pd.DataFrame, out: Path, machine: str) -> None:
    """Stacked bar of per-pass cost across all 11 molecules at 1080p."""
    med = median_per_molecule(df_full).sort_values("atoms")
    labels = [m.split()[0] for m in med.index]
    bottom = [0.0] * len(med)
    fig, ax = plt.subplots(figsize=(7.5, 4.0))
    colors = plt.cm.tab10.colors
    for i, c in enumerate(PASS_COLS):
        vals = med[c].tolist()
        ax.bar(labels, vals, bottom=bottom, label=PASS_LABELS[c], color=colors[i])
        bottom = [b + v for b, v in zip(bottom, vals)]
    ax.set_ylabel("GPU time (ms)")
    ax.set_title(f"Per-pass breakdown across molecules ({machine.upper()}, 1080p)")
    ax.legend(loc="upper left", fontsize=8, frameon=False)
    plt.xticks(rotation=30, ha="right", fontsize=8)
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)
    print(f"  wrote {out.relative_to(Path.cwd())}")

# Comparison tables and plots (both machines together)

def table_comparison_perpass(amd_dir: Path, m3_dir: Path, out: Path) -> None:
    """Side-by-side per-pass table for both machines at 1080p, HEADLINE_MOL."""
    amd = median_per_molecule(load_csv(amd_dir / "bench_full_1080p_amd.csv")).loc[HEADLINE_MOL]
    m3  = median_per_molecule(load_csv(m3_dir  / "bench_full_1080p_m3.csv" )).loc[HEADLINE_MOL]
    lines = [latex_header("", "", "lrrrr")]
    lines.append("        Pass & AMD (ms) & M3 (ms) & M3 share (\\%) & Speedup \\\\\n")
    lines.append("        \\midrule\n")
    m3_total = sum(m3[c] for c in PASS_COLS)
    for c in PASS_COLS:
        pct   = 100.0 * m3[c] / m3_total
        speedup = amd[c] / m3[c]
        lines.append(f"        {PASS_LABELS[c]} & {amd[c]:.2f} & {m3[c]:.2f}"
                     f" & {pct:.1f} & {speedup:.1f}$\\times$ \\\\\n")
    lines.append("        \\midrule\n")
    amd_t, m3_t = amd["gpu_total_ms"], m3["gpu_total_ms"]
    lines.append(f"        \\textbf{{Total}} & \\textbf{{{amd_t:.2f}}} & \\textbf{{{m3_t:.2f}}}"
                 f" & 100.0 & {amd_t/m3_t:.1f}$\\times$ \\\\\n")
    lines.append(f"        FPS & {1000/amd_t:.0f} & {1000/m3_t:.0f} & & \\\\\n")
    cap = (f"Per-pass GPU time comparison at 1080p, molecule {HEADLINE_LABEL}. "
           f"Median over 300 frames. Speedup is AMD time divided by M3 time.")
    lines.append(latex_footer(cap, "tab:compare_perpass"))
    write_tex(out, "".join(lines))

def table_comparison_resolution(amd_dir: Path, m3_dir: Path, out: Path) -> None:
    """Resolution scaling comparison for both machines, HEADLINE_MOL."""
    tags = [("720p", "bench_full_720p"), ("1080p", "bench_full_1080p"), ("native", "bench_full_native")]
    lines = [latex_header("", "", "lrrrr")]
    lines.append("        Resolution & FB pixels & AMD (ms) & M3 (ms) & Speedup \\\\\n")
    lines.append("        \\midrule\n")
    for tag, stem in tags:
        a_row = median_per_molecule(load_csv(amd_dir / f"{stem}_amd.csv")).loc[HEADLINE_MOL]
        m_row = median_per_molecule(load_csv(m3_dir  / f"{stem}_m3.csv" )).loc[HEADLINE_MOL]
        fb = f"{int(a_row['fb_w'])}$\\times${int(a_row['fb_h'])}"
        a, m = a_row["gpu_total_ms"], m_row["gpu_total_ms"]
        lines.append(f"        {tag} & {fb} & {a:.2f} & {m:.2f} & {a/m:.1f}$\\times$ \\\\\n")
    cap = (f"Resolution scaling comparison, molecule {HEADLINE_LABEL}. "
           f"GPU total frame time (ms, median over 300 frames).")
    lines.append(latex_footer(cap, "tab:compare_resolution"))
    write_tex(out, "".join(lines))

def table_comparison_ablation(amd_dir: Path, m3_dir: Path, out: Path) -> None:
    """Feature ablation comparison for both machines at 1080p, HEADLINE_MOL."""
    configs = [
        ("Full pipeline",  "bench_full_1080p"),
        ("No shadows",     "bench_no_shadows"),
        ("No SSAO",        "bench_no_ssao"),
        ("No outlines",    "bench_no_outlines"),
        ("Bare",           "bench_bare"),
    ]
    lines = [latex_header("", "", "lrrrr")]
    lines.append("        Configuration & AMD (ms) & AMD FPS & M3 (ms) & M3 FPS \\\\\n")
    lines.append("        \\midrule\n")
    for label, stem in configs:
        a = median_per_molecule(load_csv(amd_dir / f"{stem}_amd.csv")).loc[HEADLINE_MOL, "gpu_total_ms"]
        m = median_per_molecule(load_csv(m3_dir  / f"{stem}_m3.csv" )).loc[HEADLINE_MOL, "gpu_total_ms"]
        lines.append(f"        {label} & {a:.2f} & {1000/a:.0f} & {m:.2f} & {1000/m:.0f} \\\\\n")
    cap = (f"Feature ablation on both machines at 1080p, molecule {HEADLINE_LABEL}.")
    lines.append(latex_footer(cap, "tab:compare_ablation"))
    write_tex(out, "".join(lines))

def plot_comparison_scaling(amd_dir: Path, m3_dir: Path, out: Path) -> None:
    """GPU total time vs atom count, both machines on the same axes."""
    amd = median_per_molecule(load_csv(amd_dir / "bench_full_1080p_amd.csv")).sort_values("atoms")
    m3  = median_per_molecule(load_csv(m3_dir  / "bench_full_1080p_m3.csv" )).sort_values("atoms")
    fig, ax = plt.subplots(figsize=(6.5, 4.0))
    ax.plot(amd["atoms"], amd["gpu_total_ms"], marker="o", color="C0",
            linewidth=1.5, label="AMD (2019 MBP)")
    ax.plot(m3["atoms"],  m3["gpu_total_ms"],  marker="s", color="C1",
            linewidth=1.5, label="Apple M3 (MacBook Air)")
    for label, row in amd.iterrows():
        ax.annotate(label.split()[0], (row["atoms"], row["gpu_total_ms"]),
                    fontsize=6, xytext=(4, 2), textcoords="offset points", color="C0")
    ax.set_xscale("log")
    ax.set_xlabel("Atom count (log scale)")
    ax.set_ylabel("GPU frame time (ms, median)")
    ax.set_title("Molecule scaling comparison (1080p)")
    ax.legend(fontsize=8, frameon=False)
    ax.grid(True, which="both", alpha=0.3)
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)
    print(f"  wrote {out.relative_to(Path.cwd())}")

# Main
def _resolve_dir(root: Path, machine: str) -> Path:
    candidates = [
        root / "bench_data" / machine.upper(),
        root / "build" / "test_results" / machine.upper(),
        root / "build" / "test_results",
    ]
    return next((d for d in candidates if d.is_dir()), candidates[0])

def run_single(machine: str, root: Path, tab_dir: Path, plot_dir: Path) -> None:
    csv_dir = _resolve_dir(root, machine)
    print(f"Reading CSVs from {csv_dir}")
    df_720    = load_csv(csv_dir / f"bench_full_720p_{machine}.csv")
    df_1080   = load_csv(csv_dir / f"bench_full_1080p_{machine}.csv")
    df_native = load_csv(csv_dir / f"bench_full_native_{machine}.csv")

    print("Writing tables:")
    table_per_pass        (df_1080, tab_dir / f"perpass_{machine}.tex",   machine)
    table_molecule_scaling(df_1080, tab_dir / f"scaling_{machine}.tex",   machine)
    table_resolution      (df_720, df_1080, df_native,
                           tab_dir / f"resolution_{machine}.tex",         machine)
    table_ablation        (csv_dir, tab_dir / f"ablation_{machine}.tex",  machine)

    load_csv_path = csv_dir / f"bench_load_{machine}.csv"
    if load_csv_path.exists():
        table_load_times(load_csv_path, tab_dir  / f"load_{machine}.tex", machine)
        plot_load_times (load_csv_path, plot_dir / f"load_{machine}.png",  machine)
    else:
        print(f"  skipping load tables (no {load_csv_path.name})")

    print("Writing plots:")
    plot_scaling       (df_1080, plot_dir / f"scaling_{machine}.png",       machine)
    plot_pass_breakdown(df_1080, plot_dir / f"pass_breakdown_{machine}.png", machine)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--machine", default="amd",
                    help="amd | m3 | both  (default: amd)")
    args = ap.parse_args()
    machine = args.machine.lower()

    root     = Path(__file__).resolve().parent.parent
    tab_dir  = root / "report" / "tables"
    plot_dir = root / "report" / "plots"
    tab_dir.mkdir(parents=True, exist_ok=True)
    plot_dir.mkdir(parents=True, exist_ok=True)

    if machine == "both":
        run_single("amd", root, tab_dir, plot_dir)
        run_single("m3",  root, tab_dir, plot_dir)

        amd_dir = _resolve_dir(root, "amd")
        m3_dir  = _resolve_dir(root, "m3")
        print("Writing comparison tables and plots:")
        table_comparison_perpass   (amd_dir, m3_dir, tab_dir  / "compare_perpass.tex")
        table_comparison_resolution(amd_dir, m3_dir, tab_dir  / "compare_resolution.tex")
        table_comparison_ablation  (amd_dir, m3_dir, tab_dir  / "compare_ablation.tex")
        plot_comparison_scaling    (amd_dir, m3_dir, plot_dir / "compare_scaling.png")
    else:
        run_single(machine, root, tab_dir, plot_dir)

    print("Done.")

if __name__ == "__main__":
    main()
