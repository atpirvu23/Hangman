// =====================================================================
//  SPANZURATOAREA  -  Hangman in C++17
//  Functioneaza pe Windows / Linux / macOS (culori ANSI + ASCII art)
//  Compilare:  g++ -std=c++17 -O2 -Wall -Wextra spanzuratoarea.cpp -o spanzuratoarea
//  (necesita minim C++11: in Code::Blocks / Dev-C++ seteaza standardul pe C++11 sau mai nou)
// =====================================================================
#include <algorithm>
#include <cstddef>
#include <ctime>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace color {
const std::string reset  = "\033[0m";
const std::string bold   = "\033[1m";
const std::string dim    = "\033[2m";
const std::string red    = "\033[31m";
const std::string green  = "\033[32m";
const std::string yellow = "\033[33m";
const std::string cyan   = "\033[36m";
}  // namespace color

namespace {

constexpr const char* WORDS_FILE = "cuvinte.txt";
constexpr int MIN_WORD_LEN = 3;

// Folosit daca cuvinte.txt lipseste sau e gol
const std::vector<std::string> FALLBACK_WORDS = {
    "procesor", "algoritm", "memorie", "registru", "pointer",
    "compilator", "tranzistor", "semnal", "circuit", "program"};

enum class Difficulty { Easy, Medium, Hard };
enum class Result { Won, Lost, Quit };

struct Stats {
    int wins = 0;
    int losses = 0;
    int streak = 0;
    int bestStreak = 0;
};

// Starea unei runde
struct Round {
    std::string word;
    std::set<char> guessed;
    std::vector<char> wrong;
    int mistakes = 0;
    int lives = 6;
    int hints = 1;

    bool solved() const {
        return std::all_of(word.begin(), word.end(),
                           [this](char c) { return guessed.count(c) > 0; });
    }
    std::string masked() const {
        std::string out;
        for (char c : word) {
            out += guessed.count(c) ? c : '_';
            out += ' ';
        }
        return out;
    }
};

// ------------------------------ utilitare ------------------------------

int livesFor(Difficulty d) {
    switch (d) {
        case Difficulty::Easy:   return 8;
        case Difficulty::Medium: return 6;
        case Difficulty::Hard:   return 4;
    }
    return 6;
}

std::string nameFor(Difficulty d) {
    switch (d) {
        case Difficulty::Easy:   return "Usor (8 vieti)";
        case Difficulty::Medium: return "Mediu (6 vieti)";
        case Difficulty::Hard:   return "Greu (4 vieti)";
    }
    return "";
}

void enableAnsi() {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode))
        SetConsoleMode(h, mode | 0x0004);  // ENABLE_VIRTUAL_TERMINAL_PROCESSING
#endif
}

void clearScreen() { std::cout << "\033[2J\033[H"; }

bool isLowerAlpha(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(),
                                     [](char c) { return c >= 'a' && c <= 'z'; });
}

std::string toLower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    std::size_t a = s.find_first_not_of(ws);
    if (a == std::string::npos) return "";
    std::size_t b = s.find_last_not_of(ws);
    return s.substr(a, b - a + 1);
}

// Citeste o linie (trim + litere mici). Returneaza false la EOF.
bool readLine(const std::string& prompt, std::string& out) {
    std::cout << prompt << std::flush;
    if (!std::getline(std::cin, out)) return false;
    out = toLower(trim(out));
    return true;
}

void waitEnter() {
    std::string tmp;
    std::cout << color::dim << "\n  Apasa ENTER pentru a continua..." << color::reset << std::flush;
    std::getline(std::cin, tmp);
}

std::vector<std::string> loadWords(const std::string& path) {
    std::vector<std::string> words;
    std::ifstream f(path);
    std::string w;
    while (f >> w) {
        w = toLower(w);
        if (isLowerAlpha(w) && static_cast<int>(w.size()) >= MIN_WORD_LEN) words.push_back(w);
    }
    return words;
}

