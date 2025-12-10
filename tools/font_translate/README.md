font_translate tool

用途：
- 从 C header（类似 `const uint8_t font_td_20[] = { ... };`）提取二进制数据
- 从 HZK（二进制，GB2312 顺序，如 HZK16）按 txt 列表生成精简 .h 字库，仅包含所需汉字
- 去重 txt 字符

示例：

1) 提取 header 到 binary：

```
python tools/font_translate/font_translate.py extract-h --input src/font/font_td_20.h --out build/font_td_20.bin
```

若你知道每个字模的字节大小（例如 16x16 -> 32 bytes），可以同时分割：

```
python tools/font_translate/font_translate.py extract-h --input src/font/font_td_20.h --out build/font_td_20.bin --glyph-size 32 --out-glyph-dir build/glyphs
```

2) 从 HZK16 二进制和字符 txt 生成精简 .h（推荐流程）：
- 在 PC 上准备 `hzk16.bin`（GB2312 排列的位图字库）并放在项目或指定路径
- 准备包含要显示汉字的 `chars.txt`（UTF-8 编码，字符连续即可）

执行：

```
python tools/font_translate/font_translate.py txt2hzk --hzk /path/to/hzk16.bin --txt chars.txt --out src/font/font_small.h --name font_small --width 16 --height 16
```

3) 去重 txt：

```
python tools/font_translate/font_translate.py dedupe --txt rawchars.txt --out dedup.txt
```

注意：
- 生成 .h 依赖 HZK 的字模排列与制定的尺寸（width/height）。若 HZK 排列或尺寸不同，请调整参数或准备相应的 HZK 文件。
- 如果你的原始 `font_td_20.h` 是一种自定义排列（并非 GB2312 HZK），你可以先用 `extract-h` 得到二进制，再用 `--glyph-size` 切分，最后手动或借助 `chars.txt` 将字符与字模一一对应。

后续我可以：
- 把脚本扩展为直接识别常见 header 的注释中字符映射并自动生成 txt（若你的 `font_td_20.h` 中带有字符注释）
- 添加将生成的 .h 自动集成到项目的示例调用（platformio 上传 LittleFS, etc.）

