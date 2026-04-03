import json
from pathlib import Path

import numpy as np
from sklearn.model_selection import train_test_split
from tensorflow.keras.callbacks import EarlyStopping
from tensorflow.keras.layers import Dense, Dropout, Input
from tensorflow.keras.models import Sequential

HEAD_DIR = Path(__file__).resolve().parent
ROOT_DIR = Path(__file__).resolve().parents[2]
MODEL_DIR = ROOT_DIR / "models"

X_PATH = HEAD_DIR / "X_head.npy"
Y_PATH = HEAD_DIR / "y_head.npy"
MODEL_PATH = MODEL_DIR / "head_model.keras"
SCALER_PATH = MODEL_DIR / "head_scaler.npz"
CLASSES_PATH = MODEL_DIR / "head_classes.json"

CLASSES = ["normal", "tilt_forward", "tilt_side", "look_away"]

# -------- LOAD DATA --------
X = np.load(X_PATH).astype(np.float32)
y = np.load(Y_PATH).astype(np.int64)

mean = X.mean(axis=0)
std = X.std(axis=0)
std = np.where(std < 1e-6, 1.0, std)
X_norm = (X - mean) / std

# -------- TRAIN TEST SPLIT --------
X_train, X_val, y_train, y_val = train_test_split(
    X_norm, y, test_size=0.2, random_state=42, stratify=y
)

# -------- MODEL --------
model = Sequential(
    [
        Input(shape=(3,)),
        Dense(128, activation="relu"),
        Dropout(0.35),
        Dense(64, activation="relu"),
        Dropout(0.25),
        Dense(32, activation="relu"),
        Dense(len(CLASSES), activation="softmax"),
    ]
)

model.compile(
    optimizer="adam",
    loss="sparse_categorical_crossentropy",
    metrics=["accuracy"],
)

callbacks = [
    EarlyStopping(
        monitor="val_loss",
        patience=4,
        restore_best_weights=True,
    )
]

# -------- TRAIN --------
history = model.fit(
    X_train,
    y_train,
    validation_data=(X_val, y_val),
    epochs=30,
    batch_size=64,
    callbacks=callbacks,
)

val_loss, val_acc = model.evaluate(X_val, y_val, verbose=0)

# -------- SAVE --------
MODEL_DIR.mkdir(parents=True, exist_ok=True)
model.save(MODEL_PATH)
np.savez(SCALER_PATH, mean=mean, std=std)
with CLASSES_PATH.open("w", encoding="utf-8") as f:
    json.dump(CLASSES, f, indent=2)

print(f"✅ Model trained and saved: {MODEL_PATH}")
print(f"✅ Scaler saved: {SCALER_PATH}")
print(f"✅ Classes saved: {CLASSES_PATH}")
print(f"Validation accuracy: {val_acc:.4f}")