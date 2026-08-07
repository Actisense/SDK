#!/usr/bin/env python3
"""Documentation checks for the Actisense SDK repository.

Runs three checks over the repository's Markdown files and exits non-zero
if any of them fails:

1. Link check      - every relative link and anchor resolves inside the
                     repository; no link may escape the tree. External URLs
                     must be http(s) to public hosts and are listed for
                     information.
2. Internal-ref    - no Markdown file may contain an internal issue-tracker
   lint              reference (the SDK is public; change context belongs in
                     prose, not tracker IDs).
3. Symbol lint     - Actisense-SDK-looking API mentions in the API-facing
                     docs (code/cpp/docs/, the root README, and the example
                     help docs) must exist in the public headers under
                     code/cpp/src/public/. Legitimate exceptions live in
                     symbol_allowlist.txt next to this script.

Usage (from anywhere in the repository):

    python tools/docs-checks/check_docs.py
"""

import re
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent.parent
PUBLIC_HEADERS_DIR = REPO_ROOT / "code" / "cpp" / "src" / "public"
ALLOWLIST_FILE = SCRIPT_DIR / "symbol_allowlist.txt"

# Directories never scanned. Local build trees use arbitrary "build*" names
# (and vendor fetched dependencies under _deps), so exclusion is prefix-based
# rather than an exact-name set - the checks must stay quiet on a working
# checkout, not only on CI's fresh clone.
EXCLUDED_DIR_NAMES = {".git", "node_modules", "_deps", "out"}
EXCLUDED_DIR_PREFIXES = ("build",)


def is_excluded_dir(name):
    return name in EXCLUDED_DIR_NAMES or name.startswith(EXCLUDED_DIR_PREFIXES)

# The docs whose API mentions are checked against the public headers.
SYMBOL_LINT_SCOPES = [
    "README.md",
    "code/cpp/docs",
    "code/cpp/examples",
]

TRACKER_REF_RE = re.compile(r"\b(?:NGXSW|DESKTOP|A2D|COM|IEC450|GIT)-[0-9]+\b")

INTERNAL_INCLUDE_PREFIXES = ("core/", "protocols/", "transport/", "util/", "platform/")

# Hosts that are not public.
NON_PUBLIC_HOST_RE = re.compile(
    r"^(localhost|127\.\d+\.\d+\.\d+|0\.0\.0\.0|10\.\d+\.\d+\.\d+|"
    r"192\.168\.\d+\.\d+|172\.(1[6-9]|2\d|3[01])\.\d+\.\d+|[^./]+)$"
)


def find_markdown_files(root):
    """Yield every .md file under root, skipping excluded directories."""
    for path in sorted(root.rglob("*.md")):
        if any(is_excluded_dir(part) for part in path.parts):
            continue
        yield path


def read_text(path):
    return path.read_text(encoding="utf-8", errors="replace")


def strip_code_regions(text):
    """Replace fenced code blocks and inline code spans with placeholders of
    equal line count so line numbers stay meaningful for link scanning."""
    out_lines = []
    in_fence = False
    fence_marker = None
    for line in text.splitlines():
        stripped = line.lstrip()
        if in_fence:
            out_lines.append("")
            if stripped.startswith(fence_marker):
                in_fence = False
            continue
        if stripped.startswith("```") or stripped.startswith("~~~"):
            in_fence = True
            fence_marker = stripped[:3]
            out_lines.append("")
            continue
        # Remove inline code spans on kept lines.
        out_lines.append(re.sub(r"`[^`]*`", "``", line))
    return "\n".join(out_lines)


def github_slug(heading_text, seen):
    """GitHub-style anchor slug for a heading, tracking duplicates."""
    text = heading_text.strip()
    text = text.replace("`", "")
    # Strip markdown links, keep the link text.
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)
    # Strip emphasis markers.
    text = text.replace("*", "").replace("_", " ") if False else text
    text = text.lower()
    text = re.sub(r"[^\w\- ]", "", text)
    text = text.replace(" ", "-")
    base = text
    if base not in seen:
        seen[base] = 0
        return base
    seen[base] += 1
    return f"{base}-{seen[base]}"


def collect_anchors(md_path, cache={}):
    """Set of valid anchors (heading slugs + explicit HTML anchors) in a file."""
    if md_path in cache:
        return cache[md_path]
    anchors = set()
    seen = {}
    text = read_text(md_path)
    in_fence = False
    for line in text.splitlines():
        stripped = line.lstrip()
        if stripped.startswith("```") or stripped.startswith("~~~"):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        m = re.match(r"#{1,6}\s+(.*)$", stripped)
        if m:
            anchors.add(github_slug(m.group(1), seen))
    for m in re.finditer(r'<a\s+(?:name|id)="([^"]+)"', text):
        anchors.add(m.group(1))
    cache[md_path] = anchors
    return anchors


LINK_RE = re.compile(r"!?\[[^\]]*\]\(\s*(<[^>]*>|[^)\s]+)(?:\s+\"[^\"]*\")?\s*\)")


