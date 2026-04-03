from pathlib import Path

import numpy as np
import pandas as pd

ROOT_DIR = Path(__file__).resolve().parents[2]
DATA_DIR = ROOT_DIR / "data" / "head_dataset"
OUT_X = Path(__file__).resolve().parent / "X_head.npy"
OUT_Y = Path(__file__).resolve().parent / "y_head.npy"

labels = ["normal", "tilt_forward", "tilt_side", "look_away"]
label_map = {label: idx for idx, label in enumerate(labels)}

X = []
y = []

for label in labels:
    folder = DATA_DIR / label
    if not folder.exists():
        print(f"Skipping missing folder: {folder}")
        continue

    for csv_file in sorted(folder.glob("*.csv")):
        df = pd.read_csv(csv_file)
        required = {"pitch", "yaw", "roll"}
        if not required.issubset(set(df.columns)):
            print(f"Skipping malformed file: {csv_file}")
            continue

        values = df[["pitch", "yaw", "roll"]].to_numpy(dtype=np.float32)
        if len(values) == 0:
            continue

        X.extend(values)
        y.extend([label_map[label]] * len(values))

X = np.array(X, dtype=np.float32)
y = np.array(y, dtype=np.int64)

if len(X) == 0:
    raise RuntimeError("No head dataset samples found. Record data first.")

np.save(OUT_X, X)
np.save(OUT_Y, y)

print("Dataset shape:", X.shape)
print("Labels shape:", y.shape)
print("Saved:", OUT_X)
print("Saved:", OUT_Y)