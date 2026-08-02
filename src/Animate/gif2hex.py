"""Convert a GIF into deterministic JPEG frame tables for ESP8266 PROGMEM."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import tempfile
from io import BytesIO
from pathlib import Path
from typing import Sequence

from PIL import Image, ImageSequence


DEFAULT_MAX_SIZE = 70
DEFAULT_JPEG_QUALITY = 85


def _resolve_input(in_file: str | os.PathLike[str]) -> Path:
    """Resolve CLI paths from cwd, with script-relative fallback for compatibility."""
    input_path = Path(in_file).expanduser()
    if input_path.is_absolute() or input_path.exists():
        return input_path.resolve()

    script_relative = Path(__file__).resolve().parent / input_path
    return script_relative.resolve()


def _c_identifier(value: str) -> str:
    identifier = re.sub(r"[^0-9A-Za-z_]", "_", value)
    if not identifier:
        identifier = "animation"
    if identifier[0].isdigit():
        identifier = f"animation_{identifier}"
    return identifier


def _resample_lanczos() -> int:
    # Pillow >= 9 exposes Resampling; older supported versions expose LANCZOS directly.
    resampling = getattr(Image, "Resampling", Image)
    return resampling.LANCZOS


def _to_jpeg(frame: Image.Image, max_size: int, quality: int) -> bytes:
    rgba = frame.convert("RGBA")
    rgba.thumbnail((max_size, max_size), _resample_lanczos())

    # JPEG has no alpha channel. The TFT background is black, so composite transparent
    # GIF pixels onto black instead of relying on palette/transparency side effects.
    rgb = Image.new("RGB", rgba.size, (0, 0, 0))
    rgb.paste(rgba, mask=rgba.getchannel("A"))

    output = BytesIO()
    rgb.save(
        output,
        format="JPEG",
        quality=quality,
        optimize=False,
        progressive=False,
        subsampling=2,
    )
    return output.getvalue()


def encode_gif(
    input_path: Path,
    *,
    max_size: int = DEFAULT_MAX_SIZE,
    jpeg_quality: int = DEFAULT_JPEG_QUALITY,
    sample_every: int = 1,
) -> list[bytes]:
    """Encode GIF frames in source order; no filesystem ordering is involved."""
    if max_size <= 0:
        raise ValueError("max_size must be positive")
    if not 1 <= jpeg_quality <= 100:
        raise ValueError("jpeg_quality must be in the range 1..100")
    if sample_every <= 0:
        raise ValueError("sample_every must be positive")
    if not input_path.is_file():
        raise FileNotFoundError(f"GIF not found: {input_path}")

    frames: list[bytes] = []
    with Image.open(input_path) as image:
        if image.format != "GIF":
            raise ValueError(f"expected a GIF file, got {image.format or 'unknown'}")

        # ImageSequence follows the GIF's frame order and Pillow applies disposal while
        # seeking. Convert each frame immediately so later seeks cannot mutate it.
        for index, frame in enumerate(ImageSequence.Iterator(image)):
            if index % sample_every == 0:
                frames.append(_to_jpeg(frame.copy(), max_size, jpeg_quality))

    if not frames:
        raise ValueError("GIF contains no selected frames")
    return frames


def render_header(animation_name: str, frames: Sequence[bytes]) -> str:
    """Render one self-contained header whose tables describe their own length."""
    if not frames:
        raise ValueError("at least one frame is required")

    identifier = _c_identifier(animation_name)
    guard = f"ANIMATE_GENERATED_{identifier.upper()}_H"
    frame_names = [f"{identifier}_{index}" for index in range(len(frames))]
    lines = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        "#include <pgmspace.h>",
        "#include <stdint.h>",
        "",
    ]

    for frame_name, frame_bytes in zip(frame_names, frames):
        lines.append(f"const uint8_t {frame_name}[] PROGMEM = {{")
        for offset in range(0, len(frame_bytes), 16):
            chunk = frame_bytes[offset : offset + 16]
            lines.append("    " + ", ".join(f"0x{value:02X}" for value in chunk) + ",")
        lines.extend(["};", ""])

    lines.append(
        f"const uint8_t *const {identifier}[{len(frames)}] PROGMEM = "
        "{" + ", ".join(frame_names) + "};"
    )
    lines.append(
        f"const uint32_t {identifier}_size[{len(frames)}] PROGMEM = "
        "{" + ", ".join(str(len(frame)) for frame in frames) + "};"
    )
    lines.extend(["", f"#endif  // {guard}", ""])
    return "\n".join(lines)


def _atomic_write_text(output_path: Path, content: str) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temp_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            newline="\n",
            dir=output_path.parent,
            prefix=f".{output_path.name}.",
            suffix=".tmp",
            delete=False,
        ) as temp_file:
            temp_file.write(content)
            temp_name = temp_file.name
        os.replace(temp_name, output_path)
        temp_name = None
    finally:
        if temp_name is not None:
            Path(temp_name).unlink(missing_ok=True)


def _replace_previews(preview_path: Path, frames: Sequence[bytes]) -> None:
    """Replace one animation's preview directory so stale frames cannot leak in."""
    preview_path.parent.mkdir(parents=True, exist_ok=True)
    temp_path = Path(tempfile.mkdtemp(prefix=f".{preview_path.name}.", dir=preview_path.parent))
    try:
        width = max(3, len(str(len(frames) - 1)))
        for index, frame in enumerate(frames):
            (temp_path / f"{index:0{width}d}.jpg").write_bytes(frame)

        if preview_path.exists():
            shutil.rmtree(preview_path)
        temp_path.replace(preview_path)
    finally:
        if temp_path.exists():
            shutil.rmtree(temp_path)


def processImage(
    in_file: str | os.PathLike[str],
    saveImg: bool = True,
    *,
    output_file: str | os.PathLike[str] | None = None,
    max_size: int = DEFAULT_MAX_SIZE,
    jpeg_quality: int = DEFAULT_JPEG_QUALITY,
    sample_every: int = 1,
) -> Path:
    """Backward-compatible entry point used by the original conversion workflow."""
    input_path = _resolve_input(in_file)
    animation_name = _c_identifier(input_path.stem)
    output_path = (
        Path(output_file).expanduser().resolve()
        if output_file is not None
        else input_path.with_suffix(".h")
    )

    frames = encode_gif(
        input_path,
        max_size=max_size,
        jpeg_quality=jpeg_quality,
        sample_every=sample_every,
    )
    _atomic_write_text(output_path, render_header(animation_name, frames))

    if saveImg:
        _replace_previews(input_path.parent / "out_img" / animation_name, frames)

    print(f"Generated {output_path} with {len(frames)} frame(s)")
    return output_path


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("gif", help="input GIF; relative paths are resolved from cwd")
    parser.add_argument("-o", "--output", help="output header (default: next to the GIF)")
    parser.add_argument("--max-size", type=int, default=DEFAULT_MAX_SIZE, help="maximum frame width/height")
    parser.add_argument("--quality", type=int, default=DEFAULT_JPEG_QUALITY, help="JPEG quality, 1..100")
    parser.add_argument("--sample-every", type=int, default=1, help="keep every Nth source frame")
    parser.add_argument("--no-preview", action="store_true", help="do not replace out_img/<name> previews")
    return parser


def main() -> None:
    args = _build_parser().parse_args()
    processImage(
        args.gif,
        saveImg=not args.no_preview,
        output_file=args.output,
        max_size=args.max_size,
        jpeg_quality=args.quality,
        sample_every=args.sample_every,
    )


if __name__ == "__main__":
    main()