def check_links(md_files):
    """Check every relative link/anchor; report external URLs."""
    errors = []
    externals = []
    for md in md_files:
        rel = md.relative_to(REPO_ROOT).as_posix()
        text = strip_code_regions(read_text(md))
        for lineno, line in enumerate(text.splitlines(), 1):
            for m in LINK_RE.finditer(line):
                target = m.group(1).strip()
                if target.startswith("<") and target.endswith(">"):
                    target = target[1:-1].strip()
                if not target:
                    continue
                scheme = re.match(r"^([a-zA-Z][a-zA-Z0-9+.-]*):", target)
                if scheme:
                    proto = scheme.group(1).lower()
                    if proto in ("http", "https"):
                        host = re.match(r"^https?://([^/:?#]+)", target)
                        hostname = host.group(1).lower() if host else ""
                        if NON_PUBLIC_HOST_RE.match(hostname) and "." not in hostname:
                            errors.append(
                                f"{rel}:{lineno}: external link to non-public "
                                f"host: {target}"
                            )
                        else:
                            externals.append(f"{rel}:{lineno}: {target}")
                    else:
                        errors.append(
                            f"{rel}:{lineno}: non-http(s) link scheme "
                            f"'{proto}:': {target}"
                        )
                    continue
                # Split off any anchor.
                path_part, _, anchor = target.partition("#")
                if not path_part:
                    # Same-file anchor.
                    if anchor and anchor not in collect_anchors(md):
                        errors.append(
                            f"{rel}:{lineno}: broken anchor '#{anchor}' "
                            f"(no such heading in this file)"
                        )
                    continue
                resolved = (md.parent / path_part).resolve()
                try:
                    resolved.relative_to(REPO_ROOT)
                except ValueError:
                    errors.append(
                        f"{rel}:{lineno}: link escapes the repository: {target}"
                    )
                    continue
                if not resolved.exists():
                    errors.append(f"{rel}:{lineno}: broken link target: {target}")
                    continue
                if anchor:
                    if resolved.is_file() and resolved.suffix.lower() == ".md":
                        if anchor not in collect_anchors(resolved):
                            errors.append(
                                f"{rel}:{lineno}: broken anchor "
                                f"'{path_part}#{anchor}'"
                            )
                    else:
                        errors.append(
                            f"{rel}:{lineno}: anchor on non-Markdown target: "
                            f"{target}"
                        )
    return errors, externals


def check_internal_refs(md_files):
    """No tracker IDs anywhere in Markdown (outside http(s) URLs)."""
    errors = []
    for md in md_files:
        rel = md.relative_to(REPO_ROOT).as_posix()
        for lineno, line in enumerate(read_text(md).splitlines(), 1):
            for m in TRACKER_REF_RE.finditer(line):
                # Ignore a match living inside an http(s) URL.
                url_spans = [
                    u.span() for u in re.finditer(r"https?://[^\s)\"'>]+", line)
                ]
                if any(s <= m.start() < e for s, e in url_spans):
                    continue
                errors.append(
                    f"{rel}:{lineno}: internal tracker reference "
                    f"'{m.group(0)}'"
                )
    return errors


# --- Symbol lint -------------------------------------------------------------

# Acronym-ish tokens that read as PascalCase but are domain vocabulary,
# not API symbols.
CPP_KEYWORDS_AND_NOISE = {
    "true", "false", "nullptr", "const", "void", "auto", "if", "else",
    "for", "while", "return", "static", "class", "struct", "enum",
}


def load_allowlist():
    allow = set()
    if ALLOWLIST_FILE.exists():
        for line in read_text(ALLOWLIST_FILE).splitlines():
            line = line.split("#", 1)[0].strip()
            if line:
                allow.add(line)
    return allow


def strip_cpp_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return text


def build_symbol_universe():
    """Every identifier appearing in the *code* of the public headers."""
    universe = set()
    for hpp in sorted(PUBLIC_HEADERS_DIR.rglob("*.hpp")):
        code = strip_cpp_comments(read_text(hpp))
        universe.update(re.findall(r"\b[A-Za-z_]\w*\b", code))
    return universe


def iter_inline_code_spans(text):
    """Yield (lineno, span_text) for inline code spans outside fenced blocks."""
    in_fence = False
    for lineno, line in enumerate(text.splitlines(), 1):
        stripped = line.lstrip()
        if stripped.startswith("```") or stripped.startswith("~~~"):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        for m in re.finditer(r"`([^`]+)`", line):
            yield lineno, m.group(1).strip()


def iter_fenced_code(text):
    """Yield (lineno, line) for lines inside ``` fenced blocks (any language
    except mermaid, whose node labels are prose)."""
    in_fence = False
    lang = ""
    for lineno, line in enumerate(text.splitlines(), 1):
        stripped = line.lstrip()
        if stripped.startswith("```"):
            if not in_fence:
                in_fence = True
                lang = stripped[3:].strip().lower()
            else:
                in_fence = False
            continue
        if in_fence and lang != "mermaid":
            yield lineno, line


def symbol_lint_files():
    files = []
    for scope in SYMBOL_LINT_SCOPES:
        path = REPO_ROOT / scope
        if path.is_file():
            files.append(path)
        elif path.is_dir():
            files.extend(find_markdown_files(path))
    return files


