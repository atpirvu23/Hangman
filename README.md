# Hangman (C++17)

A fully playable terminal Hangman game written in modern C++. It loads its vocabulary from a text file and picks a random word each round, using a shuffled deck so words don't repeat until the list is exhausted. A two-player mode lets a friend enter a secret word instead.

## Features

- **Random words from file**: reads `cuvinte.txt` (one word per line); falls back to a built-in list if the file is missing or empty
- **Two-player mode**: one player types a secret word, the screen clears, the other guesses
- **3 difficulty levels**: Easy (8 lives), Medium (6 lives), Hard (4 lives); the ASCII gallows scales to the number of lives
- **Hint system**: type `?` to reveal one letter (once per round, costs a life)
- **Whole-word guesses**: a wrong guess costs a life
- **Session statistics**: wins, losses, current streak, best streak, win rate
- **Colored terminal UI** with ANSI escape codes (enabled automatically on Windows 10+)
- **Robust input handling**: validation, case-insensitive input, repeated-letter detection, safe on EOF

## Build and run

Requires a C++11 (or newer) compiler.

```bash
g++ -std=c++17 -O2 -Wall -Wextra spanzuratoarea.cpp -o spanzuratoarea
./spanzuratoarea
```

On Windows (MinGW):

```bash
g++ -std=c++17 -O2 spanzuratoarea.cpp -o spanzuratoarea.exe
spanzuratoarea.exe
```

Run it from the folder that contains `cuvinte.txt`.

## How to play

| Input | Action |
|-------|--------|
| a single letter | guess that letter |
| a full word | guess the whole word |
| `?` | use a hint (1 per round, costs a life) |
| `quit` | abandon the round |

## Custom word list

Edit `cuvinte.txt` and put one word per line. Only lowercase letters `a-z` are accepted (no diacritics), with a minimum length of 3. Other entries are skipped.

## Project structure

```
.
├── spanzuratoarea.cpp   # the whole game (single file)
├── cuvinte.txt          # word list
└── README.md
```

## Implementation notes

- `std::string`, `std::set` and `<random>` (`std::mt19937`) instead of raw `char[]` buffers and `rand()`
- Game state is kept in a `Round` struct; outcomes are modeled with `enum class`
- Cross-platform screen clearing via ANSI codes instead of `system("CLS")`
- Compiles without warnings with `-Wall -Wextra` and was checked with AddressSanitizer and UBSan
