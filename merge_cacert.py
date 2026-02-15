#!/usr/bin/env python3
"""
Merge CA certificates from bundle.pem into cacert-new.pem.

Reads bundle.pem and cacert-new.pem (format: CERTNAME, ===..., -----BEGIN CERTIFICATE-----,
certificate body, -----END CERTIFICATE-----). For each certificate in bundle.pem that is
not already present in cacert-new.pem (compared by certificate body), prepends it to
cacert-new.pem. The updated cacert-new.pem is written back.
"""

import re
from pathlib import Path


def parse_pem_certs(path: Path) -> list[tuple[str, str, str, str]]:
    """
    Parse a PEM file. Returns list of (body_key, name, separator, full_cert_block).
    body_key is normalized cert body for deduplication.
    """
    text = path.read_text(encoding="utf-8", errors="replace")
    entries: list[tuple[str, str, str, str]] = []

    pos = 0
    while True:
        begin_idx = text.find("-----BEGIN CERTIFICATE-----", pos)
        if begin_idx == -1:
            break
        end_idx = text.find("-----END CERTIFICATE-----", begin_idx)
        if end_idx == -1:
            break
        end_idx += len("-----END CERTIFICATE-----")
        cert_block = text[begin_idx:end_idx]

        # Extract body for dedup key (normalize whitespace)
        body_match = re.search(
            r"-----BEGIN CERTIFICATE-----\s*(.*?)\s*-----END CERTIFICATE-----",
            cert_block,
            re.DOTALL,
        )
        body = re.sub(r"\s+", "", body_match.group(1)) if body_match else ""

        # Header: lines between previous END (or start) and this BEGIN
        prev_end = text.rfind("-----END CERTIFICATE-----", 0, begin_idx)
        if prev_end == -1:
            header_chunk = text[:begin_idx]
        else:
            header_chunk = text[prev_end + len("-----END CERTIFICATE-----") : begin_idx]
        header_lines = [line.strip() for line in header_chunk.split("\n") if line.strip()]
        if len(header_lines) >= 2 and "=" in header_lines[-1]:
            name = header_lines[-2]
            separator = header_lines[-1]
        elif header_lines:
            name = header_lines[-1]
            separator = "=================="
        else:
            name = "Unknown"
            separator = "=================="

        entries.append((body, name, separator, cert_block))
        pos = end_idx

    return entries


def main() -> None:
    script_dir = Path(__file__).resolve().parent
    bundle_path = script_dir / "bundle.pem"
    cacert_path = script_dir / "cacert-new.pem"

    if not bundle_path.exists():
        raise SystemExit(f"Missing: {bundle_path}")
    if not cacert_path.exists():
        raise SystemExit(f"Missing: {cacert_path}")

    bundle_entries = parse_pem_certs(bundle_path)
    cacert_entries = parse_pem_certs(cacert_path)

    cacert_bodies = {body for (body, _n, _s, _c) in cacert_entries}
    missing: list[tuple[str, str, str, str]] = []
    for item in bundle_entries:
        body, name, sep, cert_block = item
        if body not in cacert_bodies:
            missing.append(item)

    if not missing:
        print("All certificates from bundle.pem are already in cacert-new.pem. No changes.")
        return

    print(f"Found {len(missing)} certificate(s) in bundle.pem missing from cacert-new.pem:")
    for (_body, name, _sep, _block) in missing:
        print(f"  - {name}")

    # Build new file: missing certs (with name + separator) then original cacert content
    new_parts: list[str] = []
    for (_body, name, separator, cert_block) in missing:
        new_parts.append(name)
        new_parts.append(separator)
        new_parts.append(cert_block)
        new_parts.append("")

    original_content = cacert_path.read_text(encoding="utf-8", errors="replace")
    if not original_content.endswith("\n"):
        original_content += "\n"
    new_content = "\n".join(new_parts) + "\n" + original_content

    cacert_path.write_text(new_content, encoding="utf-8", newline="\n")
    print(f"Prepended {len(missing)} certificate(s) to {cacert_path}.")


if __name__ == "__main__":
    main()
