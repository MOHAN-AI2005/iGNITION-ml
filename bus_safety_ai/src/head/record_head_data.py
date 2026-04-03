import csv
import os
import time
from pathlib import Path

import cv2
import mediapipe as mp
import numpy as np

# -------- CONFIG --------
ROOT_DIR = Path(__file__).resolve().parents[2]
BASE_DIR = ROOT_DIR / "data" / "head_dataset"
FACE_LANDMARKER_PATH = Path(
    os.getenv(
        "FACE_LANDMARKER_PATH",
        ROOT_DIR / "src" / "data_processing" / "face_landmarker.task",
    )
)
WINDOW_NAME = "Head Data Recorder"

labels = {
    "1": "normal",
    "2": "tilt_forward",
    "3": "tilt_side",
    "4": "look_away",
}

current_label = "normal"
recording = False
writer = None
file_handle = None


def estimate_head_features(lm):
    left_eye = lm[33]
    right_eye = lm[263]
    nose_tip = lm[1]
    mouth_center = lm[13]

    eye_mid_x = (left_eye.x + right_eye.x) / 2.0
    eye_mid_y = (left_eye.y + right_eye.y) / 2.0

    eye_dx = right_eye.x - left_eye.x
    eye_dy = right_eye.y - left_eye.y
    eye_dist = max(1e-6, (eye_dx ** 2 + eye_dy ** 2) ** 0.5)

    yaw = (nose_tip.x - eye_mid_x) / eye_dist

    face_vertical = max(1e-6, abs(mouth_center.y - eye_mid_y))
    expected_nose_y = (eye_mid_y + mouth_center.y) / 2.0
    pitch = (nose_tip.y - expected_nose_y) / face_vertical

    roll = np.degrees(np.arctan2(eye_dy, eye_dx))

    return float(pitch), float(yaw), float(roll)


# -------- MEDIAPIPE --------
BaseOptions = mp.tasks.BaseOptions
VisionRunningMode = mp.tasks.vision.RunningMode
FaceLandmarker = mp.tasks.vision.FaceLandmarker
FaceLandmarkerOptions = mp.tasks.vision.FaceLandmarkerOptions

options = FaceLandmarkerOptions(
    base_options=BaseOptions(model_asset_path=str(FACE_LANDMARKER_PATH)),
    running_mode=VisionRunningMode.IMAGE,
    num_faces=1,
)

landmarker = FaceLandmarker.create_from_options(options)

# -------- CAMERA --------
cap = cv2.VideoCapture(0)

print(
    """
Controls:
1 -> NORMAL
2 -> TILT_FORWARD
3 -> TILT_SIDE
4 -> LOOK_AWAY
r -> START recording
s -> STOP recording
ESC -> EXIT
"""
)

while True:
    ret, frame = cap.read()
    if not ret:
        continue

    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
    result = landmarker.detect(mp_image)

    pitch, yaw, roll = 0.0, 0.0, 0.0

    if result.face_landmarks:
        lm = result.face_landmarks[0]
        pitch, yaw, roll = estimate_head_features(lm)

        if recording and writer:
            writer.writerow([pitch, yaw, roll])

    status = "RECORDING" if recording else "IDLE"

    cv2.putText(frame, f"Class: {current_label}", (10, 30), 0, 0.7, (255, 255, 255), 2)
    cv2.putText(frame, f"Status: {status}", (10, 60), 0, 0.7, (0, 255, 0) if recording else (0, 0, 255), 2)
    cv2.putText(frame, f"Pitch:{pitch:.2f} Yaw:{yaw:.2f} Roll:{roll:.2f}", (10, 90), 0, 0.6, (200, 200, 255), 1)
    cv2.putText(frame, "1:Normal  2:Forward  3:Side  4:LookAway", (10, 120), 0, 0.6, (200, 200, 255), 1)
    cv2.putText(frame, "r:Start  s:Stop  ESC:Exit", (10, 145), 0, 0.6, (200, 200, 255), 1)

    cv2.imshow(WINDOW_NAME, frame)
    key = cv2.waitKey(1) & 0xFF

    if key in [ord(k) for k in labels.keys()]:
        current_label = labels[chr(key)]
        print("Selected:", current_label)
    elif key == ord("r") and not recording:
        folder = BASE_DIR / current_label
        folder.mkdir(parents=True, exist_ok=True)

        filename = f"{current_label}_{int(time.time())}.csv"
        filepath = folder / filename

        file_handle = open(filepath, "w", newline="", encoding="utf-8")
        writer = csv.writer(file_handle)
        writer.writerow(["pitch", "yaw", "roll"])

        recording = True
        print("Recording started:", filepath)
    elif key == ord("s") and recording:
        recording = False
        if file_handle:
            file_handle.close()
        file_handle = None
        writer = None
        print("Recording stopped")
    elif key == 27:
        break

if file_handle:
    file_handle.close()
cap.release()
cv2.destroyAllWindows()
del landmarker