// ------------------------------ desen ------------------------------

void drawGallows(int mistakes, int lives) {
    // 7 stadii (0 = gol ... 6 = spanzurat), scalate la numarul de vieti
    int stage = (mistakes * 6 + lives - 1) / lives;
    if (stage > 6) stage = 6;

    static const char* head[7]  = {"      |", "  O   |", "  O   |", "  O   |", "  O   |", "  O   |", "  O   |"};
    static const char* torso[7] = {"      |", "      |", "  |   |", " /|   |", " /|\\  |", " /|\\  |", " /|\\  |"};
    static const char* legs[7]  = {"      |", "      |", "      |", "      |", "      |", " /    |", " / \\  |"};

    const std::string& c = stage >= 5 ? color::red : (stage >= 3 ? color::yellow : color::cyan);
    std::cout << c;
    std::cout << "    +---+\n";
    std::cout << "    |   |\n";
    std::cout << "    " << head[stage] << "\n";
    std::cout << "    " << torso[stage] << "\n";
    std::cout << "    " << legs[stage] << "\n";
    std::cout << "        |\n";
    std::cout << "  =========\n";
    std::cout << color::reset;
}

void drawScreen(const Round& r, const std::string& msg) {
    clearScreen();
    std::cout << color::bold << color::cyan
              << "  ==== SPANZURATOAREA ====\n\n" << color::reset;
    drawGallows(r.mistakes, r.lives);

    std::cout << "\n  Cuvant:  " << color::bold << r.masked() << color::reset << "  ("
              << r.word.size() << " litere)\n";

    std::cout << "  Vieti:   [";
    for (int i = 0; i < r.lives; ++i)
        std::cout << (i < r.lives - r.mistakes ? color::green + "#" : color::red + "-") << color::reset;
    std::cout << "]  " << (r.lives - r.mistakes) << "/" << r.lives << "\n";

    std::cout << "  Gresite: ";
    if (r.wrong.empty()) std::cout << color::dim << "-" << color::reset;
    for (char c : r.wrong) std::cout << color::red << c << ' ' << color::reset;
    std::cout << "\n  Indicii: " << r.hints << (r.hints == 1 ? " disponibila" : " disponibile")
              << color::dim << "  (scrie ? ; costa o viata)" << color::reset << "\n";

    if (!msg.empty()) std::cout << "\n  " << msg << "\n";
    std::cout << "\n";
}

// ------------------------------ logica rundei ------------------------------

Result playRound(Round& r, std::mt19937& rng) {
    std::string msg;
    std::string in;

    while (true) {
        drawScreen(r, msg);
        if (!readLine("  Litera / cuvant intreg / ? / quit > ", in)) return Result::Quit;
        msg.clear();

        if (in == "quit" || in == "exit") return Result::Quit;

        if (in == "?") {
            if (r.hints <= 0) {
                msg = color::yellow + "Nu mai ai indicii!" + color::reset;
            } else if (r.mistakes + 1 >= r.lives) {
                msg = color::yellow + "Indiciul te-ar costa ultima viata. Nu il pot da!" + color::reset;
            } else {
                std::vector<char> hidden;
                for (char c : r.word)
                    if (!r.guessed.count(c) && std::find(hidden.begin(), hidden.end(), c) == hidden.end())
                        hidden.push_back(c);
                if (!hidden.empty()) {
                    char c = hidden[std::uniform_int_distribution<std::size_t>(0, hidden.size() - 1)(rng)];
                    r.guessed.insert(c);
                    --r.hints;
                    ++r.mistakes;
                    msg = color::cyan + std::string("Indiciu: litera '") + c + "' e in cuvant! (-1 viata)" + color::reset;
                }
            }
        } else if (!isLowerAlpha(in)) {
            msg = color::yellow + "Intrare invalida. Foloseste doar litere a-z (fara diacritice)." + color::reset;
        } else if (in.size() == 1) {
            char c = in[0];
            if (r.guessed.count(c) || std::find(r.wrong.begin(), r.wrong.end(), c) != r.wrong.end()) {
                msg = color::yellow + "Ai incercat deja litera '" + std::string(1, c) + "'." + color::reset;
            } else if (r.word.find(c) != std::string::npos) {
                r.guessed.insert(c);
                int cnt = static_cast<int>(std::count(r.word.begin(), r.word.end(), c));
                msg = color::green + "Bravo! Litera '" + std::string(1, c) + "' apare de " +
                      std::to_string(cnt) + (cnt == 1 ? " data." : " ori.") + color::reset;
            } else {
                r.wrong.push_back(c);
                ++r.mistakes;
                msg = color::red + "Litera '" + std::string(1, c) + "' nu exista in cuvant." + color::reset;
            }
        } else {  // ghicire cuvant intreg
            if (in == r.word) {
                for (char c : r.word) r.guessed.insert(c);
            } else {
                ++r.mistakes;
                msg = color::red + "Cuvantul \"" + in + "\" nu este cel corect! (-1 viata)" + color::reset;
            }
        }

        if (r.solved()) return Result::Won;
        if (r.mistakes >= r.lives) return Result::Lost;
    }
}

