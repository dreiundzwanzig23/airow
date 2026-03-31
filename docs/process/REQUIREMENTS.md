<!-- TRACE-SYNTAX: REQUIREMENTS
Each requirement must use this block format:

## R-xyz — <Title>
- **Title**: <short summary>
- **Acceptance Criteria**:
  - <bullet, observable>
- **Priority**: P0|P1|P2
- **Status**: OPEN|IN_PROGRESS|DONE
- **Created**: YYYY-MM-DD
- **Updated**: YYYY-MM-DD
- **Change-Type**: none|editorial|semantic
- **Needs-Review**: yes|no
- **Change-Note**: <optional text>
- **Notes**: <optional text>

Validation policy:
- DONE requirements must be satisfied by at least one architecture item.
- IN_PROGRESS requirements should be satisfied by at least one architecture item.
- OPEN requirements may exist without architecture mapping during backlog refinement.
- `Change-Type: semantic` requires `Needs-Review: yes`.
-->

# REQUIREMENTS.md — Template Requirements

## R-001 — Basic uppercase utility
- **Title**: Convert ASCII letters to uppercase
- **Acceptance Criteria**:
  - Given `"abcXYZ"`, returns `"ABCXYZ"`
  - Non-letters remain unchanged
  - Behavior is verified at UT/IT/QT layers
- **Priority**: P2
- **Status**: DONE
- **Created**: 2026-02-28
- **Updated**: 2026-03-23
- **Change-Type**: editorial
- **Needs-Review**: no
- **Change-Note**: Updated to the refined template metadata format.
- **Notes**: Minimal sample requirement retained for template traceability.
