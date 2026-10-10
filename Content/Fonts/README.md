# Fonts

`ShipporiMincho-Medium-Latin.ttf` is a Latin subset of **Shippori Mincho Medium**
by The Shippori Mincho Project Authors, licensed under the SIL Open Font
License 1.1 (see `OFL.txt`). The font declares no Reserved Font Name.

Source: https://github.com/google/fonts/tree/main/ofl/shipporimincho

The full font is ~8.7 MB because it covers Japanese; the engine only needs
Latin text for now, so it was subset with fontTools:

```
pyftsubset ShipporiMincho-Medium.ttf \
  --unicodes="U+0020-007E,U+00A0-00FF,U+2018-201F,U+2026,U+2014,U+2013" \
  --layout-features='kern' \
  --output-file=ShipporiMincho-Medium-Latin.ttf
```
