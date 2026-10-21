from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


FILES = ("idle.csv", "flexion.csv", "extension.csv", "partial.csv")
CHANNELS = ("ax", "ay", "az", "gx", "gy", "gz")


def main() -> None:
    root = Path(__file__).parent
    files = [root / name for name in FILES if (root / name).exists()]

    if not files:
        raise SystemExit("No expected CSV files were found.")

    figure, axes = plt.subplots(
        len(files),
        1,
        figsize=(15, 3.5 * len(files)),
        sharex=False,
    )
    if len(files) == 1:
        axes = [axes]

    for axis, path in zip(axes, files):
        data = pd.read_csv(path)
        missing = set(CHANNELS).difference(data.columns)
        if missing:
            raise SystemExit(f"{path.name} is missing columns: {sorted(missing)}")

        time_seconds = (
            data["timestamp_ms"] - data["timestamp_ms"].iloc[0]
        ) / 1000

        for channel in ("ax", "ay", "az"):
            axis.plot(time_seconds, data[channel], label=channel)
        for channel in ("gx", "gy", "gz"):
            axis.plot(time_seconds, data[channel], "--", label=channel)

        axis.set_title(path.name)
        axis.set_ylabel("Raw value")
        axis.grid(True)
        axis.legend(ncol=6, loc="upper right")

    axes[-1].set_xlabel("Time (seconds)")
    figure.suptitle("Wrist rehabilitation sessions", fontsize=16)
    figure.tight_layout()

    output = root / "wrist_sessions_plot.png"
    figure.savefig(output, dpi=150)
    print(f"Saved plot to {output}")
    plt.show()


if __name__ == "__main__":
    main()
