# CALIBRATION_PROVENANCE.md

This document defines provenance expectations for imported calibration and
derived model artifacts.

## Required metadata
- source identifier,
- artifact version,
- immutable content hash or equivalent identifier,
- schema or format identifier.

## Storage and usage expectations
- Calibration inputs and derived artifacts must remain machine-readable.
- Imported artifacts must be validated before runtime use.
- File-backed imported artifacts used by the shared runtime must reject partial
  provenance metadata deterministically.
- Run metadata must record the identifiers of external artifacts used during
  execution.

## Project guardrails
- Keep calibration-generation workflows optional from the default runtime path.
- Do not accept partially specified provenance metadata.
- Prefer explicit versioned artifacts over unnamed ad hoc files.
