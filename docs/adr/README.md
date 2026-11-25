# flint Architecture Decision Records

Each record captures one significant decision: the context, what was decided, and its consequences. Records are never deleted; a decision that changes is marked **Superseded** and linked to its replacement.

Statuses: **Proposed** (awaiting the owner's agreement), **Accepted**, **Superseded**, **Deprecated**.

| ADR | Title | Status | Date |
|---|---|---|---|
| [0001](0001-cpp26-and-gcc16.md) | C++26 with GCC 16 as the reference compiler | Accepted | 2026-11-24 |
| [0002](0002-dependencies-via-fetchcontent.md) | Third-party dependencies via CMake FetchContent | Accepted | 2026-11-24 |
| [0003](0003-confined-reflection.md) | Reflection confined to `flint/meta`, with tooling stubs | Accepted | 2026-11-24 |
| [0004](0004-logger-architecture.md) | Logger architecture: severities, categories, sinks, async | Accepted | 2026-11-24 |
| [0005](0005-own-json-library.md) | flint's own JSON library | Superseded by 0012 | 2026-11-24 |
| [0006](0006-json-configuration-files.md) | JSON (with comments) for configuration files | Amended by 0012 | 2026-11-24 |
| [0007](0007-network-sink-tcp-json-lines.md) | Network logging as TCP JSON Lines | Accepted | 2026-11-24 |
| [0008](0008-profiling-with-tracy.md) | Profiling with Tracy behind `FLINT_PROFILE_*` macros | Accepted | 2026-11-24 |
| [0009](0009-signals-observables-and-commands.md) | Signals, observables and commands live in flint | Accepted | 2026-11-25 |
| [0010](0010-own-xml-parser.md) | flint has its own small XML parser | Superseded by 0011 | 2026-11-25 |
| [0011](0011-xml-with-pugixml.md) | XML is parsed with pugixml behind flint::xml | Accepted | 2026-11-25 |
| [0012](0012-json-with-glaze.md) | JSON is read and written with glaze | Accepted | 2026-11-25 |
| [0013](0013-model-reflection-and-collections.md) | View models are reflected by flint, and lists are ObservableVector | Accepted | 2026-11-25 |
| [0014](0014-file-watcher.md) | FileWatcher for hot reload | Accepted | 2026-11-25 |

## Template

```markdown
# NNNN. Title

- Status: Proposed | Accepted | Superseded by NNNN
- Date: YYYY-MM-DD

## Context
What forces are at play, and what problem needs deciding.

## Decision
What we will do.

## Consequences
What becomes easier or harder; follow-up work.
```
