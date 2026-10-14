#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace solitaire {

enum class Suit : std::uint8_t { Spades, Hearts, Diamonds, Clubs };
constexpr int SuitCount = 4;
constexpr int RankCount = 13;
constexpr int DeckSize = 52;

// A card is its id (copy * 52 + suit * 13 + rank - 1) plus whether it shows
// its face. `copy` tells apart the identical cards of a two-deck game.
struct Card {
    std::uint16_t id = 0;
    bool faceUp = false;

    constexpr int rank() const noexcept { return id % RankCount + 1; }
    constexpr Suit suit() const noexcept { return static_cast<Suit>(id % DeckSize / RankCount); }
    constexpr int copy() const noexcept { return id / DeckSize; }
    constexpr bool red() const noexcept { return suit() == Suit::Hearts || suit() == Suit::Diamonds; }
    constexpr bool operator==(const Card&) const = default;
};

constexpr Card makeCard(Suit suit, int rank, bool faceUp = true, int copy = 0) noexcept
{
    return { static_cast<std::uint16_t>(copy * DeckSize + int(suit) * RankCount + rank - 1), faceUp };
}

using Pile = std::vector<Card>;

// The games. Klondike's option is how many cards a turn of the stock deals
// (1 or 3); Spider's is how many suits it uses (1, 2 or 4).
enum class Variant : std::uint8_t { Klondike, FreeCell, Spider, Yukon, Pyramid, TriPeaks, Golf };
constexpr int VariantCount = 7;

// Cells are FreeCell's spare places. In Pyramid and TriPeaks each tableau
// pile is one place in the layout, holding one card or none.
enum class PileKind : std::uint8_t { Stock, Waste, Foundation, Tableau, Cell };

// A pile on the table. Foundation `index` is the suit it builds in Klondike,
// FreeCell and Yukon.
struct PileRef {
    PileKind kind = PileKind::Stock;
    int index = 0;
    constexpr bool operator==(const PileRef&) const = default;
};

// Moving the cards of `from` starting at position `start` onto `to`. In
// Pyramid, a card moved onto another card (in the tableau or the waste)
// pairs them up and both go to the foundation.
struct Move {
    PileRef from;
    int start = 0;
    PileRef to;
    constexpr bool operator==(const Move&) const = default;
};

// A suggested next step: a card move, or turning the stock.
struct Hint {
    bool draw = false;
    Move move;
    constexpr bool operator==(const Hint&) const = default;
};

enum class DrawResult { Nothing, Drew, Recycled };

// What a successful move did, so the app can pick the right sound.
struct MoveResult {
    bool ok = false;
    bool flipped = false; // a face-down card was turned over
    bool toFoundation = false; // cards were put away (home, paired or played off)
};

// The state and rules of one game of any variant:
// - Klondike: seven columns built down in alternating colours, foundations
//   built up by suit, a stock dealt one or three at a time, endless passes.
// - FreeCell: every card dealt face up into eight columns, with four cells
//   that hold one card each. Runs move as far as the free space allows.
// - Spider: two decks in ten columns, built down regardless of suit. Only
//   same-suit runs move together, and a full king-to-ace run goes home.
//   The stock deals one card onto every column, when none is empty.
// - Yukon: Klondike's columns with most cards face up and no stock; any
//   face-up card moves with whatever lies on it.
// - Pyramid: pairs of uncovered cards adding up to 13 are removed (kings
//   alone). The stock turns one card at a time, three passes in all.
// - TriPeaks: uncovered cards are played onto the waste one rank above or
//   below its top card, wrapping from king to ace; the stock has one pass.
// - Golf: as TriPeaks with seven fanned columns, no wrapping, and nothing
//   played on a king.
class Game {
public:
    Game() = default;

    // Shuffles with `seed` and deals the variant's opening layout.
    static Game deal(Variant variant, std::uint64_t seed, int option = 0);

