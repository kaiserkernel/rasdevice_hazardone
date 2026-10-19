"""Builds a short synthetic test video from labeled Home-fire-dataset test
images, for exercising hazard-1's --video pipeline without a live camera.
Not part of the training pipeline -- just a dev convenience.

Usage: python make_test_video.py
"""
import random
from pathlib import Path

import cv2

DATASET_TEST = Path(r"D:\Home-fire-dataset\dataset\test")
OUT_PATH = Path(r"D:\hazard-1\test_assets\sample_fire_smoke.mp4")
FRAME_SIZE = (640, 480)
HOLD_FRAMES = 15  # frames per still image (~0.5s at 30fps)
FPS = 30


def classes_in_label(label_path: Path) -> set:
    classes = set()
    if not label_path.exists():
        return classes
    for line in label_path.read_text().splitlines():
        line = line.strip()
        if not line:
            continue
        classes.add(int(line.split()[0]))
    return classes


def pick_images(n_fire_only: int, n_smoke_only: int, n_both: int):
    images_dir = DATASET_TEST / "images"
    labels_dir = DATASET_TEST / "labels"
    fire_only, smoke_only, both = [], [], []
    for img_path in sorted(images_dir.glob("*.jpg")):
        label_path = labels_dir / (img_path.stem + ".txt")
        classes = classes_in_label(label_path)
        if classes == {0}:
            fire_only.append(img_path)
        elif classes == {1}:
            smoke_only.append(img_path)
        elif classes == {0, 1}:
            both.append(img_path)
    random.seed(42)
    random.shuffle(fire_only)
    random.shuffle(smoke_only)
    random.shuffle(both)
    chosen = fire_only[:n_fire_only] + smoke_only[:n_smoke_only] + both[:n_both]
    random.shuffle(chosen)
    return chosen


def main():
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    chosen = pick_images(n_fire_only=8, n_smoke_only=8, n_both=4)
    if not chosen:
        raise SystemExit("No labeled test images found -- is the dataset extracted?")

    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    writer = cv2.VideoWriter(str(OUT_PATH), fourcc, FPS, FRAME_SIZE)

    for img_path in chosen:
        img = cv2.imread(str(img_path))
        if img is None:
            continue
        img = cv2.resize(img, FRAME_SIZE)
        for _ in range(HOLD_FRAMES):
            writer.write(img)

    writer.release()
    print(f"Wrote {len(chosen)} images ({len(chosen) * HOLD_FRAMES} frames) to {OUT_PATH}")


if __name__ == "__main__":
    main()
