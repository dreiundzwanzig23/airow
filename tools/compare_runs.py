#!/usr/bin/env python3
"""Generate an offline comparison report bundle for emitted AIRow run artifacts."""

from __future__ import annotations

import argparse
from html import escape
import json
import math
from pathlib import Path
import sys
from typing import Any, Iterable

from validate_visualization_artifact import validate_document

MANIFEST_SCHEMA_ID = "airow.run_comparison.v1"
REPORT_SCHEMA_ID = "airow.run_comparison_report.v1"
SUPPORTED_ARTIFACT_SCHEMA = "a007-json-v2"
SUPPORTED_VISUALIZATION_SCHEMA = "airow.visualization.v1"
PLOT_COLORS = ("#1f77b4", "#d62728", "#2ca02c", "#9467bd", "#ff7f0e")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate an offline AIRow run comparison report bundle."
    )
    parser.add_argument("--manifest", required=True, help="Comparison manifest JSON")
    parser.add_argument(
        "--output-dir", required=True, help="Directory where report files are written"
    )
    return parser.parse_args()


def is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def load_json(path: Path, label: str) -> dict[str, Any]:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise RuntimeError(f"missing {label}: {path}") from exc
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"failed to parse {label} {path}: {exc}") from exc
    if not isinstance(document, dict):
        raise RuntimeError(f"{label} {path}: expected JSON object")
    return document


def require_string(mapping: dict[str, Any], key: str, path: str) -> str:
    value = mapping.get(key)
    if not isinstance(value, str) or not value:
        raise RuntimeError(f"{path}.{key}: expected non-empty string")
    return value


def require_existing_path(mapping: dict[str, Any], key: str, path: str) -> Path:
    value = require_string(mapping, key, path)
    artifact_path = Path(value)
    if not artifact_path.exists():
        raise RuntimeError(f"{path}.{key}: missing artifact file {artifact_path}")
    return artifact_path


def validate_manifest(manifest: dict[str, Any]) -> None:
    if manifest.get("schema_id") != MANIFEST_SCHEMA_ID:
        raise RuntimeError(f"$.schema_id: expected {MANIFEST_SCHEMA_ID}")
    require_string(manifest, "comparison_id", "$")

    alignment = manifest.get("alignment")
    if not isinstance(alignment, dict):
        raise RuntimeError("$.alignment: expected object")
    mode = alignment.get("mode")
    if mode not in ("time", "stroke_phase"):
        raise RuntimeError("$.alignment.mode: expected time or stroke_phase")
    start_time_s = alignment.get("start_time_s")
    end_time_s = alignment.get("end_time_s")
    if not is_number(start_time_s):
        raise RuntimeError("$.alignment.start_time_s: expected finite number")
    if not is_number(end_time_s):
        raise RuntimeError("$.alignment.end_time_s: expected finite number")
    if float(start_time_s) >= float(end_time_s):
        raise RuntimeError("$.alignment: start_time_s must be less than end_time_s")

    runs = manifest.get("runs")
    if not isinstance(runs, list) or len(runs) < 2:
        raise RuntimeError("$.runs: expected at least two runs")
    seen_ids: set[str] = set()
    for index, run in enumerate(runs):
        run_path = f"$.runs[{index}]"
        if not isinstance(run, dict):
            raise RuntimeError(f"{run_path}: expected object")
        run_id = require_string(run, "id", run_path)
        if run_id in seen_ids:
            raise RuntimeError(f"{run_path}.id: duplicate run id {run_id}")
        seen_ids.add(run_id)
        require_string(run, "label", run_path)
        require_string(run, "role", run_path)
        require_existing_path(run, "summary_path", run_path)
        require_existing_path(run, "time_series_path", run_path)
        if "visualization_path" in run:
            require_existing_path(run, "visualization_path", run_path)


