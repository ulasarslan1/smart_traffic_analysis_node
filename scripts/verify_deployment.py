import argparse
import re

from pathlib import Path

import joblib
import pandas as pd


MODEL_PATH = "models/decision_tree.joblib"

FEATURES = [
    "duration_ms",
    "min_cm",
    "max_cm",
    "mean_cm",
    "std_cm",
    "range_cm",
    "mean_delta_cm",
    "valid_samples",
]


def parse_fields(line):

    _, payload = line.split("]", 1)

    fields = {}

    for item in payload.strip().split(","):
        key, value = item.split("=", 1)
        fields[key.strip()] = value.strip()

    return fields


def load_log(filename):

    passages = {}
    completed = []

    current_completed = None

    with open(filename, "r", encoding="utf-8", errors="replace") as file:

        for line in file:

            line = line.strip()

            if line.startswith("[PASSAGE_START]"):

                data = parse_fields(line)
                passage_id = int(data["id"])

                passages[passage_id] = {
                    "id": passage_id,
                    "start_ms": int(data["start_ms"]),
                    "end_ms": None,
                    "samples": [],
                    "esp_features": None,
                    "esp_prediction": None,
                }

            elif line.startswith("[SAMPLE]"):

                data = parse_fields(line)
                passage_id = int(data["id"])

                if passage_id not in passages:
                    continue

                passages[passage_id]["samples"].append(
                    float(data["distance_cm"])
                )

            elif line.startswith("[PASSAGE_END]"):

                data = parse_fields(line)
                passage_id = int(data["id"])

                if passage_id not in passages:
                    continue

                passage = passages.pop(passage_id)
                passage["end_ms"] = int(data["end_ms"])

                completed.append(passage)
                current_completed = passage

            elif line.startswith("[FEATURES]"):

                if current_completed is None:
                    continue

                data = parse_fields(line)

                current_completed["esp_features"] = {
                    name: float(data[name])
                    for name in FEATURES
                }

            elif line.startswith("[ML]"):

                if current_completed is None:
                    continue

                match = re.search(r"prediction=([A-Za-z]+)", line)

                if match:
                    current_completed["esp_prediction"] = (
                        match.group(1).lower()
                    )

    return completed


def calculate_python_features(passage):

    distances = pd.Series(
        passage["samples"],
        dtype="float64"
    )

    if len(distances) < 3:
        return None

    min_cm = distances.min()
    max_cm = distances.max()

    return {
        "duration_ms":
            passage["end_ms"] - passage["start_ms"],

        "min_cm":
            min_cm,

        "max_cm":
            max_cm,

        "mean_cm":
            distances.mean(),

        "std_cm":
            distances.std(ddof=0),

        "range_cm":
            max_cm - min_cm,

        "mean_delta_cm":
            distances.diff().abs().dropna().mean(),

        "valid_samples":
            len(distances),
    }


def main():

    parser = argparse.ArgumentParser(
        description="Verify Python and ESP32 ML deployment results."
    )

    parser.add_argument(
        "--input",
        required=True,
        help="ESP32 serial log containing FEATURES and ML output"
    )

    args = parser.parse_args()

    if not Path(MODEL_PATH).exists():
        raise FileNotFoundError(MODEL_PATH)

    model = joblib.load(MODEL_PATH)

    passages = load_log(args.input)

    print("=" * 80)
    print("ESP32 DEPLOYMENT VERIFICATION")
    print("=" * 80)
    print("Completed passages:", len(passages))
    print()

    prediction_matches = 0
    verified_passages = 0

    for passage in passages:

        python_features = calculate_python_features(passage)

        if python_features is None:
            continue

        if passage["esp_features"] is None:
            continue

        feature_frame = pd.DataFrame(
            [[python_features[name] for name in FEATURES]],
            columns=FEATURES
        )

        python_prediction = model.predict(feature_frame)[0]
        esp_prediction = passage["esp_prediction"]

        print("-" * 80)
        print(f"Passage {passage['id']}")
        print()

        print(
            f"{'Feature':<20}"
            f"{'Python':>15}"
            f"{'ESP32':>15}"
            f"{'Difference':>15}"
        )

        for feature in FEATURES:

            python_value = float(python_features[feature])
            esp_value = float(
                passage["esp_features"][feature]
            )

            difference = abs(
                python_value - esp_value
            )

            print(
                f"{feature:<20}"
                f"{python_value:>15.4f}"
                f"{esp_value:>15.4f}"
                f"{difference:>15.6f}"
            )

        print()
        print("Python prediction :", python_prediction)
        print("ESP32 prediction  :", esp_prediction)

        prediction_match = (
            str(python_prediction).lower()
            == str(esp_prediction).lower()
        )

        print(
            "Prediction match   :",
            "YES" if prediction_match else "NO"
        )

        if prediction_match:
            prediction_matches += 1

        verified_passages += 1

    print()
    print("=" * 80)
    print("SUMMARY")
    print("=" * 80)

    print("Verified passages :", verified_passages)
    print(
        "Prediction matches :",
        f"{prediction_matches}/{verified_passages}"
    )

    if (
        verified_passages > 0
        and prediction_matches == verified_passages
    ):
        print("Deployment verification: PASSED")
    else:
        print("Deployment verification: FAILED")


if __name__ == "__main__":
    main()