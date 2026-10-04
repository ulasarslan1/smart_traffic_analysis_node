import pandas as pd
import matplotlib.pyplot as plt

from sklearn.model_selection import train_test_split, StratifiedKFold, cross_val_score, GridSearchCV
from sklearn.metrics import accuracy_score, classification_report, confusion_matrix, ConfusionMatrixDisplay
from sklearn.ensemble import RandomForestClassifier

# ============================================================
# Configuration
# ============================================================

DATASET_PATH = "datasets/features.csv"

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

TARGET = "label"
RANDOM_STATE = 42


# ============================================================
# Load dataset
# ============================================================

df = pd.read_csv(DATASET_PATH)

print("=" * 60)
print("DATASET")
print("=" * 60)
print(df.head())
print()
print("Dataset shape:", df.shape)


# ============================================================
# Dataset validation
# ============================================================

print()
print("=" * 60)
print("DATA VALIDATION")
print("=" * 60)
print("\nMissing values:")
print(df.isnull().sum())
print("\nDuplicate rows:", df.duplicated().sum())
print("\nClass distribution:")
print(df[TARGET].value_counts())
print("\nClass distribution (%):")
print(df[TARGET].value_counts(normalize=True).mul(100).round(2))

required_columns = FEATURES + [TARGET]

missing_columns = [column for column in required_columns if column not in df.columns]

if missing_columns:
    raise ValueError(f"Missing columns in dataset: {missing_columns}")

if df[required_columns].isnull().any().any():
    raise ValueError("Dataset contains missing values in ML features or target.")


# ============================================================
# Features and target
# ============================================================

X = df[FEATURES]
y = df[TARGET]


# ============================================================
# Train / Validation / Test split
# ============================================================

X_trainval, X_test, y_trainval, y_test = train_test_split(
    X,
    y,
    test_size=0.20,
    random_state=RANDOM_STATE,
    stratify=y,
)

X_train, X_val, y_train, y_val = train_test_split(
    X_trainval,
    y_trainval,
    test_size=0.25,
    random_state=RANDOM_STATE,
    stratify=y_trainval,
)

print()
print("=" * 60)
print("DATA SPLIT")
print("=" * 60)
print("Train samples:", len(X_train))
print("Validation samples:", len(X_val))
print("Test samples:", len(X_test))
print("Total samples:", len(X_train) + len(X_val) + len(X_test))


# ============================================================
# Baseline Random Forest
# ============================================================

print()
print("=" * 60)
print("BASELINE MODEL")
print("=" * 60)

baseline_model = RandomForestClassifier(random_state=RANDOM_STATE ,n_jobs=-1)

baseline_model.fit(X_train, y_train)

baseline_pred = baseline_model.predict(X_val)

baseline_accuracy = accuracy_score(y_val, baseline_pred)

print("Baseline validation accuracy:", round(baseline_accuracy, 4))
print("\nBaseline classification report:")
print(classification_report(y_val, baseline_pred, digits=4))


# ============================================================
# Cross-validation
# ============================================================

print()
print("=" * 60)
print("CROSS VALIDATION")
print("=" * 60)

cv = StratifiedKFold(
    n_splits=5,
    shuffle=True,
    random_state=RANDOM_STATE,
)

cv_scores = cross_val_score(
    baseline_model,
    X_train,
    y_train,
    cv=cv,
    scoring="f1_macro",
    n_jobs=-1,
)

print("CV F1 scores:", cv_scores)
print("Mean CV F1:", round(cv_scores.mean(), 4))
print("Std CV F1:", round(cv_scores.std(), 4))


# ============================================================
# Hyperparameter tuning
# ============================================================

print()
print("=" * 60)
print("HYPERPARAMETER TUNING")
print("=" * 60)

param_grid = {
    "n_estimators": [50, 100, 200],
    "criterion": ["gini", "entropy"],
    "max_depth": [None, 3, 5, 8],
    "min_samples_split": [2, 5, 10],
    "min_samples_leaf": [1, 2, 4],
    "max_features": ["sqrt", "log2"],
}

grid_search = GridSearchCV(
    estimator=RandomForestClassifier(
        random_state=RANDOM_STATE,
        n_jobs=1,
    ),
    param_grid=param_grid,
    scoring="f1_macro",
    cv=cv,
    n_jobs=-1,
    refit=True,
)

grid_search.fit(X_train, y_train)

print("Best hyperparameters:", grid_search.best_params_)
print("Best CV F1:", round(grid_search.best_score_, 4))


# ============================================================
# Tuned model validation
# ============================================================

print()
print("=" * 60)
print("TUNED MODEL - VALIDATION")
print("=" * 60)

best_model = grid_search.best_estimator_

val_pred = best_model.predict(X_val)

val_accuracy = accuracy_score(y_val, val_pred)

print("Validation accuracy:", round(val_accuracy, 4))
print("\nClassification report:")
print(classification_report(y_val, val_pred, digits=4))


# ============================================================
# Final training
# ============================================================

print()
print("=" * 60)
print("FINAL TRAINING")
print("=" * 60)

final_model = RandomForestClassifier(**grid_search.best_params_, random_state=RANDOM_STATE)

final_model.fit(X_trainval, y_trainval)

print("Final model trained on", len(X_trainval), "samples.")


# ============================================================
# Final test
# ============================================================

print()
print("=" * 60)
print("FINAL TEST")
print("=" * 60)

test_pred = final_model.predict(X_test)

test_accuracy = accuracy_score(y_test, test_pred)

print("Final test accuracy:", round(test_accuracy, 4))
print("\nConfusion matrix:")
print(confusion_matrix(y_test, test_pred))
print("\nClassification report:")
print(classification_report(y_test, test_pred, digits=4))


# ============================================================
# Feature importance
# ============================================================

print()
print("=" * 60)
print("FEATURE IMPORTANCE")
print("=" * 60)

importance_df = pd.DataFrame({
    "feature": FEATURES,
    "importance": final_model.feature_importances_,
})

importance_df = importance_df.sort_values(by="importance", ascending=False).reset_index(drop=True)

print(importance_df)


# ============================================================
# Confusion matrix visualization
# ============================================================

ConfusionMatrixDisplay.from_predictions(y_test, test_pred)

plt.title("Random Forest - Confusion Matrix")
plt.tight_layout()
plt.show()


# ============================================================
# Feature importance visualization
# ============================================================

plt.figure(figsize=(9, 5))
plt.bar(importance_df["feature"], importance_df["importance"])
plt.title("Random Forest - Feature Importance")
plt.xlabel("Feature")
plt.ylabel("Importance")
plt.xticks(rotation=45, ha="right")
plt.tight_layout()
plt.show()