def scalar_channel_value(record: dict[str, Any], key: str) -> float | None:
    value = record.get(key)
    if isinstance(value, dict):
        raw = value.get("value")
        if is_number(raw):
            return float(raw)
    return None


def nested_scalar_value(record: dict[str, Any], parent: str, key: str) -> float | None:
    value = record.get(parent)
    if isinstance(value, dict):
        return scalar_channel_value(value, key)
    return None


def vector_magnitude(value: Any) -> float | None:
    if not isinstance(value, dict):
        return None
    vector = value.get("vector")
    if not isinstance(vector, dict):
        return None
    components = vector.get("value")
    if not isinstance(components, list) or len(components) != 3:
        return None
    if not all(is_number(component) for component in components):
        return None
    return math.sqrt(sum(float(component) * float(component) for component in components))


def blade_load_magnitude(record: dict[str, Any], side: str) -> float | None:
    loads = record.get("blade_load_world_n")
    if not isinstance(loads, dict):
        return None
    return vector_magnitude(loads.get(side))


def mean(values: Iterable[float]) -> float:
    values_list = list(values)
    if not values_list:
        return 0.0
    return sum(values_list) / len(values_list)


def schema_version(document: dict[str, Any]) -> str:
    value = document.get("schema_version")
    return value if isinstance(value, str) else ""


def validate_artifact_schema(document: dict[str, Any], path: Path, label: str) -> None:
    if schema_version(document) != SUPPORTED_ARTIFACT_SCHEMA:
        raise RuntimeError(
            f"{label} {path}: expected schema_version {SUPPORTED_ARTIFACT_SCHEMA}"
        )


def load_visualization(path: Path) -> dict[str, Any]:
    visualization = load_json(path, "visualization artifact")
    issues = validate_document(visualization)
    if issues:
        joined = "; ".join(issues[:4])
        if len(issues) > 4:
            joined += f"; {len(issues) - 4} more issue(s)"
        raise RuntimeError(f"invalid visualization artifact {path}: {joined}")
    if visualization.get("schema_id") != SUPPORTED_VISUALIZATION_SCHEMA:
        raise RuntimeError(
            f"visualization artifact {path}: expected schema_id {SUPPORTED_VISUALIZATION_SCHEMA}"
        )
    return visualization


class RunBundle:
    def __init__(self, manifest_run: dict[str, Any]) -> None:
        self.id = require_string(manifest_run, "id", "$.runs[]")
        self.label = require_string(manifest_run, "label", "$.runs[]")
        self.role = require_string(manifest_run, "role", "$.runs[]")
        self.summary_path = Path(require_string(manifest_run, "summary_path", "$.runs[]"))
        self.time_series_path = Path(
            require_string(manifest_run, "time_series_path", "$.runs[]")
        )
        visualization_value = manifest_run.get("visualization_path")
        self.visualization_path = (
            Path(visualization_value) if isinstance(visualization_value, str) else None
        )
        self.summary = load_json(self.summary_path, "summary artifact")
        self.time_series = load_json(self.time_series_path, "time-series artifact")
        validate_artifact_schema(self.summary, self.summary_path, "summary artifact")
        validate_artifact_schema(
            self.time_series, self.time_series_path, "time-series artifact"
        )
        self.records = self.time_series.get("records")
        if not isinstance(self.records, list) or not self.records:
            raise RuntimeError(f"time-series artifact {self.time_series_path}: empty records")
        if not all(isinstance(record, dict) for record in self.records):
            raise RuntimeError(
                f"time-series artifact {self.time_series_path}: malformed records"
            )
        self.visualization = (
            load_visualization(self.visualization_path)
            if self.visualization_path is not None
            else None
        )

    @property
    def metadata(self) -> dict[str, Any]:
        metadata = self.summary.get("metadata")
        return metadata if isinstance(metadata, dict) else {}

    @property
    def normalized_config(self) -> dict[str, str]:
        items = self.metadata.get("normalized_config")
        config: dict[str, str] = {}
        if not isinstance(items, list):
            return config
        for item in items:
            if isinstance(item, dict) and isinstance(item.get("key"), str):
                config[item["key"]] = str(item.get("value", ""))
        return config


