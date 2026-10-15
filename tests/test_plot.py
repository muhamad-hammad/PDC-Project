import csv
import tempfile
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "analysis"))
from plot import main


class PlotTest(unittest.TestCase):
    def test_summary_and_graphs(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[1]) as temp:
            raw = Path(temp) / "raw.csv"
            with raw.open("w", newline="") as f:
                writer = csv.writer(f)
                writer.writerow(("jobs", "max_operand", "seed", "mode", "schedule", "threads", "repeat", "seconds", "steps"))
                for rep in range(10):
                    writer.writerow((100, 12, 1, "serial", "static", 0, rep, 2.0, 1000))
                    writer.writerow((100, 12, 1, "openmp", "static", 2, rep, 1.0, 1000))
                    writer.writerow((100, 12, 1, "openmp", "dynamic", 2, rep, 1.2, 1000))
            main(raw)
            with (Path(temp) / "raw_summary.csv").open(newline="") as f:
                rows = list(csv.DictReader(f))
            parallel = next(r for r in rows if r["mode"] == "openmp" and r["schedule"] == "static")
            self.assertEqual(float(parallel["speedup"]), 2.0)
            self.assertEqual(float(parallel["efficiency"]), 1.0)
            for name in ("time", "speedup", "efficiency"):
                self.assertIn("<svg", (Path(temp) / f"raw_{name}.svg").read_text())


if __name__ == "__main__":
    unittest.main()
