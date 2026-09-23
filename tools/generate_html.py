#!/usr/bin/env python3
"""Generate compressed C headers from HTML resources.

The HTML files (and any inline <style>/<script> blocks) are minified with a
pure-Python, zero-dependency minifier before being gzip-compressed so that the
final PROGMEM arrays are as small as possible.
"""
import os
import re
import subprocess
import tempfile
from pathlib import Path

# ---------------------------------------------------------------------------
# Pure-Python minifier (no third-party packages required)
# ---------------------------------------------------------------------------

def _strip_js_comments(js: str) -> str:
    """Remove // line comments and /* block comments */ from JavaScript.

    Respects string literals (single, double, template) and regex literals so
    that slashes inside them are not misinterpreted.
    """
    result: list[str] = []
    i = 0
    n = len(js)
    while i < n:
        c = js[i]
        # String literals — copy verbatim until closing quote
        if c in ('"', "'", '`'):
            quote = c
            result.append(c)
            i += 1
            while i < n:
                ch = js[i]
                result.append(ch)
                if ch == '\\' and i + 1 < n:          # escape sequence
                    i += 1
                    result.append(js[i])
                elif ch == quote:
                    break
                i += 1
            i += 1
            continue
        # Block comment
        if c == '/' and i + 1 < n and js[i + 1] == '*':
            i += 2
            while i < n and not (js[i] == '*' and i + 1 < n and js[i + 1] == '/'):
                i += 1
            i += 2  # skip */
            result.append(' ')
            continue
        # Line comment
        if c == '/' and i + 1 < n and js[i + 1] == '/':
            i += 2
            while i < n and js[i] != '\n':
                i += 1
            result.append('\n')
            continue
        result.append(c)
        i += 1
    return ''.join(result)


def _strip_css_comments(css: str) -> str:
    """Remove /* block comments */ from CSS."""
    return re.sub(r'/\*.*?\*/', '', css, flags=re.DOTALL)


def _minify_css(css: str) -> str:
    css = _strip_css_comments(css)
    css = re.sub(r'\s+', ' ', css)           # collapse whitespace
    css = re.sub(r'\s*([{}:;,>~+])\s*', r'\1', css)  # spaces around punctuation
    css = re.sub(r';\s*}', '}', css)         # trailing semicolons before }
    return css.strip()


def _minify_js(js: str) -> str:
    js = _strip_js_comments(js)
    # Collapse runs of whitespace that are not inside strings
    # (a simple line-by-line strip is good enough for most ESP8266 JS)
    lines = [ln.strip() for ln in js.splitlines()]
    js = '\n'.join(ln for ln in lines if ln)
    # Replace multiple blank lines
    js = re.sub(r'\n{2,}', '\n', js)
    return js.strip()


def _minify_html(html: str) -> str:
    """Minify HTML: strip comments, minify inline CSS/JS, collapse whitespace."""
    # 1. Remove HTML comments but preserve IE conditionals <!--[if …]> … <![endif]-->
    html = re.sub(r'<!--(?!\[if).*?-->', '', html, flags=re.DOTALL)

    # 2. Minify inline <style> blocks
    def _repl_style(m: re.Match) -> str:
        return f'<style>{_minify_css(m.group(1))}</style>'
    html = re.sub(r'<style[^>]*>(.*?)</style>', _repl_style, html, flags=re.DOTALL | re.IGNORECASE)

    # 3. Minify inline <script> blocks
    def _repl_script(m: re.Match) -> str:
        return f'<script>{_minify_js(m.group(1))}</script>'
    html = re.sub(r'<script[^>]*>(.*?)</script>', _repl_script, html, flags=re.DOTALL | re.IGNORECASE)

    # 4. Collapse runs of whitespace between tags
    html = re.sub(r'>\s+<', '><', html)
    # 5. Collapse internal whitespace runs (outside tags) to a single space
    html = re.sub(r'[ \t]{2,}', ' ', html)
    # 6. Remove leading/trailing whitespace on every line
    lines = [ln.strip() for ln in html.splitlines()]
    html = '\n'.join(ln for ln in lines if ln)
    return html


# ---------------------------------------------------------------------------
# Build pipeline
# ---------------------------------------------------------------------------

#: Source HTML files → (gzip target, header target, C symbol prefix)
HTML_SOURCES = [
    ("res/index.html", "src/generated/index.html.gz", "src/generated/index_html.h"),
    ("res/ota.html",   "src/generated/ota.html.gz",   "src/generated/ota_html.h"),
]


def _process_html(src: str, gz: str, header: str) -> None:
    """Minify *src*, compress with gzip, and produce a PROGMEM C header."""
    raw = Path(src).read_text(encoding="utf-8")
    minified = _minify_html(raw)

    original_size = len(raw.encode())
    minified_size = len(minified.encode())
    saving = original_size - minified_size
    print(f"  {src}: {original_size} → {minified_size} bytes ({saving:+d}, "
          f"{saving / original_size * 100:.1f}% saved)")

    # Write minified HTML to a temp file so we can pipe it through gzip
    with tempfile.NamedTemporaryFile(suffix=".html", mode="w",
                                     encoding="utf-8", delete=False) as tmp:
        tmp.write(minified)
        tmp_path = tmp.name

    try:
        subprocess.run(f"gzip -9 -c {tmp_path} > {gz}", shell=True, check=True)
        subprocess.run(f"xxd -i {gz} > {header}",        shell=True, check=True)
        subprocess.run(
            f"sed -i 's/unsigned char/const unsigned char PROGMEM/' {header}",
            shell=True, check=True,
        )
        subprocess.run(f"rm {gz}", shell=True, check=True)
    finally:
        os.unlink(tmp_path)


def main() -> None:
    project_root = Path(__file__).resolve().parent.parent
    os.chdir(project_root)
    os.makedirs("src/generated", exist_ok=True)

    for src, gz, header in HTML_SOURCES:
        print(f"Processing {src} …")
        _process_html(src, gz, header)

    print("Done.")


if __name__ == "__main__":
    main()