void showResult(const Round& r, Result res, Stats& st) {
    drawScreen(r, "");
    if (res == Result::Won) {
        ++st.wins;
        ++st.streak;
        st.bestStreak = (std::max)(st.bestStreak, st.streak);
        std::cout << color::bold << color::green << "  ATI CASTIGAT! FELICITARI!" << color::reset
                  << "\n  Cuvantul era: " << color::bold << r.word << color::reset
                  << "  (greseli: " << r.mistakes << ")\n";
    } else if (res == Result::Lost) {
        ++st.losses;
        st.streak = 0;
        std::cout << color::bold << color::red << "  AI PIERDUT!" << color::reset
                  << "\n  Cuvantul era: " << color::bold << r.word << color::reset << "\n";
    }
}

void showStats(const Stats& st) {
    clearScreen();
    int total = st.wins + st.losses;
    std::cout << color::bold << color::cyan << "  ==== STATISTICI ====\n\n" << color::reset;
    std::cout << "  Jocuri jucate:   " << total << "\n";
    std::cout << "  Victorii:        " << color::green << st.wins << color::reset << "\n";
    std::cout << "  Infrangeri:      " << color::red << st.losses << color::reset << "\n";
    std::cout << "  Serie curenta:   " << st.streak << "\n";
    std::cout << "  Cea mai buna:    " << st.bestStreak << "\n";
    if (total > 0) std::cout << "  Rata de castig:  " << (100 * st.wins / total) << "%\n";
    waitEnter();
}

void showRules() {
    clearScreen();
    std::cout << color::bold << color::cyan << "  ==== REGULI ====\n\n" << color::reset
              << "  - Ghiceste cuvantul litera cu litera, inainte sa se termine vietile.\n"
              << "  - Poti scrie si cuvantul intreg, dar daca gresesti pierzi o viata.\n"
              << "  - Scrie ?  pentru un indiciu (1 pe runda, costa o viata).\n"
              << "  - Scrie quit  pentru a abandona runda.\n"
              << "  - Cuvintele se citesc din cuvinte.txt (cate unul pe linie).\n";
    waitEnter();
}

Difficulty chooseDifficulty(Difficulty current) {
    clearScreen();
    std::cout << color::bold << color::cyan << "  ==== DIFICULTATE ====\n\n" << color::reset
              << "  1. " << nameFor(Difficulty::Easy) << "\n"
              << "  2. " << nameFor(Difficulty::Medium) << "\n"
              << "  3. " << nameFor(Difficulty::Hard) << "\n\n";
    std::string in;
    if (!readLine("  Alege (1-3) > ", in)) return current;
    if (in == "1") return Difficulty::Easy;
    if (in == "2") return Difficulty::Medium;
    if (in == "3") return Difficulty::Hard;
    return current;
}

