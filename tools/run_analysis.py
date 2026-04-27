#!/usr/bin/env python3
"""Generate a static HTML and SVG analysis bundle from simulator JSON artifacts."""

from __future__ import annotations

import argparse
from html import escape
import json
import math
from pathlib import Path
import sys
from typing import Iterable

from validate_visualization_artifact import validate_document


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a static run-analysis report bundle from simulator JSON artifacts."
    )
    parser.add_argument("--summary", required=True, help="Path to summary JSON artifact")
    parser.add_argument(
        "--time-series", required=True, help="Path to time-series JSON artifact"
    )
    parser.add_argument(
        "--visualization",
        help="Optional path to airow.visualization.v1 JSON for interactive inspection",
    )
    parser.add_argument(
        "--output-dir", required=True, help="Directory where the report bundle will be written"
    )
    return parser.parse_args()


def load_json(path: Path) -> dict:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise RuntimeError(f"missing artifact: {path}") from exc
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"failed to parse JSON artifact {path}: {exc}") from exc


def load_visualization(path: Path) -> dict:
    visualization = load_json(path)
    issues = validate_document(visualization)
    if issues:
        joined = "; ".join(issues[:4])
        if len(issues) > 4:
            joined += f"; {len(issues) - 4} more issue(s)"
        raise RuntimeError(f"invalid visualization artifact {path}: {joined}")
    return visualization


def nested_value(mapping: dict, *keys: str) -> dict | list | float | str:
    value: dict | list | float | str = mapping
    for key in keys:
        if not isinstance(value, dict) or key not in value:
            joined = ".".join(keys)
            raise RuntimeError(f"missing expected field '{joined}' in time-series record")
        value = value[key]
    return value


def scalar_channel_value(record: dict, *keys: str) -> float:
    value = nested_value(record, *keys)
    if not isinstance(value, dict) or "value" not in value:
        joined = ".".join(keys)
        raise RuntimeError(f"expected scalar channel at '{joined}'")
    return float(value["value"])


def vector_channel_value(record: dict, *keys: str) -> list[float]:
    value = nested_value(record, *keys)
    if not isinstance(value, dict) or "vector" not in value:
        joined = ".".join(keys)
        raise RuntimeError(f"expected vector channel at '{joined}'")
    vector = value["vector"]
    if not isinstance(vector, dict) or "value" not in vector:
        joined = ".".join(keys)
        raise RuntimeError(f"expected vector payload at '{joined}'")
    components = vector["value"]
    if not isinstance(components, list) or len(components) != 3:
        joined = ".".join(keys)
        raise RuntimeError(f"expected 3-vector at '{joined}'")
    return [float(component) for component in components]


def magnitude(components: Iterable[float]) -> float:
    return math.sqrt(sum(component * component for component in components))


