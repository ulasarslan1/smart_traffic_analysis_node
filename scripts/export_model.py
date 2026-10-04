import joblib

from pathlib import Path
from sklearn.tree import _tree


# ============================================================
# Configuration
# ============================================================

MODEL_PATH = "models/decision_tree.joblib"
OUTPUT_PATH = "include/DecisionTreeModel.h"

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


# ============================================================
# Load trained model
# ============================================================

model = joblib.load(MODEL_PATH)

tree = model.tree_
class_names = model.classes_

print("=" * 60)
print("DECISION TREE EXPORT")
print("=" * 60)
print("Model:", MODEL_PATH)
print("Classes:", list(class_names))
print("Features:", FEATURES)
print("Tree nodes:", tree.node_count)
print("Tree depth:", tree.max_depth)


# ============================================================
# Validation
# ============================================================

if model.n_features_in_ != len(FEATURES):
    raise ValueError(
        f"Feature count mismatch: model expects {model.n_features_in_}, "
        f"but exporter defines {len(FEATURES)}."
    )

if hasattr(model, "feature_names_in_"):
    trained_features = list(model.feature_names_in_)

    if trained_features != FEATURES:
        raise ValueError(
            f"Feature order mismatch.\n"
            f"Model: {trained_features}\n"
            f"Exporter: {FEATURES}"
        )


# ============================================================
# Generate C++ tree
# ============================================================

def generate_node(node_id, indent=1):
    indentation = "    " * indent

    feature_index = tree.feature[node_id]

    if feature_index != _tree.TREE_UNDEFINED:
        feature_name = FEATURES[feature_index]
        threshold = tree.threshold[node_id]

        left_child = tree.children_left[node_id]
        right_child = tree.children_right[node_id]

        code = f"{indentation}if (features.{feature_name} <= {threshold:.10f}f) {{\n"
        code += generate_node(left_child, indent + 1)
        code += f"{indentation}}} else {{\n"
        code += generate_node(right_child, indent + 1)
        code += f"{indentation}}}\n"

        return code

    class_index = tree.value[node_id][0].argmax()
    class_name = class_names[class_index].upper()

    return f"{indentation}return VehicleClass::{class_name};\n"


tree_code = generate_node(0)


# ============================================================
# Generate header
# ============================================================

header = f"""#pragma once

// ============================================================
// AUTO-GENERATED FILE
//
// Generated from:
// {MODEL_PATH}
//
// Do not edit the decision rules manually.
// Regenerate this file using:
// python scripts/export_model.py
// ============================================================

enum class VehicleClass {{
    CAR,
    MOTORCYCLE,
    TRUCK
}};

struct PassageFeatures {{
    float duration_ms;
    float min_cm;
    float max_cm;
    float mean_cm;
    float std_cm;
    float range_cm;
    float mean_delta_cm;
    float valid_samples;
}};

inline VehicleClass predictVehicle(const PassageFeatures& features)
{{
{tree_code}}}

inline const char* vehicleClassName(VehicleClass vehicleClass)
{{
    switch (vehicleClass) {{
        case VehicleClass::CAR:
            return "CAR";

        case VehicleClass::MOTORCYCLE:
            return "MOTORCYCLE";

        case VehicleClass::TRUCK:
            return "TRUCK";

        default:
            return "UNKNOWN";
    }}
}}
"""


# ============================================================
# Write generated header
# ============================================================

output_path = Path(OUTPUT_PATH)
output_path.parent.mkdir(parents=True, exist_ok=True)

output_path.write_text(header, encoding="utf-8")

print()
print("Generated:", OUTPUT_PATH)
print("Export completed successfully.")