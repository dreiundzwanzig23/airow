#!/usr/bin/env python3
"""Export airow visualization artifacts to a small ParaView/VTK bundle."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import re
import sys
from typing import Any

from validate_visualization_artifact import validate_document

EXPORT_SCHEMA_ID = "airow.paraview_export.v1"
LOADING_GUIDE_FILE_NAME = "paraview_loading_guide.md"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Export airow.visualization.v1 JSON to legacy ASCII VTK files."
    )
    parser.add_argument(
        "--visualization",
        required=True,
        help="Path to airow.visualization.v1 JSON",
    )
    parser.add_argument(
        "--output-dir",
        required=True,
        help="Directory where ParaView/VTK files will be written",
    )
    return parser.parse_args()


def load_visualization(path: Path) -> dict[str, Any]:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise RuntimeError(f"missing visualization artifact: {path}") from exc
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"failed to parse visualization artifact {path}: {exc}") from exc

    issues = validate_document(document)
    if issues:
        joined = "; ".join(issues[:4])
        if len(issues) > 4:
            joined += f"; {len(issues) - 4} more issue(s)"
        raise RuntimeError(f"invalid visualization artifact {path}: {joined}")
    return document


def vtk_name(identifier: str) -> str:
    sanitized = re.sub(r"[^A-Za-z0-9_]", "_", identifier)
    if not sanitized or sanitized[0].isdigit():
        return f"airow_{sanitized}"
    return sanitized


def vector_channel_metadata_id(vector_id: str) -> str:
    replacements = {
        "hull_hydro_force_world_n": "hull_hydro_force",
        "hull_hydro_force_body_n": "hull_hydro_force_body",
        "hull_hydro_moment_world_n_m": "hull_hydro_moment",
        "hull_hydro_moment_body_n_m": "hull_hydro_moment_body",
        "port_blade_force_world_n": "blade_force",
        "port_blade_force_body_n": "blade_force_body",
        "starboard_blade_force_world_n": "blade_force",
        "starboard_blade_force_body_n": "blade_force_body",
        "aero_force_world_n": "aero_force",
        "aero_force_body_n": "aero_force_body",
        "aero_moment_world_n_m": "aero_moment",
        "aero_moment_body_n_m": "aero_moment_body",
        "ambient_wind_world_mps": "ambient_wind",
        "ambient_wind_body_mps": "ambient_wind_body",
        "apparent_wind_world_mps": "apparent_wind",
        "apparent_wind_body_mps": "apparent_wind_body",
        "rower_inertial_force_world_n": "rower_inertial_force",
        "rower_inertial_force_body_n": "rower_inertial_force_body",
    }
    return replacements.get(vector_id, vector_id)


def numeric_vector(value: Any) -> list[float]:
    if not isinstance(value, list) or len(value) != 3:
        return [0.0, 0.0, 0.0]
    return [float(component or 0.0) for component in value]


def numeric_quaternion(value: Any) -> list[float]:
    if not isinstance(value, list) or len(value) != 4:
        return [0.0, 0.0, 0.0, 1.0]
    return [float(component or 0.0) for component in value]


def quaternion_conjugate(quaternion: list[float]) -> list[float]:
    return [-quaternion[0], -quaternion[1], -quaternion[2], quaternion[3]]


def quaternion_multiply(left: list[float], right: list[float]) -> list[float]:
    lx, ly, lz, lw = left
    rx, ry, rz, rw = right
    return [
        lw * rx + lx * rw + ly * rz - lz * ry,
        lw * ry - lx * rz + ly * rw + lz * rx,
        lw * rz + lx * ry - ly * rx + lz * rw,
        lw * rw - lx * rx - ly * ry - lz * rz,
    ]


def normalize_quaternion(quaternion: list[float]) -> list[float]:
    norm = math.sqrt(sum(component * component for component in quaternion))
    if norm <= 0.0:
        return [0.0, 0.0, 0.0, 1.0]
    return [component / norm for component in quaternion]


def rotate_body_to_world(quaternion: list[float], vector: list[float]) -> list[float]:
    normalized = normalize_quaternion(quaternion)
    rotated = quaternion_multiply(
        quaternion_multiply(normalized, [vector[0], vector[1], vector[2], 0.0]),
        quaternion_conjugate(normalized),
    )
    return rotated[:3]


def add_points(points: list[list[float]], *new_points: list[float]) -> list[int]:
    indices: list[int] = []
    for point in new_points:
        indices.append(len(points))
        points.append(point)
    return indices


def hull_position(sample: dict[str, Any]) -> list[float]:
    hull = sample.get("transforms", {}).get("hull", {})
    return numeric_vector(hull.get("position_world_m", [0.0, 0.0, 0.0]))


def hull_orientation(sample: dict[str, Any]) -> list[float]:
    hull = sample.get("transforms", {}).get("hull", {})
    return numeric_quaternion(hull.get("orientation_world_from_body_xyzw"))


def body_point_to_world(sample: dict[str, Any], body_point: list[float]) -> list[float]:
    position = hull_position(sample)
    rotated = rotate_body_to_world(hull_orientation(sample), body_point)
    return [position[index] + rotated[index] for index in range(3)]


def write_geometry_vtk(path: Path, visualization: dict[str, Any]) -> None:
    samples = visualization.get("samples", [])
    points: list[list[float]] = []
    vertices: list[int] = []
    lines: list[list[int]] = []
    trajectory: list[int] = []

    for sample in samples:
        if not isinstance(sample, dict):
            continue
        transforms = sample.get("transforms", {})
        hull_index = add_points(points, hull_position(sample))[0]
        trajectory.append(hull_index)
        vertices.append(hull_index)

        for key in ("port_oar", "starboard_oar"):
            oar = transforms.get(key, {})
            oarlock = body_point_to_world(
                sample, numeric_vector(oar.get("oarlock_position_body_m", [0, 0, 0]))
            )
            blade_tip = numeric_vector(oar.get("blade_tip_position_world_m", [0, 0, 0]))
            line = add_points(points, oarlock, blade_tip)
            lines.append(line)
            vertices.extend(line)

        waterline = transforms.get("waterline", {})
        origin = numeric_vector(waterline.get("origin_world_m", [0.0, 0.0, 0.0]))
        waterline_line = add_points(
            points,
            [origin[0] - 1.0, origin[1], origin[2]],
            [origin[0] + 1.0, origin[1], origin[2]],
        )
        lines.append(waterline_line)

    if len(trajectory) > 1:
        lines.append(trajectory)

    line_index_count = sum(len(line) + 1 for line in lines)
    vertex_index_count = len(vertices) * 2
    rows = [
        "# vtk DataFile Version 3.0",
        "airow_geometry hull_trajectory oars blades waterline",
        "ASCII",
        "DATASET POLYDATA",
        f"POINTS {len(points)} float",
    ]
    rows.extend(f"{point[0]:.9g} {point[1]:.9g} {point[2]:.9g}" for point in points)
    rows.append(f"VERTICES {len(vertices)} {vertex_index_count}")
    rows.extend(f"1 {index}" for index in vertices)
    rows.append(f"LINES {len(lines)} {line_index_count}")
    rows.extend(
        " ".join([str(len(line)), *[str(index) for index in line]]) for line in lines
    )
    path.write_text("\n".join(rows) + "\n", encoding="utf-8")


def sample_vector(sample: dict[str, Any], vector_id: str) -> list[float]:
    vector = sample.get("vectors", {}).get(vector_id, {})
    return numeric_vector(vector.get("value", [0.0, 0.0, 0.0]))


def vector_ids(visualization: dict[str, Any]) -> list[str]:
    samples = visualization.get("samples", [])
    if not samples or not isinstance(samples[0], dict):
        return []
    vectors = samples[0].get("vectors", {})
    if not isinstance(vectors, dict):
        return []
    return sorted(str(vector_id) for vector_id in vectors)


def write_vectors_vtk(path: Path, visualization: dict[str, Any]) -> None:
    samples = [
        sample for sample in visualization.get("samples", []) if isinstance(sample, dict)
    ]
    points = [hull_position(sample) for sample in samples]
    rows = [
        "# vtk DataFile Version 3.0",
        "airow_vectors hull_sample_vector_channels",
        "ASCII",
        "DATASET POLYDATA",
        f"POINTS {len(points)} float",
    ]
    rows.extend(f"{point[0]:.9g} {point[1]:.9g} {point[2]:.9g}" for point in points)
    rows.append(f"VERTICES {len(points)} {len(points) * 2}")
    rows.extend(f"1 {index}" for index in range(len(points)))
    rows.append(f"POINT_DATA {len(points)}")
    for vector_id in vector_ids(visualization):
        rows.append(f"VECTORS {vtk_name(vector_id)} float")
        for sample in samples:
            vector = sample_vector(sample, vector_id)
            rows.append(f"{vector[0]:.9g} {vector[1]:.9g} {vector[2]:.9g}")
    path.write_text("\n".join(rows) + "\n", encoding="utf-8")


def vector_metadata(visualization: dict[str, Any]) -> dict[str, dict[str, str]]:
    channels = visualization.get("channels", {})
    samples = visualization.get("samples", [])
    first_sample = samples[0] if samples and isinstance(samples[0], dict) else {}
    sample_vectors = first_sample.get("vectors", {})
    metadata: dict[str, dict[str, str]] = {}
    if not isinstance(sample_vectors, dict):
        return metadata
    for vector_id, vector in sorted(sample_vectors.items()):
        if not isinstance(vector, dict):
            continue
        metadata_id = vector_channel_metadata_id(str(vector_id))
        channel = channels.get(metadata_id, {})
        if not isinstance(channel, dict):
            channel = {}
        metadata[str(vector_id)] = {
            "vtk_name": vtk_name(str(vector_id)),
            "unit": str(vector.get("unit", channel.get("unit", ""))),
            "frame": str(vector.get("frame", channel.get("frame", ""))),
            "provenance": str(channel.get("provenance", "emitted_sample")),
            "metadata_channel": metadata_id,
        }
    return metadata


def write_metadata_json(
    path: Path,
    visualization: dict[str, Any],
    files: list[str],
) -> None:
    metadata = {
        "schema_id": EXPORT_SCHEMA_ID,
        "source_schema_id": visualization.get("schema_id"),
        "config_id": visualization.get("config_id"),
        "simulator_version": visualization.get("simulator_version"),
        "sample_count": len(visualization.get("samples", [])),
        "coordinate_conventions": visualization.get("frames", {}),
        "files": files,
        "loading_guide_path": LOADING_GUIDE_FILE_NAME,
        "vector_channels": vector_metadata(visualization),
    }
    path.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")


def write_loading_guide(path: Path) -> None:
    guide = """# ParaView Loading Guide