def record_time(record: dict[str, Any]) -> float:
    value = record.get("time_s")
    if not is_number(value):
        raise RuntimeError("time-series record time_s: expected finite number")
    return float(value)


def records_in_window(run: RunBundle, start_time_s: float, end_time_s: float) -> list[dict[str, Any]]:
    records = [
        record
        for record in run.records
        if start_time_s <= record_time(record) <= end_time_s
    ]
    if not records:
        raise RuntimeError(f"run {run.id}: no samples in comparison window")
    return records


def rounded_time_key(value: float) -> str:
    return f"{value:.9f}"


def aligned_records(
    runs: list[RunBundle], alignment: dict[str, Any]
) -> tuple[list[str], dict[str, dict[str, dict[str, Any]]]]:
    mode = str(alignment["mode"])
    start_time_s = float(alignment["start_time_s"])
    end_time_s = float(alignment["end_time_s"])
    per_run: dict[str, dict[str, dict[str, Any]]] = {}

    for run in runs:
        selected = records_in_window(run, start_time_s, end_time_s)
        keyed: dict[str, dict[str, Any]] = {}
        if mode == "time":
            for record in selected:
                keyed[rounded_time_key(record_time(record))] = record
        else:
            for record in selected:
                stroke_input = record.get("stroke_input")
                if not isinstance(stroke_input, dict) or not isinstance(
                    stroke_input.get("phase"), str
                ):
                    raise RuntimeError(
                        f"run {run.id}: stroke_phase alignment requires phase labels"
                    )
                phase = stroke_input["phase"]
                keyed.setdefault(phase, record)
        per_run[run.id] = keyed

    common_keys = sorted(set.intersection(*(set(per_run[run.id]) for run in runs)))
    if not common_keys:
        raise RuntimeError("alignment: no overlapping samples across runs")
    return common_keys, per_run


def provider_ids(metadata: dict[str, Any]) -> dict[str, str]:
    providers = metadata.get("providers")
    if not isinstance(providers, dict):
        return {}
    result: dict[str, str] = {}
    for role, provider in providers.items():
        if isinstance(provider, dict):
            result[str(role)] = str(provider.get("id", "unknown"))
        else:
            result[str(role)] = str(provider)
    return result


def backend_ids(metadata: dict[str, Any]) -> dict[str, str]:
    return {
        "mechanics": str(metadata.get("mechanics_backend_id", "")),
        "integration": str(metadata.get("integration_backend_id", "")),
    }


def calibration_provenance(metadata: dict[str, Any]) -> list[dict[str, str]]:
    artifacts = metadata.get("external_artifacts")
    result: list[dict[str, str]] = []
    if not isinstance(artifacts, list):
        return result
    for artifact in artifacts:
        if isinstance(artifact, dict):
            result.append(
                {
                    "role": str(artifact.get("role", "")),
                    "schema_id": str(artifact.get("schema_id", "")),
                    "source_id": str(artifact.get("source_id", "")),
                    "content_hash": str(artifact.get("content_hash", "")),
                }
            )
    return sorted(result, key=lambda item: (item["role"], item["source_id"]))


def trust_envelope(metadata: dict[str, Any]) -> dict[str, Any]:
    trust = metadata.get("trust_envelope")
    return trust if isinstance(trust, dict) else {}


