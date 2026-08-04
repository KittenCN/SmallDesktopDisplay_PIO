# font_translate

`font_translate.py` 是一个无第三方依赖的 Python 3.10+ 命令行工具，用于：

- 从 C/C++ header 的 `uint8_t`、`unsigned char` 或 `byte` 数组提取二进制；
- 从 GB2312 顺序的 HZK 字库生成只包含所需字符的 C header；
- 从 header 注释提取字符；
- 对 UTF-8 字符列表去重。

所有输出命令都会自动创建父目录。参数或输入不合法时命令返回非零退出码，适合在 CI 中使用。

## 从 header 提取二进制

```powershell
python tools/font_translate/font_translate.py extract-h `
  --input src/font/font_td_20.h `
  --out build/font_td_20.bin
```

解析器只接受 0 到 255 的 C 整数常量，忽略注释内容，不会把注释中的数字误当成字节。支持十进制、十六进制、二进制、八进制及整数后缀。

如果 header 包含多个字节数组，必须用 `--array` 明确选择：

```powershell
python tools/font_translate/font_translate.py extract-h `
  --input assets/fonts.h --array font_small `
  --out build/font_small.bin
```

可同时校验固定字形大小并分割文件。数据长度不是字形大小的整数倍时会失败，不会生成不完整字形：

```powershell
python tools/font_translate/font_translate.py extract-h `
  --input src/font/font_td_20.h `
  --out build/font_td_20.bin `
  --glyph-size 32 --out-glyph-dir build/glyphs
```

`--out-glyph-dir` 必须和 `--glyph-size` 一起使用。

## 从 HZK 生成精简 header

准备 GB2312 顺序的 HZK 二进制和 UTF-8 字符列表，然后执行：

```powershell
python tools/font_translate/font_translate.py txt2hzk `
  --hzk assets/hzk16.bin --txt chars.txt `
  --out src/font/font_small.h --name font_small `
  --width 16 --height 16
```

`width` 和 `height` 必须为正数，HZK 文件大小也必须是单字形大小的整数倍。默认情况下，任一字符不能编码为双字节 GB2312 或字形超出 HZK 文件范围，命令都会失败，避免悄悄生成缺字字库。确实允许缺字时可显式增加 `--allow-missing`；即使如此，至少要成功生成一个字形。

生成数组按字符列表去重后的顺序连续存储，每个字形占 `ceil(width / 8) * height` 字节。

## 无损精简 TFT_eSPI VLW 字体

`subset-vlw` 从现有 Processing/TFT_eSPI VLW header 中只保留字符清单指定的字形。工具不会重新栅格化：选中字形的 28 字节 metrics、逐像素 alpha bitmap、原始顺序和字体 footer 都逐字节保留，仅更新字形数量。

日历字体使用仓库内的明确字符契约重新生成：

```powershell
python tools/font_translate/font_translate.py subset-vlw `
  --input src/font/font_td_20.h `
  --chars src/font/font_td_20_chars.txt `
  --out src/font/font_td_20.h `
  --array font_td_20
```

命令会拒绝源字体缺字、重复/非 BMP 码点、截断 bitmap，以及超出 TFT_eSPI 实际整数范围的字形度量。普通空格、Tab 和换行只作为清单排版字符忽略；其他 Unicode 空白不会被静默删除。CI 还要求生成字体的实际字形集合与 manifest 完全相等。

## 从注释提取字符

```powershell
python tools/font_translate/font_translate.py header2chars `
  --input src/font/example.h --out build/chars.txt
```

该命令从 `// ...` 和 `/* ... */` 注释中按首次出现顺序提取 CJK、ASCII 字母数字及常见标点。没有可提取字符时返回失败。

## 字符去重

```powershell
python tools/font_translate/font_translate.py dedupe `
  --txt rawchars.txt --out build/dedup.txt
```

空白字符会被移除，其他字符按首次出现顺序保留。

## 测试

```powershell
python -m unittest discover -s tools/font_translate/tests -v
```

测试仅使用 Python 标准库，不读取真实 HZK 文件，也不写入仓库目录。
