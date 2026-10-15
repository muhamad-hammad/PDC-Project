"""Summarize benchmark CSV and draw SVG graphs (standard library only)."""
import csv
import html
import statistics
import sys
from collections import defaultdict
from pathlib import Path


def main(source: Path) -> None:
    groups = defaultdict(list)
    with source.open(newline="") as f:
        for row in csv.DictReader(f):
            key = (int(row["jobs"]), row["mode"], row["schedule"], int(row["threads"]))
            groups[key].append(float(row["seconds"]))
    if not groups:
        raise ValueError("no benchmark rows")
    summary = []
    for (jobs, mode, schedule, threads), times in sorted(groups.items()):
        baseline = statistics.median(groups[(jobs, "serial", "static", 0)])
        median = statistics.median(times)
        speedup = baseline / median
        summary.append((jobs, mode, schedule, threads, median, statistics.pstdev(times), speedup,
                        speedup / threads if threads else 1.0))
    with source.with_name(source.stem + "_summary.csv").open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(("jobs", "mode", "schedule", "threads", "median_seconds", "stddev_seconds", "speedup", "efficiency"))
        writer.writerows(summary)
    for metric, index, label in (("time", 4, "Median seconds"), ("speedup", 6, "Speedup"),
                                 ("efficiency", 7, "Efficiency")):
        series = []
        for jobs in sorted({r[0] for r in summary}):
            for schedule in ("static", "dynamic"):
                rows = [r for r in summary if r[0] == jobs and r[1] == "openmp" and r[2] == schedule]
                if rows:
                    series.append((f"{jobs} jobs {schedule}", [(r[3], r[index]) for r in rows]))
        if not series:
            continue
        xmax = max(x for _, points in series for x, _ in points)
        ymax = max(y for _, points in series for _, y in points) * 1.1 or 1.0
        colors = ("#1565c0", "#d84315", "#2e7d32", "#6a1b9a", "#00838f", "#ad1457")
        parts = ['<svg xmlns="http://www.w3.org/2000/svg" width="800" height="520" viewBox="0 0 800 520">',
                 '<rect width="800" height="520" fill="white"/>',
                 '<path d="M70 25 V440 H750" fill="none" stroke="#222" stroke-width="2"/>',
                 '<text x="400" y="500" text-anchor="middle">OpenMP threads</text>',
                 f'<text x="20" y="225" transform="rotate(-90 20 225)" text-anchor="middle">{html.escape(label)}</text>']
        for i, (title, points) in enumerate(series):
            color = colors[i % len(colors)]
            coords = [(70 + 660 * x / xmax, 440 - 390 * y / ymax) for x, y in points]
            path = " ".join(("M" if j == 0 else "L") + f"{x:.1f},{y:.1f}" for j, (x, y) in enumerate(coords))
            parts.append(f'<path d="{path}" fill="none" stroke="{color}" stroke-width="2"/>')
            parts.extend(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="4" fill="{color}"/>' for x, y in coords)
            parts.append(f'<text x="580" y="{35 + 20*i}" fill="{color}">{html.escape(title)}</text>')
        for i in range(5):
            y = 440 - 390 * i / 4
            parts.append(f'<text x="60" y="{y+4:.1f}" text-anchor="end">{ymax*i/4:.2f}</text>')
        for x in (1, 2, 4, 8):
            if x <= xmax:
                parts.append(f'<text x="{70+660*x/xmax:.1f}" y="460" text-anchor="middle">{x}</text>')
        parts.append('</svg>')
        source.with_name(source.stem + "_" + metric + ".svg").write_text("\n".join(parts), encoding="utf-8")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: python plot.py raw.csv")
    main(Path(sys.argv[1]))