def write_svg_chart(
    path: Path,
    title: str,
    times: list[float],
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
    time_min = min(times)
    time_max = max(times)
    if math.isclose(time_min, time_max):
        time_max = time_min + 1.0

    all_values = [value for _, values, _ in series for value in values]
    value_min = min(all_values)
    value_max = max(all_values)
    if math.isclose(value_min, value_max):
        padding = 1.0 if math.isclose(value_min, 0.0) else abs(value_min) * 0.1
        value_min -= padding
        value_max += padding

    def x_coord(time_value: float) -> float:
        return left + (time_value - time_min) / (time_max - time_min) * plot_width

    def y_coord(value: float) -> float:
        return top + (value_max - value) / (value_max - value_min) * plot_height

    polyline_parts: list[str] = []
    legend_parts: list[str] = []
    for index, (name, values, color) in enumerate(series):
        points = " ".join(
            f"{x_coord(time_value):.2f},{y_coord(value):.2f}"
            for time_value, value in zip(times, values, strict=True)
        )
        legend_y = top + 14 + index * 18
        polyline_parts.append(
            f'<polyline fill="none" stroke="{color}" stroke-width="2.5" points="{points}" />'
        )
        legend_parts.append(
            f'<rect x="{width - 190}" y="{legend_y - 10}" width="12" height="12" fill="{color}" />'
            f'<text x="{width - 172}" y="{legend_y}" font-size="12">{escape(name)}</text>'
        )

    grid_parts: list[str] = []
    label_parts: list[str] = []
    for tick in range(5):
        ratio = tick / 4.0
        y = top + ratio * plot_height
        value = value_max - ratio * (value_max - value_min)
        grid_parts.append(
            f'<line x1="{left}" y1="{y:.2f}" x2="{width - right}" y2="{y:.2f}" stroke="#d9d9d9" stroke-width="1" />'
        )
        label_parts.append(
            f'<text x="{left - 10}" y="{y + 4:.2f}" text-anchor="end" font-size="11">{value:.2f}</text>'
        )
    for tick in range(5):
        ratio = tick / 4.0
        x = left + ratio * plot_width
        time_value = time_min + ratio * (time_max - time_min)
        grid_parts.append(
            f'<line x1="{x:.2f}" y1="{top}" x2="{x:.2f}" y2="{height - bottom}" stroke="#eeeeee" stroke-width="1" />'
        )
        label_parts.append(
            f'<text x="{x:.2f}" y="{height - bottom + 18}" text-anchor="middle" font-size="11">{time_value:.2f}</text>'
        )

    svg = f"""<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">
  <rect width="{width}" height="{height}" fill="#fcfbf7" />
  <text x="{left}" y="20" font-size="18" font-weight="600">{escape(title)}</text>
  <text x="{left}" y="{height - 12}" font-size="12">time [s]</text>
  <text x="18" y="{top - 6}" font-size="12">{escape(unit)}</text>
  <rect x="{left}" y="{top}" width="{plot_width}" height="{plot_height}" fill="#ffffff" stroke="#222222" stroke-width="1" />
  {''.join(grid_parts)}
  {''.join(polyline_parts)}
  {''.join(label_parts)}
  {''.join(legend_parts)}
</svg>
"""
    path.write_text(svg, encoding="utf-8")


def flatten_metrics(prefix: str, value: object, rows: list[tuple[str, str]]) -> None:
    if isinstance(value, dict):
        if "value" in value and len(value) <= 3:
            rows.append((prefix, str(value["value"])))
            return
        for key, child in value.items():
            child_prefix = f"{prefix}.{key}" if prefix else key
            flatten_metrics(child_prefix, child, rows)
        return
    rows.append((prefix, str(value)))


def render_metrics_table(analysis: dict) -> str:
    rows: list[tuple[str, str]] = []
    flatten_metrics("", analysis, rows)
    table_rows = "\n".join(
        f"<tr><th>{escape(key)}</th><td>{escape(value)}</td></tr>" for key, value in rows
    )
    return (
        '<table class="metrics"><thead><tr><th>Metric</th><th>Value</th></tr></thead>'
        f"<tbody>{table_rows}</tbody></table>"
    )


def script_json(value: object) -> str:
    return json.dumps(value, separators=(",", ":")).replace("</", "<\\/")


def vector_channel_metadata_id(vector_id: str) -> str:
    replacements = {
        "hull_hydro_force_world_n": "hull_hydro_force",
        "hull_hydro_moment_world_n_m": "hull_hydro_moment",
        "port_blade_force_world_n": "blade_force",
        "starboard_blade_force_world_n": "blade_force",
        "aero_force_world_n": "aero_force",
        "aero_moment_world_n_m": "aero_moment",
        "ambient_wind_world_mps": "ambient_wind",
        "apparent_wind_world_mps": "apparent_wind",
        "rower_inertial_force_world_n": "rower_inertial_force",
    }
    return replacements.get(vector_id, vector_id)


def display_label(identifier: str) -> str:
    return identifier.replace("_", " ")


def vector_controls(visualization: dict | None) -> list[dict]:
    if visualization is None:
        return []

    channels = visualization.get("channels", {})
    samples = visualization.get("samples", [])
    first_sample = samples[0] if samples and isinstance(samples[0], dict) else {}
    sample_vectors = first_sample.get("vectors", {})
    controls: list[dict] = []

    if isinstance(sample_vectors, dict):
        for vector_id, vector in sorted(sample_vectors.items()):
            if not isinstance(vector, dict):
                continue
            metadata_id = vector_channel_metadata_id(vector_id)
            metadata = channels.get(metadata_id, {})
            if not isinstance(metadata, dict):
                metadata = {}
            controls.append(
                {
                    "id": vector_id,
                    "label": display_label(vector_id),
                    "availability": "available",
                    "unit": str(vector.get("unit", metadata.get("unit", ""))),
                    "frame": str(vector.get("frame", metadata.get("frame", ""))),
                    "provenance": str(metadata.get("provenance", "emitted_sample")),
                    "metadata_channel": metadata_id,
                }
            )

    return controls


def unavailable_channel_controls(visualization: dict | None) -> list[dict]:
    if visualization is None:
        return []

    unavailable: list[dict] = []
    channels = visualization.get("channels", {})
    if isinstance(channels, dict):
        for channel_id, metadata in sorted(channels.items()):
            if not isinstance(metadata, dict):
                continue
            if metadata.get("availability") != "unavailable":
                continue
            unavailable.append(
                {
                    "id": channel_id,
                    "label": display_label(channel_id),
                    "availability": "unavailable",
                    "unit": str(metadata.get("unit", "")),
                    "frame": str(metadata.get("frame", "")),
                    "reason": str(metadata.get("reason", "unavailable")),
                }
            )
    return unavailable


def reference_frame_ids(visualization: dict | None) -> list[str]:
    if visualization is None:
        return []
    frames = visualization.get("frames", {})
    if not isinstance(frames, dict):
        return []
    preferred = ["world", "hull_body", "waterline"]
    return [frame for frame in preferred if frame in frames]


def interactive_controls_metadata(
    visualization: dict | None,
    plot_channels: list[dict],
    markers: list[dict] | None = None,
) -> dict:
    return {
        "enabled": visualization is not None,
        "schema_id": visualization.get("schema_id") if visualization else None,
        "projection_modes": ["top", "side", "end"] if visualization else [],
        "reference_frames": reference_frame_ids(visualization),
        "plot_click_seek": visualization is not None,
        "vector_channels": vector_controls(visualization),
        "unavailable_channels": unavailable_channel_controls(visualization),
        "plot_channels": [plot_channel_metadata(channel) for channel in plot_channels],
        "event_markers": markers or [],
    }


def plot_channel_metadata(channel: dict) -> dict:
    return {
        "id": channel["id"],
        "label": channel["label"],
        "unit": channel["unit"],
        "frame": channel.get("frame", ""),
        "availability": "available",
        "source": channel.get("source", "time_series"),
    }


def marker_time(records: list[dict], sample_index: int) -> float:
    if 0 <= sample_index < len(records):
        return float(records[sample_index].get("time_s", 0.0))
    return 0.0


def event_marker(
    marker_id: str,
    marker_type: str,
    label: str,
    records: list[dict],
    sample_index: int,
    channel: dict,
    value: object,
) -> dict:
    return {
        "id": marker_id,
        "type": marker_type,
        "label": label,
        "time_s": marker_time(records, sample_index),
        "sample_index": sample_index,
        "channel_id": channel.get("id", ""),
        "value": value,
        "unit": channel.get("unit", ""),
        "frame": channel.get("frame", ""),
        "source": channel.get("source", ""),
    }


def first_zero_crossing(values: list[float]) -> int | None:
    for index, value in enumerate(values):
        if math.isclose(value, 0.0, abs_tol=1e-12):
            return index
    for index in range(1, len(values)):
        if values[index - 1] * values[index] < 0.0:
            return index
    return None


def channel_event_markers(records: list[dict], channel: dict) -> list[dict]:
    values = [float(value) for value in channel.get("values", [])]
    if not values:
        return []

    peak_index = max(range(len(values)), key=lambda index: abs(values[index]))
    markers = [
        event_marker(
            f"peak_value_{channel['id']}",
            "peak_value",
            f"Peak {channel['label']}",
            records,
            peak_index,
            channel,
            values[peak_index],
        )
    ]
    zero_index = first_zero_crossing(values)
    if zero_index is not None:
        markers.append(
            event_marker(
                f"zero_crossing_{channel['id']}",
                "zero_crossing",
                f"Zero crossing {channel['label']}",
                records,
                zero_index,
                channel,
                values[zero_index],
            )
        )
    return markers


def stroke_event_markers(records: list[dict]) -> list[dict]:
    markers: list[dict] = []
    previous_phase = ""
    channel = {
        "id": "stroke_input.phase",
        "unit": "",
        "frame": "",
        "source": "time_series.stroke_input.phase",
    }
    for index, record in enumerate(records):
        phase = str(record.get("stroke_input", {}).get("phase", ""))
        if index == 0 or (phase and phase != previous_phase):
            markers.append(
                event_marker(
                    f"stroke_boundary_{index}",
                    "stroke_boundary",
                    f"Stroke phase {phase or 'unknown'}",
                    records,
                    index,
                    channel,
                    phase,
                )
            )
        previous_phase = phase
    return markers


def trust_warning_markers(summary: dict, records: list[dict]) -> list[dict]:
    trust = summary.get("metadata", {}).get("trust_envelope", {})
    warnings = trust.get("warnings", [])
    if not isinstance(warnings, list):
        return []
    channel = {
        "id": "metadata.trust_envelope.warnings",
        "unit": "",
        "frame": "",
        "source": "summary.metadata.trust_envelope.warnings",
    }
    return [
        event_marker(
            f"trust_warning_{index}",
            "trust_warning",
            "Trust warning",
            records,
            0,
            channel,
            str(warning),
        )
        for index, warning in enumerate(warnings)
    ]


def event_markers(summary: dict, records: list[dict], plot_channels: list[dict]) -> list[dict]:
    markers: list[dict] = []
    for channel in plot_channels:
        markers.extend(channel_event_markers(records, channel))
    markers.extend(stroke_event_markers(records))
    markers.extend(trust_warning_markers(summary, records))
    return sorted(markers, key=lambda marker: (marker["time_s"], marker["id"]))


def viewer_payload(
    summary: dict,
    time_series: dict,
    visualization: dict | None,
    generated_plots: list[str],
    plot_channels: list[dict],
    markers: list[dict],
) -> dict:
    records = time_series.get("records", [])
    return {
        "enabled": visualization is not None,
        "summary": {
            "config_id": summary.get("config_id", "unknown"),
            "status": summary.get("status", "unknown"),
            "simulator_version": summary.get("simulator_version", "unknown"),
            "trust_envelope": summary.get("metadata", {}).get("trust_envelope", {}),
            "providers": summary.get("metadata", {}).get("providers", {}),
        },
        "records": records if isinstance(records, list) else [],
        "visualization": visualization,
        "plots": generated_plots,
        "plot_channels": plot_channels,
        "controls": interactive_controls_metadata(visualization, plot_channels, markers),
    }


def render_interactive_viewer(
    visualization: dict | None,
    plot_channels: list[dict],
    markers: list[dict],
) -> str:
    if visualization is None:
        return ""

    controls = interactive_controls_metadata(visualization, plot_channels, markers)
    vector_markup = "\n".join(
        '<label class="control-option">'
        f'<input id="vector-toggle-{escape(channel["id"])}" '
        f'type="checkbox" value="{escape(channel["id"])}" '
        'data-availability="available" checked />'
        f'<span>{escape(channel["label"])} '
        f'[{escape(channel["unit"])} {escape(channel["frame"])}]</span>'
        "</label>"
        for channel in controls["vector_channels"]
    )
    unavailable_vector_markup = "\n".join(
        '<label class="control-option unavailable">'
        f'<input id="vector-toggle-{escape(channel["id"])}" '
        f'type="checkbox" value="{escape(channel["id"])}" '
        'data-availability="unavailable" disabled />'
        f'<span>{escape(channel["label"])} unavailable: '
        f'{escape(channel["reason"])}</span>'
        "</label>"
        for channel in controls["unavailable_channels"]
    )
    projection_options = "\n".join(
        f'<option id="projection-{escape(mode)}" value="{escape(mode)}">'
        f"{escape(mode.title())}</option>"
        for mode in controls["projection_modes"]
    )
    frame_options = "\n".join(
        f'<option value="{escape(frame)}">{escape(frame)}</option>'
        for frame in controls["reference_frames"]
    )
    plot_options = "\n".join(
        f'<option value="{escape(channel["id"])}" selected>'
        f'{escape(channel["label"])} [{escape(channel["unit"])}]</option>'
        for channel in controls["plot_channels"]
    )
    plot_canvases = "\n".join(
        f'<canvas class="linked-plot" data-channel="{escape(channel["id"])}" '
        'width="720" height="180"></canvas>'
        for channel in controls["plot_channels"]
    )
    unavailable = [
        f"{channel['id']}: {channel['reason']}"
        for channel in controls["unavailable_channels"]
    ]
    unavailable_markup = "\n".join(
        f"<li>{escape(item)}</li>" for item in unavailable
    ) or "<li>none</li>"
    event_markup = "\n".join(
        '<li>'
        f'<button type="button" class="event-marker" '
        f'data-event-seek="{int(marker["sample_index"])}">'
        f'{escape(marker["label"])} '
        f'({escape(marker["type"])}) '
        f'@ {float(marker["time_s"]):.3f} s'
        "</button>"
        "</li>"
        for marker in controls["event_markers"]
    ) or "<li>none</li>"

    return f"""
    <section class="viewer" data-airow-viewer>
      <div class="viewer-toolbar">
        <button id="playback-toggle" type="button">Play</button>
        <button id="step-back" type="button">Prev</button>
        <button id="step-forward" type="button">Next</button>
        <input id="timeline" type="range" min="0" max="0" value="0" step="1" />
        <div class="readout"><strong id="viewer-time">0.000 s</strong><span id="viewer-phase">phase</span></div>
      </div>
      <div class="control-grid">
        <label>Projection
          <select id="projection-mode">{projection_options}</select>
        </label>
        <label>Reference Frame
          <select id="reference-frame">{frame_options}</select>
        </label>
        <label>Plot Channels
          <select id="plot-channel-select" multiple size="6">{plot_options}</select>
        </label>
      </div>
      <div class="vector-controls" aria-label="Vector overlays">
        {vector_markup}
        {unavailable_vector_markup}
      </div>
      <div class="viewer-grid">
        <div>
          <h2>Top Projection</h2>
          <canvas id="projection-top" width="720" height="320"></canvas>
        </div>
        <div>
          <h2>Side Projection</h2>
          <canvas id="projection-side" width="720" height="320"></canvas>
        </div>
        <div>
          <h2>End Projection</h2>
          <canvas id="projection-end-canvas" width="720" height="320"></canvas>
        </div>
      </div>
      <div class="status-row">
        <div id="trust-status">trust-status</div>
        <div id="vector-status">hull_hydro_force_world_n</div>
        <div id="frame-status">world frame</div>
      </div>
      <details open>
        <summary>Unavailable Channels</summary>
        <ul>{unavailable_markup}</ul>
      </details>
      <details open id="event-marker-list">
        <summary>Event Markers</summary>
        <ul>{event_markup}</ul>
      </details>
      <script type="application/json" id="metrics_metadata">{script_json(controls)}</script>
      <div class="plot-board" id="plot-board" data-plot-click-seek="true">
        {plot_canvases}
        <span class="plot-cursor" aria-hidden="true"></span>
      </div>
    </section>
"""


def write_html_report(
    path: Path,
    summary: dict,
    metrics: dict,
    plot_files: list[str],
    plot_channels: list[dict],
    time_series: dict,
    visualization: dict | None = None,
    markers: list[dict] | None = None,
) -> None:
    analysis = summary.get("analysis", {})
    metric_table = render_metrics_table(analysis if isinstance(analysis, dict) else {})
    event_marker_data = markers or []
    interactive_markup = render_interactive_viewer(
        visualization, plot_channels, event_marker_data
    )
    payload = viewer_payload(
        summary, time_series, visualization, plot_files, plot_channels, event_marker_data
    )
    plot_markup = "\n".join(
        f'<section class="plot-card"><h2>{escape(plot_file[:-4].replace("_", " ").title())}</h2>'
        f'<img src="{escape(plot_file)}" alt="{escape(plot_file)}" /></section>'
        for plot_file in plot_files
    )
    html = f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <title>Run Analysis - {escape(str(summary.get("config_id", "unknown")))}</title>
  <style>
    :root {{
      color-scheme: light;
      --bg: #f7f8fa;
      --panel: #ffffff;
      --text: #172026;
      --muted: #60707d;
      --accent: #145a7a;
      --port: #b34d3d;
      --starboard: #266f4d;
      --border: #d8e0e6;
    }}
    body {{
      margin: 0;
      font-family: "Iosevka Aile", "IBM Plex Sans", sans-serif;
      background: var(--bg);
      color: var(--text);
    }}
    main {{
      max-width: 1100px;
      margin: 0 auto;
      padding: 32px 24px 48px;
    }}
    .hero, .plot-card, .viewer {{
      background: var(--panel);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 20px 22px;
      box-shadow: 0 6px 18px rgba(20, 30, 40, 0.06);
      margin-bottom: 18px;
    }}
    h1, h2 {{
      margin: 0 0 12px;
    }}
    .meta {{
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
      gap: 12px;
      margin-bottom: 18px;
    }}
    .meta div {{
      padding: 10px 12px;
      border-radius: 8px;
      background: #f1f6f8;
    }}
    .viewer-toolbar {{
      display: grid;
      grid-template-columns: auto auto auto minmax(180px, 1fr) minmax(150px, auto);
      gap: 10px;
      align-items: center;
      margin-bottom: 14px;
    }}
    .control-grid {{
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
      gap: 10px;
      margin-bottom: 14px;
    }}
    .control-grid label, .control-option {{
      display: grid;
      gap: 4px;
      color: var(--muted);
      font-size: 13px;
    }}
    .control-option {{
      grid-template-columns: 18px minmax(0, 1fr);
      align-items: start;
      color: var(--text);
    }}
    .control-option.unavailable {{
      color: var(--muted);
    }}
    .vector-controls {{
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
      gap: 8px 14px;
      margin-bottom: 14px;
    }}
    button {{
      min-height: 34px;
      border: 1px solid var(--border);
      border-radius: 8px;
      background: #edf3f6;
      color: var(--text);
      font: inherit;
      cursor: pointer;
    }}
    input[type="range"] {{
      width: 100%;
    }}
    select {{
      min-height: 34px;
      border: 1px solid var(--border);
      border-radius: 8px;
      background: white;
      color: var(--text);
      font: inherit;
      padding: 4px 8px;
    }}
    .readout {{
      display: flex;
      justify-content: space-between;
      gap: 12px;
      color: var(--muted);
      white-space: nowrap;
    }}
    .viewer-grid {{
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
      gap: 14px;
    }}
    canvas {{
      width: 100%;
      max-width: 100%;
      height: auto;
      display: block;
      background: #fbfdff;
      border: 1px solid var(--border);
      border-radius: 8px;
    }}
    .status-row {{
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
      gap: 10px;
      margin: 14px 0;
      color: var(--muted);
      font-size: 14px;
    }}
    .plot-board {{
      display: grid;
      gap: 10px;
      position: relative;
    }}
    .plot-cursor {{
      display: none;
    }}
    table {{
      width: 100%;
      border-collapse: collapse;
      font-size: 14px;
    }}
    th, td {{
      text-align: left;
      padding: 8px 10px;
      border-bottom: 1px solid var(--border);
      vertical-align: top;
    }}
    th {{
      width: 40%;
    }}
    img {{
      width: 100%;
      height: auto;
      display: block;
      border-radius: 12px;
      background: white;
    }}
  </style>
</head>
<body>
  <main>
    <section class="hero">
      <h1>Run Analysis</h1>
      <div class="meta">
        <div><strong>Config</strong><br />{escape(str(metrics["config_id"]))}</div>
        <div><strong>Status</strong><br />{escape(str(metrics["status"]))}</div>
        <div><strong>Version</strong><br />{escape(str(metrics["simulator_version"]))}</div>
        <div><strong>Records</strong><br />{escape(str(metrics["record_count"]))}</div>
      </div>
      {metric_table}
    </section>
    {interactive_markup}
    {plot_markup}
  </main>
  <script>
    const airowViewerData = {script_json(payload)};
    (() => {{
      if (!airowViewerData.enabled || !airowViewerData.visualization) {{
        return;
      }}
      const samples = airowViewerData.visualization.samples || [];
      const timeline = document.getElementById("timeline");
      const playButton = document.getElementById("playback-toggle");
      const timeReadout = document.getElementById("viewer-time");
      const phaseReadout = document.getElementById("viewer-phase");
      const trustStatus = document.getElementById("trust-status");
      const vectorStatus = document.getElementById("vector-status");
      const frameStatus = document.getElementById("frame-status");
      const topCanvas = document.getElementById("projection-top");
      const sideCanvas = document.getElementById("projection-side");
      const endCanvas = document.getElementById("projection-end-canvas");
      const projectionMode = document.getElementById("projection-mode");
      const referenceFrame = document.getElementById("reference-frame");
      const plotChannelSelect = document.getElementById("plot-channel-select");
      const vectorInputs = Array.from(document.querySelectorAll(".vector-controls input"));
      const plotCanvases = Array.from(document.querySelectorAll(".linked-plot"));
      const eventButtons = Array.from(document.querySelectorAll("[data-event-seek]"));
      const plotChannelLookup = new Map((airowViewerData.plot_channels || []).map((channel) => [channel.id, channel]));
      const eventMarkers = airowViewerData.controls?.event_markers || [];
      const vectorColors = ["#0b5870", "#b34d3d", "#266f4d", "#8b3f6b", "#7b5b1f", "#4d6480"];
      let frameIndex = 0;
      let timer = 0;

      timeline.max = String(Math.max(0, samples.length - 1));
      const trust = airowViewerData.summary.trust_envelope || {{}};
      trustStatus.textContent = `trust-status: ${{trust.fidelity_tier || "unknown"}} / ${{trust.validity_status || "unknown"}}`;

      function component(vector, axis) {{
        if (!vector || !Array.isArray(vector.value)) return 0;
        return Number(vector.value[axis] || 0);
      }}

      function bounds() {{
        const xs = [];
        const ys = [];
        const zs = [];
        for (const sample of samples) {{
          const transforms = sample.transforms || {{}};
          const hull = transforms.hull || {{}};
          const pos = hull.position_world_m || [0, 0, 0];
          xs.push(Number(pos[0] || 0));
          ys.push(Number(pos[1] || 0));
          zs.push(Number(pos[2] || 0));
          for (const key of ["port_blade", "starboard_blade"]) {{
            const blade = transforms[key] || {{}};
            const tip = blade.tip_position_world_m || [0, 0, 0];
            xs.push(Number(tip[0] || 0));
            ys.push(Number(tip[1] || 0));
            zs.push(Number(tip[2] || 0));
          }}
        }}
        return {{
          xMin: Math.min(...xs, -1), xMax: Math.max(...xs, 1),
          yMin: Math.min(...ys, -1), yMax: Math.max(...ys, 1),
          zMin: Math.min(...zs, -0.5), zMax: Math.max(...zs, 0.5),
        }};
      }}

      const extent = bounds();
      function map(value, min, max, size, pad) {{
        if (Math.abs(max - min) < 1e-9) return size / 2;
        return pad + (value - min) / (max - min) * (size - 2 * pad);
      }}

      function projectedPosition(point, mode, width, height) {{
        if (mode === "end") {{
          return {{
            x: map(Number(point[1] || 0), extent.yMin, extent.yMax, width, 48),
            y: height - map(Number(point[2] || 0), extent.zMin, extent.zMax, height, 42),
          }};
        }}
        if (mode === "side") {{
          return {{
            x: map(Number(point[0] || 0), extent.xMin, extent.xMax, width, 48),
            y: height - map(Number(point[2] || 0), extent.zMin, extent.zMax, height, 42),
          }};
        }}
        return {{
          x: map(Number(point[0] || 0), extent.xMin, extent.xMax, width, 48),
          y: height - map(Number(point[1] || 0), extent.yMin, extent.yMax, height, 42),
        }};
      }}

      function projectedVector(vector, mode) {{
        if (mode === "end") {{
          return [component(vector, 1), component(vector, 2)];
        }}
        if (mode === "side") {{
          return [component(vector, 0), component(vector, 2)];
        }}
        return [component(vector, 0), component(vector, 1)];
      }}

      function selectedVectorIds() {{
        return vectorInputs
          .filter((input) => input.checked && !input.disabled)
          .map((input) => input.value);
      }}

      function selectedPlotIds() {{
        return Array.from(plotChannelSelect.selectedOptions).map((option) => option.value);
      }}

      function drawProjection(canvas, sample, mode) {{
        const ctx = canvas.getContext("2d");
        const w = canvas.width;
        const h = canvas.height;
        ctx.clearRect(0, 0, w, h);
        ctx.fillStyle = "#fbfdff";
        ctx.fillRect(0, 0, w, h);
        ctx.strokeStyle = "#d8e0e6";
        ctx.lineWidth = 1;
        for (let i = 0; i <= 4; i += 1) {{
          const x = 40 + i * (w - 80) / 4;
          const y = 30 + i * (h - 60) / 4;
          ctx.beginPath(); ctx.moveTo(x, 30); ctx.lineTo(x, h - 30); ctx.stroke();
          ctx.beginPath(); ctx.moveTo(40, y); ctx.lineTo(w - 40, y); ctx.stroke();
        }}
        const transforms = sample.transforms || {{}};
        const vectors = sample.vectors || {{}};
        const hull = transforms.hull || {{}};
        const hullPos = hull.position_world_m || [0, 0, 0];
        const hullPoint = projectedPosition(hullPos, mode, w, h);

        ctx.fillStyle = "#145a7a";
        ctx.fillRect(hullPoint.x - 24, hullPoint.y - 8, 48, 16);
        ctx.fillStyle = "#172026";
        ctx.fillText(`hull / ${{referenceFrame.value}} frame`, hullPoint.x - 34, hullPoint.y - 14);

        for (const [key, color] of [["port_blade", "#b34d3d"], ["starboard_blade", "#266f4d"]]) {{
          const blade = transforms[key] || {{}};
          const tip = blade.tip_position_world_m || [0, 0, 0];
          const bladePoint = projectedPosition(tip, mode, w, h);
          ctx.strokeStyle = color;
          ctx.lineWidth = 2;
          ctx.beginPath(); ctx.moveTo(hullPoint.x, hullPoint.y); ctx.lineTo(bladePoint.x, bladePoint.y); ctx.stroke();
          ctx.fillStyle = color;
          ctx.beginPath(); ctx.arc(bladePoint.x, bladePoint.y, 5, 0, Math.PI * 2); ctx.fill();
        }}

        const status = [];
        selectedVectorIds().forEach((vectorId, vectorIndex) => {{
          const vector = vectors[vectorId];
          if (!vector) return;
          const [vx, vy] = projectedVector(vector, mode);
          const scale = vector.unit === "m/s" ? 8.0 : 2.0;
          ctx.strokeStyle = vectorColors[vectorIndex % vectorColors.length];
          ctx.lineWidth = 3;
          ctx.beginPath();
          ctx.moveTo(hullPoint.x, hullPoint.y);
          ctx.lineTo(hullPoint.x + vx * scale, hullPoint.y - vy * scale);
          ctx.stroke();
          ctx.fillStyle = ctx.strokeStyle;
          ctx.fillText(`${{vectorId}} ${{vector.unit || ""}}/${{vector.frame || ""}}`, 48, 22 + vectorIndex * 16);
          status.push(`${{vectorId}}: [${{component(vector, 0).toFixed(2)}}, ${{component(vector, 1).toFixed(2)}}, ${{component(vector, 2).toFixed(2)}}] ${{vector.unit || ""}} ${{vector.frame || ""}}`);
        }});
        if (status.length) {{
          vectorStatus.textContent = status.join(" | ");
        }} else {{
          vectorStatus.textContent = "no available vector overlays selected";
        }}
      }}

      function drawPlot(canvas, channel, index) {{
        const spec = plotChannelLookup.get(channel) || {{}};
        const values = Array.isArray(spec.values) ? spec.values.map((value) => Number(value || 0)) : [];
        const ctx = canvas.getContext("2d");
        const min = Math.min(...values, 0);
        const max = Math.max(...values, 1);
        const w = canvas.width;
        const h = canvas.height;
        ctx.clearRect(0, 0, w, h);
        ctx.fillStyle = "#fbfdff";
        ctx.fillRect(0, 0, w, h);
        ctx.strokeStyle = "#d8e0e6";
        ctx.strokeRect(45, 16, w - 65, h - 48);
        ctx.strokeStyle = spec.color || "#266f4d";
        ctx.lineWidth = 2;
        ctx.beginPath();
        values.forEach((value, i) => {{
          const px = 45 + (i / Math.max(1, values.length - 1)) * (w - 65);
          const py = h - 32 - ((value - min) / Math.max(1e-9, max - min)) * (h - 48);
          if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
        }});
        ctx.stroke();
        eventMarkers
          .filter((marker) => marker.channel_id === channel)
          .forEach((marker) => {{
            const markerX = 45 + (Number(marker.sample_index || 0) / Math.max(1, values.length - 1)) * (w - 65);
            ctx.strokeStyle = "#7b5b1f";
            ctx.beginPath(); ctx.moveTo(markerX, 16); ctx.lineTo(markerX, h - 32); ctx.stroke();
          }});
        const cursorX = 45 + (index / Math.max(1, values.length - 1)) * (w - 65);
        ctx.strokeStyle = "#b34d3d";
        ctx.beginPath(); ctx.moveTo(cursorX, 16); ctx.lineTo(cursorX, h - 32); ctx.stroke();
        ctx.fillStyle = "#172026";
        ctx.fillText(`${{spec.label || channel}} [${{spec.unit || ""}} ${{spec.frame || ""}}]`, 48, 12);
      }}

      function updatePlotVisibility() {{
        const selected = new Set(selectedPlotIds());
        plotCanvases.forEach((canvas) => {{
          canvas.hidden = !selected.has(canvas.dataset.channel || "");
        }});
      }}

      function updateProjectionFocus() {{
        const selected = projectionMode.value;
        for (const [mode, canvas] of [["top", topCanvas], ["side", sideCanvas], ["end", endCanvas]]) {{
          canvas.style.outline = mode === selected ? "2px solid #145a7a" : "none";
        }}
      }}

      function render() {{
        if (!samples.length) return;
        frameIndex = Math.max(0, Math.min(frameIndex, samples.length - 1));
        timeline.value = String(frameIndex);
        const sample = samples[frameIndex];
        timeReadout.textContent = `${{Number(sample.time_s || 0).toFixed(3)}} s`;
        phaseReadout.textContent = String(sample.scalars?.stroke_phase || "phase");
        frameStatus.textContent = `${{referenceFrame.value}} frame selected; emitted vector frames are preserved`;
        updateProjectionFocus();
        updatePlotVisibility();
        drawProjection(topCanvas, sample, "top");
        drawProjection(sideCanvas, sample, "side");
        drawProjection(endCanvas, sample, "end");
        plotCanvases.forEach((canvas) => {{
          if (!canvas.hidden) drawPlot(canvas, canvas.dataset.channel || "", frameIndex);
        }});
      }}

      function stop() {{
        window.clearInterval(timer);
        timer = 0;
        playButton.textContent = "Play";
      }}

      playButton.addEventListener("click", () => {{
        if (timer) {{ stop(); return; }}
        playButton.textContent = "Pause";
        timer = window.setInterval(() => {{
          frameIndex = (frameIndex + 1) % Math.max(1, samples.length);
          render();
        }}, 200);
      }});
      document.getElementById("step-back").addEventListener("click", () => {{ stop(); frameIndex -= 1; render(); }});
      document.getElementById("step-forward").addEventListener("click", () => {{ stop(); frameIndex += 1; render(); }});
      timeline.addEventListener("input", () => {{ stop(); frameIndex = Number(timeline.value); render(); }});
      projectionMode.addEventListener("change", render);
      referenceFrame.addEventListener("change", render);
      plotChannelSelect.addEventListener("change", render);
      vectorInputs.forEach((input) => input.addEventListener("change", render));
      eventButtons.forEach((button) => {{
        button.addEventListener("click", () => {{
          stop();
          frameIndex = Number(button.dataset.eventSeek || 0);
          render();
        }});
      }});
      plotCanvases.forEach((canvas) => {{
        canvas.addEventListener("click", (event) => {{
          stop();
          const rect = canvas.getBoundingClientRect();
          const ratio = Math.max(0, Math.min(1, (event.clientX - rect.left) / Math.max(1, rect.width)));
          frameIndex = Math.round(ratio * Math.max(0, samples.length - 1));
          render();
        }});
      }});
      render();
    }})();
  </script>
</body>
</html>
"""
    path.write_text(html, encoding="utf-8")


def main() -> int:
    args = parse_args()
    summary_path = Path(args.summary)
    time_series_path = Path(args.time_series)
    output_dir = Path(args.output_dir)

    try:
        summary = load_json(summary_path)
        time_series = load_json(time_series_path)
        visualization = (
            load_visualization(Path(args.visualization))
            if args.visualization is not None
            else None
        )
        records = time_series.get("records")
        if not isinstance(records, list) or not records:
            raise RuntimeError("time-series artifact has no records")

        times = [float(record["time_s"]) for record in records]
        boat_speed = [scalar_channel_value(record, "boat_speed_mps") for record in records]
        seat_position = [
            scalar_channel_value(record, "seat_state", "position_along_rail_m")
            for record in records
        ]
        port_immersion = [
            scalar_channel_value(record, "blade_state", "port", "immersion_depth_m")
            for record in records
        ]
        starboard_immersion = [
            scalar_channel_value(record, "blade_state", "starboard", "immersion_depth_m")
            for record in records
        ]
        port_blade_load = [
            magnitude(vector_channel_value(record, "blade_load_world_n", "port"))
            for record in records
        ]
        starboard_blade_load = [
            magnitude(vector_channel_value(record, "blade_load_world_n", "starboard"))
            for record in records
        ]
        hull_position_z = [
            vector_channel_value(record, "hull_state", "position_world_m")[2]
            for record in records
        ]
        apparent_wind_speed = [
            magnitude(vector_channel_value(record, "apparent_wind_world_mps"))
            for record in records
        ]
        stroke_power = [scalar_channel_value(record, "stroke_power_w") for record in records]

        plot_channels = [
            {
                "id": "boat_speed_mps",
                "label": "Boat Speed",
                "unit": "m/s",
                "frame": "world",
                "source": "time_series.boat_speed_mps",
                "values": boat_speed,
                "color": "#145a7a",
            },
            {
                "id": "seat_position_m",
                "label": "Seat Position",
                "unit": "m",
                "frame": "hull_body",
                "source": "time_series.seat_state.position_along_rail_m",
                "values": seat_position,
                "color": "#4c7a34",
            },
            {
                "id": "port_blade_immersion_depth_m",
                "label": "Port Blade Immersion",
                "unit": "m",
                "frame": "blade",
                "source": "time_series.blade_state.port.immersion_depth_m",
                "values": port_immersion,
                "color": "#145a7a",
            },
            {
                "id": "starboard_blade_immersion_depth_m",
                "label": "Starboard Blade Immersion",
                "unit": "m",
                "frame": "blade",
                "source": "time_series.blade_state.starboard.immersion_depth_m",
                "values": starboard_immersion,
                "color": "#c05a2b",
            },
            {
                "id": "port_blade_force_n",
                "label": "Port Blade Force",
                "unit": "N",
                "frame": "world",
                "source": "time_series.blade_load_world_n.port",
                "values": port_blade_load,
                "color": "#145a7a",
            },
            {
                "id": "starboard_blade_force_n",
                "label": "Starboard Blade Force",
                "unit": "N",
                "frame": "world",
                "source": "time_series.blade_load_world_n.starboard",
                "values": starboard_blade_load,
                "color": "#c05a2b",
            },
            {
                "id": "hull_position_z_m",
                "label": "Hull Vertical Position",
                "unit": "m",
                "frame": "world",
                "source": "time_series.hull_state.position_world_m",
                "values": hull_position_z,
                "color": "#8b3f6b",
            },
            {
                "id": "apparent_wind_speed_mps",
                "label": "Apparent Wind Speed",
                "unit": "m/s",
                "frame": "world",
                "source": "time_series.apparent_wind_world_mps",
                "values": apparent_wind_speed,
                "color": "#c05a2b",
            },
            {
                "id": "stroke_power_w",
                "label": "Stroke Power",
                "unit": "W",
                "frame": "",
                "source": "time_series.stroke_power_w",
                "values": stroke_power,
                "color": "#4c7a34",
            },
        ]

        output_dir.mkdir(parents=True, exist_ok=True)
        plot_specs = [
            ("boat_speed.svg", "Boat Speed", [("boat_speed_mps", boat_speed, "#145a7a")], "m/s"),
            ("seat_position.svg", "Seat Position", [("seat_position_m", seat_position, "#4c7a34")], "m"),
            (
                "blade_immersion.svg",
                "Blade Immersion",
                [
                    ("port_blade_immersion_depth_m", port_immersion, "#145a7a"),
                    ("starboard_blade_immersion_depth_m", starboard_immersion, "#c05a2b"),
                ],
                "m",
            ),
            (
                "blade_loads.svg",
                "Blade Loads",
                [
                    ("port_blade_force_n", port_blade_load, "#145a7a"),
                    ("starboard_blade_force_n", starboard_blade_load, "#c05a2b"),
                ],
                "N",
            ),
            ("hull_position_z.svg", "Hull Vertical Position", [("hull_position_z_m", hull_position_z, "#8b3f6b")], "m"),
            (
                "apparent_wind_speed.svg",
                "Apparent Wind Speed",
                [("apparent_wind_speed_mps", apparent_wind_speed, "#c05a2b")],
                "m/s",
            ),
            ("stroke_power.svg", "Stroke Power", [("stroke_power_w", stroke_power, "#4c7a34")], "W"),
        ]
        generated_plots: list[str] = []
        for file_name, title, series, unit in plot_specs:
            write_svg_chart(output_dir / file_name, title, times, series, unit)
            generated_plots.append(file_name)

        markers = event_markers(summary, records, plot_channels)
        metrics = {
            "config_id": summary.get("config_id", "unknown"),
            "status": summary.get("status", "unknown"),
            "simulator_version": summary.get("simulator_version", "unknown"),
            "record_count": len(records),
            "generated_plots": generated_plots,
            "interactive_visualization": visualization is not None,
            "interactive_controls": interactive_controls_metadata(
                visualization, plot_channels, markers
            ),
            "analysis": summary.get("analysis", {}),
        }
        (output_dir / "metrics.json").write_text(
            json.dumps(metrics, indent=2) + "\n", encoding="utf-8"
        )
        write_html_report(
            output_dir / "index.html",
            summary,
            metrics,
            generated_plots,
            plot_channels,
            time_series,
            visualization,
            markers,
        )
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
