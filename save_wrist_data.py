import argparse
import csv
import time
from pathlib import Path

import serial


def is_data_row(line: str) -> bool:
    fields = line.split(",")
    return len(fields) == 8 and fields[-1] in {
        "idle",
        "flexion",
        "extension",
        "partial",
    }


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Save wrist rehabilitation data from the ESP8266 logger."
    )
    parser.add_argument("--port", default="COM3", help="NodeMCU serial port")
    parser.add_argument(
        "--label",
        required=True,
        choices=("idle", "flexion", "extension", "partial"),
    )
    parser.add_argument("--seconds", type=int, default=60)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    output = args.output or Path(
        f"wrist_{args.label}_{time.strftime('%Y%m%d_%H%M%S')}.csv"
    )

    with serial.Serial(args.port, args.baud, timeout=1) as device:
        time.sleep(2)
        device.reset_input_buffer()
        device.write(f"l={args.label}\n".encode("ascii"))
        device.write(b"start\n")
        time.sleep(0.2)

        rows = []
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            line = device.readline().decode("utf-8", errors="replace").strip()
            if is_data_row(line):
                rows.append(line.split(","))

        device.write(b"stop\n")

    with output.open("w", newline="", encoding="utf-8") as csv_file:
        writer = csv.writer(csv_file)
        writer.writerow(["timestamp_ms", "ax", "ay", "az", "gx", "gy", "gz", "label"])
        writer.writerows(rows)

    print(f"Saved {len(rows)} samples to {output.resolve()}")
    if len(rows) < args.seconds * 40:
        print("Warning: fewer samples than expected. Check the COM port and cable.")


if __name__ == "__main__":
    main()
