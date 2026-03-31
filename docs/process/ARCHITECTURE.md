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

# ARCHITECTURE.md — Template Architecture

## A-001 — String Utilities Component
- **Title**: ASCII uppercase transformation component
- **Satisfies**: [R-001]
- **Status**: DONE
- **Description**: Provides a deterministic uppercase conversion utility.
- **Interfaces**: `project::to_upper_ascii(std::string)`