def run_metric(run: RunBundle, metric: str, records: list[dict[str, Any]]) -> float:
    summary = run.summary.get("summary")
    analysis = run.summary.get("analysis")
    if metric == "mean_speed_mps" and isinstance(summary, dict) and is_number(
        summary.get("mean_speed_mps")
    ):
        return float(summary["mean_speed_mps"])
    if metric == "distance_m" and isinstance(summary, dict) and is_number(
        summary.get("distance_m")
    ):
        return float(summary["distance_m"])
    if metric == "peak_stroke_power_w" and isinstance(analysis, dict):
        peak = analysis.get("peak_stroke_power_w")
        if isinstance(peak, dict) and is_number(peak.get("value")):
            return float(peak["value"])
    if metric == "mean_boat_speed_window_mps":
        return mean(
            value
            for record in records
            if (value := scalar_channel_value(record, "boat_speed_mps")) is not None
        )
    return 0.0


def headline_deltas(
    runs: list[RunBundle],
    common_keys: list[str],
    per_run: dict[str, dict[str, dict[str, Any]]],
) -> dict[str, Any]:
    baseline = runs[0]
    comparison = runs[1]
    baseline_records = [per_run[baseline.id][key] for key in common_keys]
    comparison_records = [per_run[comparison.id][key] for key in common_keys]
    metrics = {
        "mean_speed_mps": "m/s",
        "distance_m": "m",
        "peak_stroke_power_w": "W",
        "mean_boat_speed_window_mps": "m/s",
    }
    deltas: dict[str, Any] = {}
    for metric, unit in metrics.items():
        baseline_value = run_metric(baseline, metric, baseline_records)
        comparison_value = run_metric(comparison, metric, comparison_records)
        deltas[metric] = {
            "baseline_run_id": baseline.id,
            "comparison_run_id": comparison.id,
            "baseline": baseline_value,
            "comparison": comparison_value,
            "delta": comparison_value - baseline_value,
            "unit": unit,
        }
    return deltas


def config_differences(runs: list[RunBundle]) -> list[dict[str, Any]]:
    baseline = runs[0]
    baseline_config = baseline.normalized_config
    differences: list[dict[str, Any]] = []
    keys = sorted(set(baseline_config).union(*(set(run.normalized_config) for run in runs[1:])))
    for key in keys:
        values = {run.id: run.normalized_config.get(key, "") for run in runs}
        if len(set(values.values())) > 1:
            differences.append({"key": key, "values": values})
    return differences


def keyed_differences(runs: list[RunBundle], name: str) -> list[dict[str, Any]]:
    values: dict[str, Any] = {}
    for run in runs:
        if name == "providers":
            values[run.id] = provider_ids(run.metadata)
        elif name == "backends":
            values[run.id] = backend_ids(run.metadata)
        else:
            values[run.id] = calibration_provenance(run.metadata)
    if len({json.dumps(value, sort_keys=True) for value in values.values()}) <= 1:
        return []
    return [{"scope": name, "values": values}]


