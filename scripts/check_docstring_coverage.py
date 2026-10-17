#!/usr/bin/env python3
"""
check_docstring_coverage.py

Measures and enforces minimum Doxygen docstring coverage for C++ source trees.
Generates temporary Doxygen XML, analyzes compounds and members for documentation,
and fails with a non-zero exit code if coverage falls below the specified threshold.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run_doxygen(input_dirs: list[str], output_dir: str, include_private: bool = True) -> bool:
    """Runs Doxygen in XML-only mode on the specified input directories."""
    doxygen_bin = shutil.which("doxygen")
    if not doxygen_bin:
        print("ERROR: 'doxygen' binary not found in PATH.", file=sys.stderr)
        print("Please install doxygen (e.g. `sudo apt-get install doxygen`).", file=sys.stderr)
        return False

    input_paths = " ".join(f'"{d}"' for d in input_dirs if os.path.exists(d))
    if not input_paths:
        print(f"ERROR: None of the specified input directories exist: {input_dirs}", file=sys.stderr)
        return False

    private_val = "YES" if include_private else "NO"
    doxyfile_content = f"""
PROJECT_NAME           = "DocCoverage"
OUTPUT_DIRECTORY       = "{output_dir}"
INPUT                  = {input_paths}
RECURSIVE              = YES
GENERATE_HTML          = NO
GENERATE_LATEX         = NO
GENERATE_XML           = YES
EXTRACT_ALL            = YES
EXTRACT_PRIVATE        = {private_val}
EXTRACT_STATIC         = YES
QUIET                  = YES
WARNINGS               = NO
WARN_IF_UNDOCUMENTED   = NO
"""

    doxyfile_path = os.path.join(output_dir, "Doxyfile")
    with open(doxyfile_path, "w", encoding="utf-8") as f:
        f.write(doxyfile_content)

    proc = subprocess.run(
        [doxygen_bin, doxyfile_path],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
        print(f"ERROR: Doxygen failed with return code {proc.returncode}:\n{proc.stderr}", file=sys.stderr)
        return False
    return True


def is_element_documented(elem: ET.Element | None) -> bool:
    """Checks whether an XML element contains non-empty documentation paragraphs."""
    if elem is None:
        return False
    # Check for text or child tags (e.g. <para>)
    if elem.text and elem.text.strip():
        return True
    return len(elem) > 0


def analyze_xml_coverage(xml_dir: str) -> tuple[int, int, list[tuple[str, str, str, str]]]:
    """
    Parses Doxygen XML output to evaluate docstring coverage.

    Returns:
        tuple containing (total_symbols, documented_symbols, list_of_undocumented_items).
    """
    total = 0
    documented = 0
    undocumented_items: list[tuple[str, str, str, str]] = []

    if not os.path.isdir(xml_dir):
        return 0, 0, []

    for fname in sorted(os.listdir(xml_dir)):
        if not fname.endswith(".xml") or fname in ("index.xml",):
            continue

        xml_path = os.path.join(xml_dir, fname)
        try:
            tree = ET.parse(xml_path)
        except Exception:
            continue

        root = tree.getroot()
        for compound in root.findall("compounddef"):
            c_kind = compound.get("kind", "")
            # Only evaluate structures, classes, and interfaces
            if c_kind in ("file", "dir", "namespace"):
                continue

            c_name = compound.findtext("compoundname", "unknown")
            c_brief = compound.find("briefdescription")
            c_detailed = compound.find("detaileddescription")
            is_c_doc = is_element_documented(c_brief) or is_element_documented(c_detailed)

            total += 1
            if is_c_doc:
                documented += 1
            else:
                loc = compound.find("location")
                file_info = loc.get("file", "") if loc is not None else ""
                line_info = loc.get("line", "") if loc is not None else ""
                undocumented_items.append((c_name, c_kind, file_info, line_info))

            for section in compound.findall("sectiondef"):
                for member in section.findall("memberdef"):
                    m_kind = member.get("kind", "")
                    m_name = member.findtext("name", "")
                    # Ignore friend declarations or anon enumerators
                    if m_kind in ("friend",) or not m_name:
                        continue

                    m_brief = member.find("briefdescription")
                    m_detailed = member.find("detaileddescription")
                    is_m_doc = is_element_documented(m_brief) or is_element_documented(m_detailed)

                    total += 1
                    if is_m_doc:
                        documented += 1
                    else:
                        loc = member.find("location")
                        file_info = loc.get("file", "") if loc is not None else ""
                        line_info = loc.get("line", "") if loc is not None else ""
                        undocumented_items.append((f"{c_name}::{m_name}", m_kind, file_info, line_info))

    return total, documented, undocumented_items


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify minimum Doxygen docstring coverage for C++ codebases."
    )
    parser.add_argument(
        "--min-coverage",
        type=float,
        default=80.0,
        help="Minimum required docstring coverage percentage (default: 80.0).",
    )
    parser.add_argument(
        "--dirs",
        nargs="+",
        default=["engine/src"],
        help="Directories to analyze (default: engine/src).",
    )
    parser.add_argument(
        "--no-private",
        action="store_true",
        help="Exclude private members from the coverage calculation.",
    )
    parser.add_argument(
        "--max-undocumented",
        type=int,
        default=30,
        help="Maximum number of undocumented symbols to display on failure.",
    )
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as tmp_dir:
        success = run_doxygen(args.dirs, tmp_dir, include_private=not args.no_private)
        if not success:
            return 1

        xml_dir = os.path.join(tmp_dir, "xml")
        total, documented, undocumented = analyze_xml_coverage(xml_dir)

        if total == 0:
            print("WARNING: No symbols found to analyze in target directories.", file=sys.stderr)
            return 0

        coverage = (documented / total) * 100.0

        print("=" * 60)
        print("Doxygen Docstring Coverage Report")
        print("=" * 60)
        print(f"Target directories  : {', '.join(args.dirs)}")
        print(f"Documented symbols  : {documented}")
        print(f"Total symbols       : {total}")
        print(f"Current coverage    : {coverage:.1f}%")
        print(f"Required threshold  : {args.min_coverage:.1f}%")
        print("=" * 60)

        if coverage < args.min_coverage:
            print(f"\nFAILED: Coverage {coverage:.1f}% is below required {args.min_coverage:.1f}%\n")
            print(f"Top undocumented symbols ({len(undocumented)} total):")
            for name, kind, file_info, line_info in undocumented[: args.max_undocumented]:
                loc_str = f" ({file_info}:{line_info})" if file_info else ""
                print(f"  - [{kind}] {name}{loc_str}")
            if len(undocumented) > args.max_undocumented:
                print(f"  ... and {len(undocumented) - args.max_undocumented} more.")
            return 1

        print("\nPASSED: Docstring coverage meets or exceeds threshold.\n")
        return 0


if __name__ == "__main__":
    sys.exit(main())
