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


def viewer_payload(
    summary: dict,
    time_series: dict,
    visualization: dict | None,
    generated_plots: list[str],
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
    }


def render_interactive_viewer(visualization: dict | None) -> str:
    if visualization is None:
        return ""

    channels = visualization.get("channels", {})
    unavailable = [
        f"{name}: {channel.get('reason', 'unavailable')}"
        for name, channel in channels.items()
        if isinstance(channel, dict) and channel.get("availability") == "unavailable"
    ]
    unavailable_markup = "\n".join(
        f"<li>{escape(item)}</li>" for item in unavailable
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
      <div class="viewer-grid">
        <div>
          <h2>Top Projection</h2>
          <canvas id="projection-top" width="720" height="320"></canvas>
        </div>
        <div>
          <h2>Side Projection</h2>
          <canvas id="projection-side" width="720" height="320"></canvas>
        </div>
      </div>
      <div class="status-row">
        <div id="trust-status">trust-status</div>
        <div id="vector-status">hull_hydro_force_world_n</div>
      </div>
      <details open>
        <summary>Unavailable Channels</summary>
        <ul>{unavailable_markup}</ul>
      </details>
      <div class="plot-board" id="plot-board">
        <canvas class="linked-plot" data-channel="boat_speed_mps" width="720" height="180"></canvas>
        <canvas class="linked-plot" data-channel="stroke_power_w" width="720" height="180"></canvas>
        <span class="plot-cursor" aria-hidden="true"></span>
      </div>
    </section>
"""


def write_html_report(
    path: Path,
    summary: dict,
    metrics: dict,
    plot_files: list[str],
    time_series: dict,
    visualization: dict | None = None,
) -> None:
    analysis = summary.get("analysis", {})
    metric_table = render_metrics_table(analysis if isinstance(analysis, dict) else {})
    interactive_markup = render_interactive_viewer(visualization)
    payload = viewer_payload(summary, time_series, visualization, plot_files)
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
      const records = airowViewerData.records || [];
      const timeline = document.getElementById("timeline");
      const playButton = document.getElementById("playback-toggle");
      const timeReadout = document.getElementById("viewer-time");
      const phaseReadout = document.getElementById("viewer-phase");
      const trustStatus = document.getElementById("trust-status");
      const vectorStatus = document.getElementById("vector-status");
      const topCanvas = document.getElementById("projection-top");
      const sideCanvas = document.getElementById("projection-side");
      const plotCanvases = Array.from(document.querySelectorAll(".linked-plot"));
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
        const x = map(Number(hullPos[0] || 0), extent.xMin, extent.xMax, w, 48);
        const yValue = mode === "top" ? Number(hullPos[1] || 0) : Number(hullPos[2] || 0);
        const yMin = mode === "top" ? extent.yMin : extent.zMin;
        const yMax = mode === "top" ? extent.yMax : extent.zMax;
        const y = h - map(yValue, yMin, yMax, h, 42);

        ctx.fillStyle = "#145a7a";
        ctx.fillRect(x - 24, y - 8, 48, 16);
        ctx.fillStyle = "#172026";
        ctx.fillText("hull", x - 12, y - 14);

        for (const [key, color] of [["port_blade", "#b34d3d"], ["starboard_blade", "#266f4d"]]) {{
          const blade = transforms[key] || {{}};
          const tip = blade.tip_position_world_m || [0, 0, 0];
          const bx = map(Number(tip[0] || 0), extent.xMin, extent.xMax, w, 48);
          const byValue = mode === "top" ? Number(tip[1] || 0) : Number(tip[2] || 0);
          const by = h - map(byValue, yMin, yMax, h, 42);
          ctx.strokeStyle = color;
          ctx.lineWidth = 2;
          ctx.beginPath(); ctx.moveTo(x, y); ctx.lineTo(bx, by); ctx.stroke();
          ctx.fillStyle = color;
          ctx.beginPath(); ctx.arc(bx, by, 5, 0, Math.PI * 2); ctx.fill();
        }}

        const hydro = vectors.hull_hydro_force_world_n;
        const vx = component(hydro, 0);
        const vy = mode === "top" ? component(hydro, 1) : component(hydro, 2);
        const scale = 2.0;
        ctx.strokeStyle = "#0b5870";
        ctx.lineWidth = 3;
        ctx.beginPath(); ctx.moveTo(x, y); ctx.lineTo(x + vx * scale, y - vy * scale); ctx.stroke();
        vectorStatus.textContent = `hull_hydro_force_world_n: [${{component(hydro, 0).toFixed(2)}}, ${{component(hydro, 1).toFixed(2)}}, ${{component(hydro, 2).toFixed(2)}}] N world`;
      }}

      function drawPlot(canvas, channel, index) {{
        const ctx = canvas.getContext("2d");
        const values = records.map((record) => {{
          if (channel === "boat_speed_mps") return Number(record.boat_speed_mps?.value || 0);
          if (channel === "stroke_power_w") return Number(record.stroke_power_w?.value || 0);
          return 0;
        }});
        const min = Math.min(...values, 0);
        const max = Math.max(...values, 1);
        const w = canvas.width;
        const h = canvas.height;
        ctx.clearRect(0, 0, w, h);
        ctx.fillStyle = "#fbfdff";
        ctx.fillRect(0, 0, w, h);
        ctx.strokeStyle = "#d8e0e6";
        ctx.strokeRect(45, 16, w - 65, h - 48);
        ctx.strokeStyle = channel === "boat_speed_mps" ? "#145a7a" : "#266f4d";
        ctx.lineWidth = 2;
        ctx.beginPath();
        values.forEach((value, i) => {{
          const px = 45 + (i / Math.max(1, values.length - 1)) * (w - 65);
          const py = h - 32 - ((value - min) / Math.max(1e-9, max - min)) * (h - 48);
          if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
        }});
        ctx.stroke();
        const cursorX = 45 + (index / Math.max(1, values.length - 1)) * (w - 65);
        ctx.strokeStyle = "#b34d3d";
        ctx.beginPath(); ctx.moveTo(cursorX, 16); ctx.lineTo(cursorX, h - 32); ctx.stroke();
        ctx.fillStyle = "#172026";
        ctx.fillText(channel, 48, 12);
      }}

      function render() {{
        if (!samples.length) return;
        frameIndex = Math.max(0, Math.min(frameIndex, samples.length - 1));
        timeline.value = String(frameIndex);
        const sample = samples[frameIndex];
        timeReadout.textContent = `${{Number(sample.time_s || 0).toFixed(3)}} s`;
        phaseReadout.textContent = String(sample.scalars?.stroke_phase || "phase");
        drawProjection(topCanvas, sample, "top");
        drawProjection(sideCanvas, sample, "side");
        plotCanvases.forEach((canvas) => drawPlot(canvas, canvas.dataset.channel || "", frameIndex));
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
            "interactive_visualization": visualization is not None,
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
            time_series,
            visualization,
        )
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