def comparability(
    runs: list[RunBundle], common_keys: list[str]
) -> dict[str, Any]:
    software_reasons: list[str] = []
    if not common_keys:
        software_reasons.append("no overlapping aligned samples")
    for run in runs:
        if schema_version(run.summary) != SUPPORTED_ARTIFACT_SCHEMA:
            software_reasons.append(f"{run.id}: unsupported summary schema")
        if schema_version(run.time_series) != SUPPORTED_ARTIFACT_SCHEMA:
            software_reasons.append(f"{run.id}: unsupported time-series schema")

    software_comparable = not software_reasons
    calibration_reasons: list[str] = []
    provider_sets = {json.dumps(provider_ids(run.metadata), sort_keys=True) for run in runs}
    backend_sets = {json.dumps(backend_ids(run.metadata), sort_keys=True) for run in runs}
    calibration_sets = {
        json.dumps(calibration_provenance(run.metadata), sort_keys=True) for run in runs
    }
    if not software_comparable:
        calibration_reasons.append("software comparison failed")
    if len(provider_sets) > 1:
        calibration_reasons.append("provider ids differ")
    if len(backend_sets) > 1:
        calibration_reasons.append("backend ids differ")
    if len(calibration_sets) > 1:
        calibration_reasons.append("calibration provenance differs")

    calibration_comparable = not calibration_reasons
    physical_reasons: list[str] = []
    fidelity_tiers = {
        str(trust_envelope(run.metadata).get("fidelity_tier", "unknown"))
        for run in runs
    }
    if not calibration_comparable:
        physical_reasons.append("calibration comparison failed")
    if len(fidelity_tiers) > 1:
        physical_reasons.append("fidelity tiers differ")
    for run in runs:
        trust = trust_envelope(run.metadata)
        validity = str(trust.get("validity_status", "unknown"))
        confidence = str(trust.get("confidence_status", "unknown"))
        if validity in ("out_of_validity", "outside_declared_envelope"):
            physical_reasons.append(f"{run.id}: trust validity is {validity}")
        if confidence == "low_confidence":
            physical_reasons.append(f"{run.id}: trust confidence is low_confidence")

    physical_comparable = not physical_reasons
    return {
        "software_comparable": software_comparable,
        "software_reasons": software_reasons,
        "calibration_comparable": calibration_comparable,
        "calibration_reasons": calibration_reasons,
        "physical_comparable": physical_comparable,
        "physical_reasons": physical_reasons,
    }


def vector_ids(visualization: dict[str, Any] | None) -> set[str]:
    if not visualization:
        return set()
    samples = visualization.get("samples")
    if not isinstance(samples, list) or not samples:
        return set()
    first = samples[0]
    if not isinstance(first, dict):
        return set()
    vectors = first.get("vectors")
    if not isinstance(vectors, dict):
        return set()
    return {str(key) for key in vectors}


def unavailable_visual_channels(visualization: dict[str, Any] | None) -> dict[str, str]:
    if not visualization:
        return {}
    channels = visualization.get("channels")
    if not isinstance(channels, dict):
        return {}
    reasons: dict[str, str] = {}
    for channel_id, channel in channels.items():
        if isinstance(channel, dict) and channel.get("availability") == "unavailable":
            reasons[str(channel_id)] = str(channel.get("reason", "unavailable"))
    return reasons


def vector_metadata(
    visualization: dict[str, Any] | None, shared_vectors: list[str]
) -> dict[str, dict[str, str]]:
    if not visualization:
        return {}
    samples = visualization.get("samples")
    if not isinstance(samples, list) or not samples or not isinstance(samples[0], dict):
        return {}
    vectors = samples[0].get("vectors")
    if not isinstance(vectors, dict):
        return {}
    metadata: dict[str, dict[str, str]] = {}
    for vector_id in shared_vectors:
        channel = vectors.get(vector_id)
        if isinstance(channel, dict):
            metadata[vector_id] = {
                "unit": str(channel.get("unit", "")),
                "frame": str(channel.get("frame", "")),
            }
    return metadata


def channel_coverage(runs: list[RunBundle]) -> dict[str, Any]:
    scalar_candidates = [
        "boat_speed_mps",
        "stroke_power_w",
        "energy_accounting.blade_power_w",
        "energy_accounting.rower_input_power_w",
    ]
    shared_scalars: list[str] = []
    unavailable: dict[str, str] = {}
    for channel in scalar_candidates:
        supported = True
        for run in runs:
            record = run.records[0]
            if "." in channel:
                parent, key = channel.split(".", 1)
                supported = nested_scalar_value(record, parent, key) is not None
            else:
                supported = scalar_channel_value(record, channel) is not None
            if not supported:
                break
        if supported:
            shared_scalars.append(channel)
        else:
            unavailable[channel] = "channel missing from one or more time-series artifacts"

    vector_sets = [vector_ids(run.visualization) for run in runs if run.visualization]
    shared_vectors = sorted(set.intersection(*vector_sets)) if vector_sets else []
    for run in runs:
        unavailable.update(unavailable_visual_channels(run.visualization))

    return {
        "time_series_channels": shared_scalars,
        "visualization_vectors": vector_metadata(runs[0].visualization, shared_vectors),
        "unavailable_reasons": dict(sorted(unavailable.items())),
    }


