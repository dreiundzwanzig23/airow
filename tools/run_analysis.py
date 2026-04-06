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


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a static run-analysis report bundle from simulator JSON artifacts."
    )
    parser.add_argument("--summary", required=True, help="Path to summary JSON artifact")
    parser.add_argument(
        "--time-series", required=True, help="Path to time-series JSON artifact"
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


def write_html_report(
    path: Path,
    summary: dict,
    metrics: dict,
    plot_files: list[str],
) -> None:
    analysis = summary.get("analysis", {})
    metric_table = render_metrics_table(analysis if isinstance(analysis, dict) else {})
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
      --bg: #f6f2e8;
      --card: #fffdf8;
      --text: #1f241f;
      --accent: #145a7a;
      --border: #d9d0bf;
    }}
    body {{
      margin: 0;
      font-family: "Iosevka Aile", "IBM Plex Sans", sans-serif;
      background: linear-gradient(180deg, #f6f2e8 0%, #efe7d7 100%);
      color: var(--text);
    }}
    main {{
      max-width: 1100px;
      margin: 0 auto;
      padding: 32px 24px 48px;
    }}
    .hero, .plot-card {{
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 18px;
      padding: 20px 22px;
      box-shadow: 0 10px 30px rgba(20, 30, 40, 0.08);
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
      border-radius: 12px;
      background: #f1f6f8;
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
    {plot_markup}
  </main>
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

        metrics = {
            "config_id": summary.get("config_id", "unknown"),
            "status": summary.get("status", "unknown"),
            "simulator_version": summary.get("simulator_version", "unknown"),
            "record_count": len(records),
            "generated_plots": generated_plots,
            "analysis": summary.get("analysis", {}),
        }
        (output_dir / "metrics.json").write_text(
            json.dumps(metrics, indent=2) + "\n", encoding="utf-8"
        )
        write_html_report(output_dir / "index.html", summary, metrics, generated_plots)
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
