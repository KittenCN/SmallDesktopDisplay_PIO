"""Compatibility CLI for the animation GIF-to-header generator.

The old script used intermediate PNG/JPEG directories and unsorted directory reads,
which made frame order depend on the host filesystem and allowed stale frames into a
new header. The canonical generator now reads the GIF sequence directly.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path

from gif2hex import DEFAULT_JPEG_QUALITY, DEFAULT_MAX_SIZE, processImage


def init(
    file_name_all: str | os.PathLike[str],
    *,
    output_file: str | os.PathLike[str] | None = None,
    max_size: int = DEFAULT_MAX_SIZE,
    jpeg_quality: int = DEFAULT_JPEG_QUALITY,
    sample_every: int = 1,
    save_preview: bool = True,
) -> Path:
    """Generate a header directly from the GIF's deterministic frame sequence."""
    return processImage(
        file_name_all,
        saveImg=save_preview,
        output_file=output_file,
        max_size=max_size,
        jpeg_quality=jpeg_quality,
        sample_every=sample_every,
    )


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("gif", help="input GIF")
    parser.add_argument("-o", "--output", help="output header (default: next to the GIF)")
    parser.add_argument("--max-size", type=int, default=DEFAULT_MAX_SIZE)
    parser.add_argument("--quality", type=int, default=DEFAULT_JPEG_QUALITY)
    parser.add_argument("--sample-every", type=int, default=1)
    parser.add_argument("--no-preview", action="store_true")
    return parser


def main() -> None:
    args = _build_parser().parse_args()
    init(
        args.gif,
        output_file=args.output,
        max_size=args.max_size,
        jpeg_quality=args.quality,
        sample_every=args.sample_every,
        save_preview=not args.no_preview,
    )


if __name__ == "__main__":
    main()
