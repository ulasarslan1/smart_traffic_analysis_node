import argparse
from pathlib import Path

import pandas as pd

# Serial log parsing
def parse_fields(line):
    
    _, payload = line.split("]", 1)

    fields = {}

    for item in payload.strip().split(","):
        key, value = item.split("=", 1)

        fields[key.strip()] = value.strip()

    return fields


def load_serial_log(filename):
    
    active_passages = {}
    completed_passages = []

    with open(filename,"r",encoding="utf-8",errors="replace") as file:
        
        for line in file:
            line = line.strip()

            if line.startswith("[PASSAGE_START]"):

                data = parse_fields(line)
                passage_id = int(data["id"])

                active_passages[passage_id] = {
                    "id": passage_id,
                    "start_ms": int(data["start_ms"]),
                    "end_ms": None,
                    "samples": []
                }

            elif line.startswith("[SAMPLE]"):

                data = parse_fields(line)
                passage_id = int(data["id"])

                if passage_id not in active_passages:
                    continue

                active_passages[passage_id
                ]["samples"].append({
                    "t_ms": int(data["t_ms"]),
                    "distance_cm":float(data["distance_cm"])           
                })

            elif line.startswith("[PASSAGE_END]"):

                data = parse_fields(line)

                passage_id = int(data["id"])

                if passage_id not in active_passages:
                    continue

                passage = active_passages.pop(passage_id)
                passage["end_ms"] = int(data["end_ms"])
                completed_passages.append( passage)
                            
    return completed_passages


def extract_features(passages, label, source):
    """
    Calculates statistical features from each
    completed raw distance profile.
    """
    rows = []

    for passage in passages:

        samples = passage["samples"]

        if len(samples) < 3:
            continue

        distances = pd.Series( [sample["distance_cm"] for sample in samples], dtype="float64" )
                
        duration_ms = (passage["end_ms"]- passage["start_ms"])
           
        min_cm = distances.min()
        max_cm = distances.max()

        mean_delta_cm = (
            distances
            .diff()
            .abs()
            .dropna()
            .mean()
        )

        row = {
            "source": source,
            "passage_id": passage["id"],
            "duration_ms": duration_ms,
            "min_cm": min_cm,
            "max_cm": max_cm,
            "mean_cm": distances.mean(),
            "std_cm": distances.std(ddof=0),
            "range_cm": max_cm - min_cm,
            "mean_delta_cm": mean_delta_cm,
            "valid_samples": len(distances),
            "label": label
        }

        rows.append(row)

    return rows

# Dataset
def save_dataset(rows, output_file):

    if not rows:
        return None

    output_path = Path(output_file)

    output_path.parent.mkdir(parents=True,exist_ok=True)
    
    new_data = pd.DataFrame(rows)

    if output_path.exists():

        existing_data = pd.read_csv(output_path)
        
        dataset = pd.concat([existing_data, new_data ],ignore_index=True)
                
    else:
        dataset = new_data


    dataset = dataset.drop_duplicates(subset=["source","passage_id"],keep="first")   
    dataset.to_csv(output_path,index=False)
    
    return dataset


def main():

    parser = argparse.ArgumentParser(
        description=(
            "Create a SENTRY vehicle feature dataset "
            "from raw ESP32 passage logs."
        )
    )

    parser.add_argument(
        "--input",
        required=True,
        help="Raw serial log file"
    )

    parser.add_argument(
        "--label",
        required=True,
        choices=[
            "motorcycle",
            "car",
            "truck"
        ],
        help="Ground-truth vehicle class"
    )

    parser.add_argument(
        "--source",
        required=True,
        help=(
            "Unique experiment name, "
            "for example car_run_01"
        )
    )

    parser.add_argument(
        "--output",
        default="datasets/features.csv",
        help="Output CSV dataset"
    )

    args = parser.parse_args()

    # Parse
    passages = load_serial_log(args.input)
    
    print(f"Completed passages : {len(passages)}")

    # Feature extraction
    rows = extract_features(passages,args.label,args.source)
        
    print(f"Valid passages     : {len(rows)}")

    dataset = save_dataset(rows,args.output)
        
    if dataset is None:
        return

    print(f"Dataset            : {args.output}")
    print(f"Total observations : {len(dataset)}")
    print()
    print(dataset.tail())
    

if __name__ == "__main__":
    main()