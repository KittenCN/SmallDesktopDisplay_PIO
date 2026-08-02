# Animation assets

`Animate.cpp` selects one generated frame table through `Animate_Choice` in
`src/config.h`. Choices `1`, `2`, and `3` select astronaut, hutao, and miku;
`0` disables animation. Any other value is rejected at compile time.

Frame counts are derived from the selected pointer and size arrays. The compiler
also verifies that both arrays have identical lengths, so adding or removing
frames does not require a matching magic number in `Animate.cpp`.

## Generate a header

Install Pillow, then run the generator from any working directory:

```powershell
python src/Animate/gif2hex.py path\to\animation.gif `
  --output src\Animate\img\animation.h
```

Useful options are `--max-size 70`, `--quality 85`, `--sample-every N`, and
`--no-preview`. `init.py` is retained as a compatibility entry point and accepts
the same options.

The GIF sequence is read directly, so frame order is independent of filesystem
directory ordering. Header replacement is atomic. When previews are enabled,
only `out_img/<animation-name>` is replaced as a unit; this prevents frames left
by an older, longer GIF from contaminating the new result.

Generated frames are JPEG images composited onto black and fitted within the
configured maximum dimensions. The generated header contains include guards,
PROGMEM frame data, a const pointer table, and an equally sized byte-count table.
