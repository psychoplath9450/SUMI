# Chess Pieces for SUMI

These 32x32 1-bit BMP icons provide optional high-quality chess piece graphics.

## Installation

Copy the entire `chess` folder to the root of your SD card:
```
SD Card Root/
├── chess/
│   ├── pawn_w.bmp
│   ├── pawn_b.bmp
│   ├── rook_w.bmp
│   ├── rook_b.bmp
│   ├── knight_w.bmp
│   ├── knight_b.bmp
│   ├── bishop_w.bmp
│   ├── bishop_b.bmp
│   ├── queen_w.bmp
│   ├── queen_b.bmp
│   ├── king_w.bmp
│   └── king_b.bmp
```

## File Naming Convention

- `_w` suffix = White piece (outline style)
- `_b` suffix = Black piece (filled style)

## Note

If the BMP files are not found, the game will automatically fall back to the built-in 16x16 embedded bitmaps. The BMP versions provide higher detail on larger board sizes.