QUALIFIED_RE = re.compile(
    r"^(?:Actisense::)?(?:Sdk::)?([A-Z]\w*)::(~?[A-Za-z_]\w*)(?:\(\s*\))?$"
)
METHOD_RE = re.compile(r"^([A-Za-z_]\w*)\(\s*\)$")
PASCAL_RE = re.compile(r"^[A-Z][A-Za-z0-9]*$")
INCLUDE_RE = re.compile(r'#include\s+"([^"]+)"')
FENCE_QUALIFIED_RE = re.compile(r"\b(?<!:)([A-Z]\w*)::(~?[A-Za-z_]\w*)")


def has_lowercase(s):
    return any(c.islower() for c in s)


def check_symbols():
    errors = []
    universe = build_symbol_universe()
    allow = load_allowlist()

    def known(name):
        return name in universe or name in allow

    def check_include(rel, lineno, inc_path):
        if "*" in inc_path or "<" in inc_path:
            return  # wildcard/placeholder path, not a literal file
        if inc_path.startswith("public/"):
            if not (REPO_ROOT / "code" / "cpp" / "src" / inc_path).exists():
                errors.append(
                    f"{rel}:{lineno}: #include \"{inc_path}\" does not exist "
                    f"under code/cpp/src/"
                )
        elif inc_path.startswith(INTERNAL_INCLUDE_PREFIXES):
            errors.append(
                f"{rel}:{lineno}: doc instructs including internal header "
                f"\"{inc_path}\" (not reachable from consumer code)"
            )

    for md in symbol_lint_files():
        rel = md.relative_to(REPO_ROOT).as_posix()
        text = read_text(md)

        # 1. Include paths, anywhere in the file.
        for lineno, line in enumerate(text.splitlines(), 1):
            for m in INCLUDE_RE.finditer(line):
                check_include(rel, lineno, m.group(1))

        # 2. Inline code spans.
        for lineno, span in iter_inline_code_spans(text):
            qm = QUALIFIED_RE.match(span)
            if qm:
                cls, member = qm.group(1), qm.group(2).lstrip("~")
                if not known(cls):
                    errors.append(
                        f"{rel}:{lineno}: unknown public type "
                        f"'{cls}' (in `{span}`)"
                    )
                elif member and not known(member):
                    errors.append(
                        f"{rel}:{lineno}: unknown public member "
                        f"'{member}' (in `{span}`)"
                    )
                continue
            mm = METHOD_RE.match(span)
            if mm:
                name = mm.group(1)
                if name not in CPP_KEYWORDS_AND_NOISE and not known(name):
                    errors.append(
                        f"{rel}:{lineno}: unknown public function/method "
                        f"'{name}()'"
                    )
                continue
            if span.startswith("public/"):
                if "*" in span or "<" in span:
                    continue  # wildcard/placeholder path, not a literal file
                if not (REPO_ROOT / "code" / "cpp" / "src" / span).exists():
                    errors.append(
                        f"{rel}:{lineno}: header path `{span}` does not "
                        f"exist under code/cpp/src/"
                    )
                continue
            if PASCAL_RE.match(span) and has_lowercase(span) and len(span) >= 4:
                if not known(span):
                    errors.append(
                        f"{rel}:{lineno}: unknown public type '{span}'"
                    )

        # 3. Qualified references inside fenced code samples.
        for lineno, line in iter_fenced_code(text):
            for m in FENCE_QUALIFIED_RE.finditer(line):
                cls, member = m.group(1), m.group(2).lstrip("~")
                if cls in ("Actisense", "Sdk"):
                    continue
                if not known(cls):
                    errors.append(
                        f"{rel}:{lineno}: unknown public type '{cls}' "
                        f"(in code sample '{m.group(0)}')"
                    )
                elif member and not known(member):
                    errors.append(
                        f"{rel}:{lineno}: unknown public member "
                        f"'{cls}::{member}' (in code sample)"
                    )
    return errors


def main():
    md_files = list(find_markdown_files(REPO_ROOT))
    failed = False

    print(f"Scanning {len(md_files)} Markdown files under {REPO_ROOT}")

    link_errors, externals = check_links(md_files)
    print(f"\n[1/3] Link check: {len(link_errors)} error(s)")
    for e in link_errors:
        print(f"  ERROR {e}")
    if externals:
        print(f"  (info) {len(externals)} external URL(s):")
        for e in externals:
            print(f"    {e}")
    failed |= bool(link_errors)

    ref_errors = check_internal_refs(md_files)
    print(f"\n[2/3] Internal-ref lint: {len(ref_errors)} error(s)")
    for e in ref_errors:
        print(f"  ERROR {e}")
    failed |= bool(ref_errors)

    sym_errors = check_symbols()
    print(f"\n[3/3] Symbol lint: {len(sym_errors)} error(s)")
    for e in sym_errors:
        print(f"  ERROR {e}")
    failed |= bool(sym_errors)

    print()
    if failed:
        print("Documentation checks FAILED")
        return 1
    print("Documentation checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