def series_for_channel(
    runs: list[RunBundle],
    common_keys: list[str],
    per_run: dict[str, dict[str, dict[str, Any]]],
    channel: str,
) -> list[tuple[str, list[float], str]]:
    series: list[tuple[str, list[float], str]] = []
    for index, run in enumerate(runs):
        values: list[float] = []
        for key in common_keys:
            record = per_run[run.id][key]
            if channel == "boat_speed_mps":
                values.append(scalar_channel_value(record, "boat_speed_mps") or 0.0)
            elif channel == "stroke_power_w":
                values.append(scalar_channel_value(record, "stroke_power_w") or 0.0)
            elif channel == "port_blade_force_n":
                values.append(blade_load_magnitude(record, "port") or 0.0)
            elif channel == "starboard_blade_force_n":
                values.append(blade_load_magnitude(record, "starboard") or 0.0)
            elif channel == "blade_power_w":
                values.append(nested_scalar_value(record, "energy_accounting", "blade_power_w") or 0.0)
            else:
                values.append(0.0)
        series.append((run.label, values, PLOT_COLORS[index % len(PLOT_COLORS)]))
    return series


def write_svg_chart(
    path: Path,
    title: str,
    x_values: list[float],
    series: list[tuple[str, list[float], str]],
    unit: str,
) -> None:
    width = 900
    height = 320
    left = 70
    right = 25
    top = 30
    bottom = 55
    plot_width = width - left - right
    plot_height = height - top - bottom
    x_min = min(x_values)
    x_max = max(x_values)
    if math.isclose(x_min, x_max):
        x_max = x_min + 1.0
    all_values = [value for _, values, _ in series for value in values]
    value_min = min(all_values)
    value_max = max(all_values)
    if math.isclose(value_min, value_max):
        padding = 1.0 if math.isclose(value_min, 0.0) else abs(value_min) * 0.1
        value_min -= padding
        value_max += padding

    def x_coord(value: float) -> float:
        return left + (value - x_min) / (x_max - x_min) * plot_width

    def y_coord(value: float) -> float:
        return top + (value_max - value) / (value_max - value_min) * plot_height

    polylines: list[str] = []
    legend: list[str] = []
    for index, (name, values, color) in enumerate(series):
        points = " ".join(
            f"{x_coord(x_value):.2f},{y_coord(value):.2f}"
            for x_value, value in zip(x_values, values, strict=True)
        )
        legend_y = top + 14 + index * 18
        polylines.append(
            f'<polyline fill="none" stroke="{color}" stroke-width="2.5" points="{points}" />'
        )
        legend.append(
            f'<text x="{left + 10}" y="{legend_y}" font-size="12" fill="{color}">'
            f"{escape(name)}</text>"
        )

    svg = f"""<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">
  <rect width="100%" height="100%" fill="#ffffff" />
  <text x="{left}" y="22" font-size="18" font-family="sans-serif">{escape(title)}</text>
  <line x1="{left}" y1="{top + plot_height}" x2="{left + plot_width}" y2="{top + plot_height}" stroke="#333" />
  <line x1="{left}" y1="{top}" x2="{left}" y2="{top + plot_height}" stroke="#333" />
  <text x="{left + plot_width / 2:.2f}" y="{height - 15}" text-anchor="middle" font-size="12" font-family="sans-serif">aligned sample</text>
  <text x="16" y="{top + plot_height / 2:.2f}" transform="rotate(-90 16 {top + plot_height / 2:.2f})" text-anchor="middle" font-size="12" font-family="sans-serif">{escape(unit)}</text>
  {"".join(polylines)}
  {"".join(legend)}
</svg>
"""
    path.write_text(svg, encoding="utf-8")


