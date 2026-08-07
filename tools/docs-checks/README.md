# Documentation checks

Automated guards for the repository's Markdown documentation. One command
runs all three checks (Python 3, no third-party dependencies):

```
python tools/docs-checks/check_docs.py
```

The same command runs in CI as the `docs` job (`.github/workflows/ci.yml`),
so a pull request fails if any check fails.

## What is checked

1. **Link check** — every relative link and `#anchor` in every `.md` file
   must resolve inside the repository; no link may escape the tree.
   External URLs must be `http(s)` to public hosts and are printed as an
   informational list.

2. **Internal-ref lint** — no Markdown file may contain an internal
   issue-tracker reference (patterns like an uppercase project key followed
   by a hyphen and digits). The SDK is public: describe the change or
   context in prose instead. Plain words such as "COM port" or "COM10"
   (no hyphen+digits) do not match.

3. **Symbol lint** — Actisense-SDK-looking API mentions in the API-facing
   docs (`code/cpp/docs/`, the root `README.md`, and the example help
   pages under `code/cpp/examples/`) must exist in the public headers
   under `code/cpp/src/public/`. Checked forms:
   - `` `Class::member()` `` qualified mentions (inline or in code samples),
   - `` `someMethod()` `` bare method/function mentions,
   - `` `PascalCaseType` `` bare type mentions,
   - `#include "public/..."` paths (must exist) and `#include` of internal
     directories (`core/`, `protocols/`, … — always an error in these docs).

## Allowlist

`symbol_allowlist.txt` holds the legitimate exceptions for the symbol lint
(internal classes named descriptively in the architecture overview,
example-local types, wire-protocol command names, std observers). Each
entry is commented with its justification. Add a name there only when the
mention is intentionally *not* part of the public API — never to silence
real documentation drift.
