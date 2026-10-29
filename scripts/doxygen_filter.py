#!/usr/bin/env python3
"""Keep repository navigation usable when Doxygen renders only maintained pages.

Doxygen rewrites a Markdown link to any local .md file as an API reference.
Research and prompt pages are deliberately outside INPUT, so render repository
links as source hyperlinks instead. check_promptset.py validates the original
local paths independently. Explicit Doxygen @ref commands remain untouched.
"""

from __future__ import annotations

import json
import pathlib
import re
import sys
from urllib.parse import quote, unquote, urlsplit


ROOT = pathlib.Path(__file__).resolve().parents[1]
LINK = re.compile(r"(?<!!)(\[[^\]\n]+\]\()([^)\n]+)(\))")


def render_markdown(text: str, source: pathlib.Path) -> str:
    """Render existing relative repository links without changing line numbers."""
    repository = json.loads((ROOT / "library.json").read_text(encoding="utf-8"))["repository"]["url"]
    source_url = repository.removesuffix(".git") + "/blob/promptset/motorcontrol-can/"

    def replace_link(match: re.Match[str]) -> str:
        target = match.group(2).strip().removeprefix("<").removesuffix(">")
        parsed = urlsplit(target)
        if parsed.scheme or parsed.netloc or not parsed.path:
            return match.group(0)
        try:
            relative = (source.parent / unquote(parsed.path)).resolve().relative_to(ROOT)
        except ValueError:
            return match.group(0)
        url = source_url + quote(relative.as_posix())
        if parsed.query:
            url += "?" + parsed.query
        if parsed.fragment:
            url += "#" + parsed.fragment
        return match.group(1) + url + match.group(3)

    return LINK.sub(replace_link, text)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: doxygen_filter.py INPUT_FILE", file=sys.stderr)
        return 2
    source = pathlib.Path(sys.argv[1]).resolve()
    text = source.read_text(encoding="utf-8")
    # Only Markdown input is transformed, even if called directly on a header.
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stdout.write(render_markdown(text, source) if source.suffix == ".md" else text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