def write_plots(
    output_dir: Path,
    runs: list[RunBundle],
    common_keys: list[str],
    per_run: dict[str, dict[str, dict[str, Any]]],
) -> list[str]:
    x_values = [float(index) for index, _ in enumerate(common_keys)]
    plots = [
        (
            "boat_speed.svg",
            "Boat Speed",
            series_for_channel(runs, common_keys, per_run, "boat_speed_mps"),
            "m/s",
        ),
        (
            "blade_loads.svg",
            "Blade Loads",
            series_for_channel(runs, common_keys, per_run, "port_blade_force_n")
            + series_for_channel(runs, common_keys, per_run, "starboard_blade_force_n"),
            "N",
        ),
        (
            "energy_power.svg",
            "Energy and Power",
            series_for_channel(runs, common_keys, per_run, "stroke_power_w")
            + series_for_channel(runs, common_keys, per_run, "blade_power_w"),
            "W",
        ),
    ]
    generated: list[str] = []
    for filename, title, series, unit in plots:
        write_svg_chart(output_dir / filename, title, x_values, series, unit)
        generated.append(filename)
    return generated


def run_metadata(run: RunBundle) -> dict[str, Any]:
    metadata = run.metadata
    return {
        "id": run.id,
        "label": run.label,
        "role": run.role,
        "config_id": str(run.summary.get("config_id", "")),
        "summary_path": str(run.summary_path),
        "time_series_path": str(run.time_series_path),
        "visualization_path": str(run.visualization_path) if run.visualization_path else None,
        "summary_schema_version": schema_version(run.summary),
        "time_series_schema_version": schema_version(run.time_series),
        "providers": metadata.get("providers", {}),
        "backends": {
            "mechanics": metadata.get("mechanics_backend", {}),
            "integration": metadata.get("integration_backend", {}),
            "ids": backend_ids(metadata),
        },
        "calibration_provenance": calibration_provenance(metadata),
        "trust_envelope": trust_envelope(metadata),
    }


def metrics_document(
    manifest: dict[str, Any],
    runs: list[RunBundle],
    common_keys: list[str],
    per_run: dict[str, dict[str, dict[str, Any]]],
    generated_plots: list[str],
) -> dict[str, Any]:
    alignment = manifest["alignment"]
    return {
        "schema_id": REPORT_SCHEMA_ID,
        "comparison_id": manifest["comparison_id"],
        "alignment": {
            "mode": alignment["mode"],
            "start_time_s": float(alignment["start_time_s"]),
            "end_time_s": float(alignment["end_time_s"]),
            "overlapping_sample_count": len(common_keys),
            "aligned_keys": common_keys,
        },
        "runs": [run_metadata(run) for run in runs],
        "headline_deltas": headline_deltas(runs, common_keys, per_run),
        "differences": {
            "config": config_differences(runs),
            "providers": keyed_differences(runs, "providers"),
            "backends": keyed_differences(runs, "backends"),
            "calibration": keyed_differences(runs, "calibration"),
        },
        "comparability": comparability(runs, common_keys),
        "channel_coverage": channel_coverage(runs),
        "generated_plots": generated_plots,
    }


def render_delta_rows(deltas: dict[str, Any]) -> str:
    rows = []
    for metric, delta in sorted(deltas.items()):
        rows.append(
            "<tr>"
            f"<td>{escape(metric)}</td>"
            f"<td>{float(delta['baseline']):.6g}</td>"
            f"<td>{float(delta['comparison']):.6g}</td>"
            f"<td>{float(delta['delta']):.6g} {escape(str(delta['unit']))}</td>"
            "</tr>"
        )
    return "\n".join(rows)