// Urmatorul cuvant dintr-un "pachet" amestecat (fara repetitii pana se epuizeaza)
std::string nextWord(std::vector<std::string>& deck, const std::vector<std::string>& all, std::mt19937& rng) {
    if (deck.empty()) {
        deck = all;
        std::shuffle(deck.begin(), deck.end(), rng);
    }
    std::string w = deck.back();
    deck.pop_back();
    return w;
}

void printMenu(Difficulty diff, std::size_t wordCount) {
    clearScreen();
    std::cout << color::bold << color::cyan
              << "   ____                                       _\n"
              << "  / ___| _ __   __ _ _ __  ____   _ _ __ __ _| |_ ___   __ _ _ __ ___  __ _\n"
              << "  \\___ \\| '_ \\ / _` | '_ \\|_  / | | | '__/ _` | __/ _ \\ / _` | '__/ _ \\/ _` |\n"
              << "   ___) | |_) | (_| | | | |/ /| |_| | | | (_| | || (_) | (_| | | |  __/ (_| |\n"
              << "  |____/| .__/ \\__,_|_| |_/___|\\__,_|_|  \\__,_|\\__\\___/ \\__,_|_|  \\___|\\__,_|\n"
              << "        |_|\n"
              << color::reset << "\n";
    std::cout << "  1. Joc nou (cuvant aleatoriu din fisier)\n"
              << "  2. Joc in doi (un prieten alege cuvantul)\n"
              << "  3. Dificultate: " << color::yellow << nameFor(diff) << color::reset << "\n"
              << "  4. Statistici\n"
              << "  5. Reguli\n"
              << "  0. Iesire\n\n"
              << color::dim << "  " << wordCount << " cuvinte incarcate" << color::reset << "\n\n";
}

}  // namespace

int main() {
    enableAnsi();

    std::mt19937 rng(std::random_device{}() ^ static_cast<unsigned>(std::time(nullptr)));

    std::vector<std::string> words = loadWords(WORDS_FILE);
    bool usingFallback = words.empty();
    if (usingFallback) words = FALLBACK_WORDS;

    std::vector<std::string> deck;
    Difficulty diff = Difficulty::Medium;
    Stats stats;

    if (usingFallback) {
        std::cout << color::yellow << "Nu am gasit " << WORDS_FILE
                  << " (sau e gol). Folosesc lista implicita." << color::reset << "\n";
        waitEnter();
    }

    while (true) {
        printMenu(diff, words.size());
        std::string choice;
        if (!readLine("  Alege > ", choice)) break;

        if (choice == "0") break;

        if (choice == "1" || choice == "2") {
            Round r;
            r.lives = livesFor(diff);

            if (choice == "1") {
                r.word = nextWord(deck, words, rng);
            } else {
                std::string secret;
                clearScreen();
                std::cout << color::bold << "  JOC IN DOI\n" << color::reset
                          << "  Jucatorul 1 scrie cuvantul secret (doar litere a-z, min " << MIN_WORD_LEN
                          << "), jucatorul 2 sa nu se uite!\n\n";
                if (!readLine("  Cuvant secret > ", secret)) break;
                if (!isLowerAlpha(secret) || static_cast<int>(secret.size()) < MIN_WORD_LEN) {
                    std::cout << color::yellow << "\n  Cuvant invalid.\n" << color::reset;
                    waitEnter();
                    continue;
                }
                r.word = secret;
                clearScreen();  // ascunde cuvantul
            }

            Result res = playRound(r, rng);
            if (res == Result::Quit) {
                std::cout << "\n  Runda abandonata. Cuvantul era: " << color::bold << r.word << color::reset << "\n";
            } else {
                showResult(r, res, stats);
            }
            waitEnter();
        } else if (choice == "3") {
            diff = chooseDifficulty(diff);
        } else if (choice == "4") {
            showStats(stats);
        } else if (choice == "5") {
            showRules();
        }
    }

    std::cout << "\n  La revedere!\n";
    return 0;
}