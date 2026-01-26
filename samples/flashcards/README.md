# SUMI Sample Flashcard Decks

A comprehensive collection of language learning flashcards for use with SUMI e-ink reader. Each card includes phonetic pronunciations to help you learn proper speaking.

## 📊 Summary

- **15 Real Languages** × 100 cards each = 1,500 cards
- **3 Fantasy Languages** × 50 cards each = 150 cards
- **1 Visual Language (ASL)** = 26 cards with images
- **Total: 1,676+ flashcards across 37 decks**

## 📱 Installation

Copy the contents of this folder to your SD card's `/flashcards/` directory:

```
SD Card
└── flashcards/
    ├── asl/
    │   ├── a.bmp
    │   ├── b.bmp
    │   └── ... (26 letter images)
    ├── asl-alphabet.tsv
    ├── spanish-basic.tsv
    ├── spanish-phrases.tsv
    ├── japanese-basic.tsv
    └── ... (any decks you want)
```

You don't need to copy all decks - just the ones you want to study!

## 🤟 American Sign Language (ASL) - Visual Flashcards!

SUMI supports **image-based flashcards** - perfect for learning sign language!

The `asl-alphabet.tsv` deck shows you a letter and displays the corresponding ASL hand sign as an image.

![ASL Flashcards on SUMI](../../docs/images/flashcards_asl.jpg)

### ASL Images Included!

The `asl/` folder contains all 26 pre-converted 1-bit BMP images (150×150px). Just copy the entire `samples/flashcards/` folder to your SD card's `/flashcards/` directory and you're ready to go!

### Creating Your Own Image Flashcards

Any flashcard answer (or question) that ends in `.bmp` will be rendered as an image:

```tsv
Question	Answer
What is the sign for A?	/flashcards/asl/a.bmp
Show me 'Hello'	/flashcards/asl/hello.bmp
```

**Image Requirements:**
- Format: 1-bit monochrome BMP (black & white)
- Size: ~150x150 pixels recommended
- Location: Must be accessible path on SD card

## 🌍 Real Languages (100 cards each)

| Language | Basic Words | Phrases | Total |
|----------|-------------|---------|-------|
| 🇪🇸 Spanish | 50 | 50 | 100 |
| 🇫🇷 French | 50 | 50 | 100 |
| 🇩🇪 German | 50 | 50 | 100 |
| 🇮🇹 Italian | 50 | 50 | 100 |
| 🇵🇹 Portuguese | 50 | 50 | 100 |
| 🇯🇵 Japanese | 50 | 50 | 100 |
| 🇰🇷 Korean | 50 | 50 | 100 |
| 🇨🇳 Mandarin Chinese | 50 | 50 | 100 |
| 🇸🇦 Arabic | 50 | 50 | 100 |
| 🇷🇺 Russian | 50 | 50 | 100 |
| 🇮🇳 Hindi | 50 | 50 | 100 |
| 🇳🇱 Dutch | 50 | 50 | 100 |
| 🇬🇷 Greek | 50 | 50 | 100 |
| 🇹🇷 Turkish | 50 | 50 | 100 |
| 🇹🇭 Thai | 50 | 50 | 100 |

## 🧝 Fantasy Languages (50 cards each)

| Language | Source | Basic | Phrases | Total |
|----------|--------|-------|---------|-------|
| Na'vi | Avatar | 25 | 25 | 50 |
| Klingon | Star Trek | 25 | 25 | 50 |
| Sindarin Elvish | Lord of the Rings | 25 | 25 | 50 |

## 📁 Format

All files are **TSV (Tab-Separated Values)**:

```
English	Translation (Pronunciation)
Hello	Hola (OH-lah)
Thank you	Gracias (GRAH-see-ahs)
```

Compatible with:
- ✅ SUMI flashcard app (native)
- ✅ Anki (import as TSV)
- ✅ Quizlet (import as TSV)
- ✅ Any spreadsheet app

## 📚 Deck Contents

### Basic Words (50 per language)
- Greetings & farewells
- Polite expressions (please, thank you, sorry)
- Common words (yes, no, water, food)
- Numbers (1-5, 10)
- Family & relationships
- Places (bathroom, restaurant, hotel, airport)
- Transportation (train, bus, taxi)
- Directions (left, right, straight)
- Adjectives (big, small, hot, cold, good, bad)

### Phrases (50 per language)
- Navigation & directions
- Shopping & prices
- Dining & food preferences
- Communication basics
- Emergency phrases
- Travel essentials
- Social interactions
- Common questions

## 🔊 Pronunciation Guide

Each card includes phonetic pronunciation:
- **CAPS** = stressed syllable
- Hyphens separate syllables
- English approximations for sounds

Example: `Gracias (GRAH-see-ahs)`

## 📱 Usage with SUMI

1. Copy `.tsv` files to `/flashcards/` on SD card
2. Open Flashcards app on SUMI
3. Select a deck and start learning!

**Tips:**
- Enable "Shuffle Cards" for randomized review
- Use "Center Text" for cleaner display
- Try "Extra Large" font for quick glance review

## 🤝 Contributing

Corrections and additional languages welcome via pull request!

## 📄 License

Free for personal and educational use.
