<!-- TRACE-SYNTAX: ARCHITECTURE
Each architecture item must use:

<any markdown heading level> A-xyz — <Component or Capability Name>
- **Title**: <concise name>
- **Satisfies**: [R-001, R-002]
- **Status**: OPEN|IN_PROGRESS|DONE
- **Evidence Profile**: CODE|ASSET (optional, default CODE)
- **Description**: <optional>
- **Interfaces**: <optional>

Validation policy:
- DONE architecture items with `Evidence Profile: CODE` must map to at least
  one D-### implementation and one IT-### test.
- IN_PROGRESS architecture items with `Evidence Profile: CODE` should map to at
  least one D-### implementation.
- DONE architecture items with `Evidence Profile: ASSET` must be covered by at
  least one QT-### through satisfied requirements.
-->

# ARCHITECTURE.md — Rowing Simulator Bootstrap Architecture

This file is still in bootstrap state.

The repository requirements and decisions now describe the rowing simulator, but
the architecture inventory has not yet been seeded with the planned simulator
subsystems. Treat the entry below as a temporary placeholder that supports the
current sample code and trace/tool bootstrap only.

## A-001 — Bootstrap Placeholder Component
- **Title**: Temporary sample component for repository bootstrap
- **Satisfies**: []
- **Status**: DONE
- **Description**: Keeps the current sample code and tests traceable while the
  first simulator architecture inventory is still pending.
- **Interfaces**: `project::to_upper_ascii(std::string)`