    Variant variant() const noexcept { return m_variant; }
    int option() const noexcept { return m_option; }
    int drawCount() const noexcept { return m_variant == Variant::Klondike ? m_option : 1; }
    int deckSize() const noexcept { return m_variant == Variant::Spider ? 2 * DeckSize : DeckSize; }
    // Every card id in this game's deck, in order.
    std::vector<int> deck() const;

    int foundationCount() const noexcept { return int(m_foundations.size()); }
    int tableauCount() const noexcept { return int(m_tableau.size()); }
    int cellCount() const noexcept { return int(m_cells.size()); }
    // Pyramid's stock passes left after the current one (-1: endless).
    int passesLeft() const noexcept;

    const Pile& pile(PileRef ref) const;
    const Pile& stock() const noexcept { return m_stock; }
    const Pile& waste() const noexcept { return m_waste; }
    const Pile& foundation(int index) const { return m_foundations.at(index); }
    const Pile& tableau(int index) const { return m_tableau.at(index); }
    const Pile& cell(int index) const { return m_cells.at(index); }

    // Whether turning the stock would do anything.
    bool canDraw() const;
    // Deals from the stock, or turns the waste back over.
    DrawResult draw();

    // Whether the card at `start` of `from` can be picked up, with every
    // card on it, to be moved somewhere.
    bool canPick(PileRef from, int start) const;
    // Pyramid and TriPeaks: whether the place holds a card nothing covers.
    bool uncovered(int index) const;
    bool canMove(const Move& move) const;
    MoveResult move(const Move& move);

    // Where a double tap on the card at `start` of `from` should send it:
    // home if possible, otherwise the leftmost column it fits on (FreeCell
    // then tries a free cell). In Pyramid, a king goes, or the first card it
    // pairs with; in TriPeaks and Golf the card goes onto the waste.
    std::optional<Move> bestMove(PileRef from, int start) const;

    // Every legal move, plus turning the stock when that does anything.
    std::vector<Hint> legalMoves() const;

    // A helpful next step, or nothing if no move makes progress.
    std::optional<Hint> hint() const;

    // The rest of the game can be played out without thought: Klondike with
    // every card face up; FreeCell and Yukon once every column runs down.
    bool canAutoFinish() const;
    // The next step when finishing, lowest card first.
    std::optional<Hint> autoFinishStep() const;

    // Progress toward a win: cards home (or cleared from the layout in
    // Pyramid, TriPeaks and Golf) out of `goal`.
    int progress() const;
    int goal() const;
    int foundationCards() const;
    bool won() const { return progress() == goal(); }

    // Compact text form, e.g. for saving. Klondike keeps its first format,
    // "1|<stock>|<waste>|<f0>...|<t6>" (the draw count first); other
    // variants start with a letter, "F|", "S2|" and so on. Each card is
    // rank+suit ("Ts", "Qh"), upper-case suit when face up, then ".n" for
    // the second (or later) copy of a card.
    std::string serialize() const;
    static std::optional<Game> parse(std::string_view text);

    bool operator==(const Game&) const = default;

private:
    Pile& mutablePile(PileRef ref);
    bool validRef(PileRef ref) const;
    bool fitsOn(const Card& card, PileRef to) const;
    bool runFrom(const Pile& pile, int start) const;
    int maxRun(bool toEmpty) const;
    void settle(MoveResult& result);
    bool isProductive(const Move& move) const;
    // Pyramid: the card on the waste or an uncovered place, if any.
    std::optional<Card> pyramidCard(PileRef ref) const;

    Variant m_variant = Variant::Klondike;
    int m_option = 1;
    int m_recycles = 0;
    Pile m_stock;
    Pile m_waste;
    std::vector<Pile> m_foundations;
    std::vector<Pile> m_tableau;
    std::vector<Pile> m_cells;
};

// The game names used for saving and in the app, e.g. "freecell".
std::string_view variantKey(Variant variant);
std::optional<Variant> variantFromKey(std::string_view key);

} // namespace solitaire
