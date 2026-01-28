# Sample SD Card Contents

This folder contains everything you need to get started with SUMI. Just copy all the contents to your SD card's root directory.

## Quick Setup

1. Insert your SD card into your computer
2. Copy everything inside this `sample_sd` folder to the root of your SD card
3. Eject the SD card and insert it into your device
4. You're ready to go!

## What's Included

```
SD Card Root/
├── books/          ← 4 classic novels from Project Gutenberg
│   ├── Alice's Adventures in Wonderland - Lewis Carroll.epub
│   ├── Frankenstein - Mary Wollstonecraft Shelley.epub
│   ├── Middlemarch - George Eliot.epub
│   └── The Blue Castle - L. M. Montgomery.epub
│
├── flashcards/     ← Language learning decks + ASL alphabet
│   ├── spanish-basic.tsv, spanish-phrases.tsv
│   ├── french-basic.tsv, french-phrases.tsv
│   ├── german-basic.tsv, german-phrases.tsv
│   ├── japanese-basic.tsv, japanese-phrases.tsv
│   ├── ... (20+ languages including Klingon and Sindarin!)
│   └── asl/        ← ASL alphabet images
│
├── images/         ← Sample BMP images for the image viewer
│   ├── forest_fox.bmp
│   ├── gothic_castle.bmp
│   ├── mountain_valley.bmp
│   ├── starry_dock.bmp
│   └── ... (10 e-ink optimized wallpapers)
│
├── weather/        ← Weather condition icons (used by Weather app)
│
├── chess/          ← Chess piece sprites (used by Chess app)
│
├── notes/          ← Empty - add your .txt notes here
│
└── maps/           ← Empty - for future offline maps feature
```

## Important Notes

- **Folder names must be lowercase** - `books` not `Books`
- **Don't rename folders** - SUMI looks for these exact names
- **Books need processing** - After copying, open the portal and process the EPUBs before reading
- The `.sumi` and `.config` folders will be created automatically by the firmware

## Book Processing

The included EPUBs are from Project Gutenberg (public domain). Before you can read them:

1. Connect to the SUMI portal (see main README)
2. Go to **Files** → **Books**
3. Click **Process All** or process each book individually
4. Once processed, books are ready to read forever!

## Flashcard Decks

Included language decks:
- **Romance**: Spanish, French, Italian, Portuguese
- **Germanic**: German, Dutch
- **Slavic**: Russian
- **Asian**: Japanese, Korean, Mandarin, Thai, Hindi
- **Middle Eastern**: Arabic, Turkish, Greek
- **Constructed**: Klingon (Star Trek), Sindarin (Tolkien Elvish), Na'vi (Avatar)
- **Sign Language**: ASL Alphabet with hand sign images

Each language has a `-basic.tsv` (common words) and `-phrases.tsv` (useful phrases) deck.

## Adding Your Own Content

- **Books**: Drop `.epub` files into `/books/`, then process via portal
- **Flashcards**: Create `.tsv` files with `question<TAB>answer` format
- **Images**: Add `.bmp` or `.jpg` files to `/images/`
- **Notes**: Add `.txt` files to `/notes/`

Enjoy SUMI!