def render_run_rows(runs: list[dict[str, Any]]) -> str:
    rows = []
    for run in runs:
        trust = run.get("trust_envelope", {})
        rows.append(
            "<tr>"
            f"<td>{escape(str(run['id']))}</td>"
            f"<td>{escape(str(run['label']))}</td>"
            f"<td>{escape(str(run['role']))}</td>"
            f"<td>{escape(str(run['config_id']))}</td>"
            f"<td>{escape(str(trust.get('fidelity_tier', 'unknown')))}</td>"
            "</tr>"
        )
    return "\n".join(rows)


def render_html(metrics: dict[str, Any]) -> str:
    comparability_data = metrics["comparability"]
    coverage = metrics["channel_coverage"]
    vector_items = "".join(
        f"<li>{escape(vector_id)}</li>"
        for vector_id in coverage.get("visualization_vectors", {})
    )
    unavailable_items = "".join(
        f"<li>{escape(channel)}: {escape(reason)}</li>"
        for channel, reason in coverage.get("unavailable_reasons", {}).items()
    )
    plot_images = "\n".join(
        f'<figure><img src="{escape(plot)}" alt="{escape(plot)}" /></figure>'
        for plot in metrics.get("generated_plots", [])
    )
    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <title>AIRow Run Comparison</title>
  <style>
    body {{ font-family: sans-serif; margin: 2rem; color: #1f2933; }}
    table {{ border-collapse: collapse; margin: 1rem 0; width: 100%; }}
    th, td {{ border: 1px solid #c8d0d8; padding: 0.45rem; text-align: left; }}
    th {{ background: #eef2f6; }}
    img {{ max-width: 100%; height: auto; border: 1px solid #d7dee6; }}
    .flag {{ font-weight: 700; }}
  </style>
</head>
<body data-airow-comparison-report>
  <h1>Run Comparison</h1>
  <p>Comparison id: {escape(str(metrics["comparison_id"]))}</p>
  <h2>Runs</h2>
  <table>
    <thead><tr><th>ID</th><th>Label</th><th>Role</th><th>Config</th><th>Fidelity</th></tr></thead>
    <tbody>{render_run_rows(metrics["runs"])}</tbody>
  </table>
  <h2>Comparability</h2>
  <p class="flag">Software: {comparability_data["software_comparable"]}</p>
  <p class="flag">Calibration: {comparability_data["calibration_comparable"]}</p>
  <p class="flag">Physical: {comparability_data["physical_comparable"]}</p>
  <pre>{escape(json.dumps(comparability_data, indent=2, sort_keys=True))}</pre>
  <h2>Headline Metric Deltas</h2>
  <table>
    <thead><tr><th>Metric</th><th>Baseline</th><th>Comparison</th><th>Delta</th></tr></thead>
    <tbody>{render_delta_rows(metrics["headline_deltas"])}</tbody>
  </table>
  <h2>Aligned Plots</h2>
  {plot_images}
  <h2>Vector Channel Coverage</h2>
  <ul>{vector_items}</ul>
  <h2>Unavailable Channels</h2>
  <ul>{unavailable_items}</ul>
</body>
</html>
"""


def generate_report(manifest_path: Path, output_dir: Path) -> None:
    manifest = load_json(manifest_path, "comparison manifest")
    validate_manifest(manifest)
    runs = [RunBundle(run) for run in manifest["runs"]]
    common_keys, per_run = aligned_records(runs, manifest["alignment"])

    output_dir.mkdir(parents=True, exist_ok=True)
    generated_plots = write_plots(output_dir, runs, common_keys, per_run)
    metrics = metrics_document(manifest, runs, common_keys, per_run, generated_plots)
    (output_dir / "metrics.json").write_text(
        json.dumps(metrics, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    (output_dir / "index.html").write_text(render_html(metrics), encoding="utf-8")


def main() -> int:
    args = parse_args()
    try:
        generate_report(Path(args.manifest), Path(args.output_dir))
    except RuntimeError as exc:
        print("invalid comparison manifest or artifacts", file=sys.stderr)
        print(f"- {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