This bundle is a reduced visualization export from an
`airow.visualization.v1` artifact. It is intended for offline inspection of the
emitted reduced-runtime geometry and vector channels, not as a full 3D water or
optimization claim.

1. Open `airow_geometry.vtk` in ParaView to inspect hull trajectory, oar and
   blade line segments, and the waterline reference.
2. Open `airow_vectors.vtk` in the same session to inspect sample-point vector
   fields.
3. Use `airow_metadata.json` to interpret vector units, frames, provenance, and
   coordinate conventions.
4. Apply Glyph or similar vector filters to `airow_vectors.vtk` when arrow
   overlays are needed.

The files are deterministic for the same source artifact and export settings.
"""
    path.write_text(guide, encoding="utf-8")


def export_visualization_to_vtk(
    visualization: dict[str, Any], output_dir: Path
) -> dict[str, str]:
    output_dir.mkdir(parents=True, exist_ok=True)
    geometry_path = output_dir / "airow_geometry.vtk"
    vectors_path = output_dir / "airow_vectors.vtk"
    metadata_path = output_dir / "airow_metadata.json"
    loading_guide_path = output_dir / LOADING_GUIDE_FILE_NAME
    write_geometry_vtk(geometry_path, visualization)
    write_vectors_vtk(vectors_path, visualization)
    write_loading_guide(loading_guide_path)
    write_metadata_json(
        metadata_path,
        visualization,
        [
            geometry_path.name,
            vectors_path.name,
            metadata_path.name,
            loading_guide_path.name,
        ],
    )
    return {
        "schema_id": EXPORT_SCHEMA_ID,
        "geometry_path": str(geometry_path),
        "vectors_path": str(vectors_path),
        "metadata_path": str(metadata_path),
        "loading_guide_path": str(loading_guide_path),
    }


def main() -> int:
    args = parse_args()
    try:
        visualization = load_visualization(Path(args.visualization))
        export_visualization_to_vtk(visualization, Path(args.output_dir))
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
