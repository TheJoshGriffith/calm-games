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
constexpr int TableauCount = 7;

// A card is its id (suit * 13 + rank - 1) plus whether it shows its face.
struct Card {
    std::uint8_t id = 0;
    bool faceUp = false;

    constexpr int rank() const noexcept { return id % RankCount + 1; }
    constexpr Suit suit() const noexcept { return static_cast<Suit>(id / RankCount); }
    constexpr bool red() const noexcept { return suit() == Suit::Hearts || suit() == Suit::Diamonds; }
    constexpr bool operator==(const Card&) const = default;
};

constexpr Card makeCard(Suit suit, int rank, bool faceUp = true) noexcept
{
    return { static_cast<std::uint8_t>(int(suit) * RankCount + rank - 1), faceUp };
}

using Pile = std::vector<Card>;

enum class PileKind : std::uint8_t { Stock, Waste, Foundation, Tableau };

// A pile on the table. Foundation `index` is the suit it builds.
struct PileRef {
    PileKind kind = PileKind::Stock;
    int index = 0;
    constexpr bool operator==(const PileRef&) const = default;
};

// Moving the cards of `from` starting at position `start` onto `to`.
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
};

enum class DrawResult { Nothing, Drew, Recycled };

// What a successful move did, so the app can pick the right sound.
struct MoveResult {
    bool ok = false;
    bool flipped = false; // a face-down tableau card was turned over
    bool toFoundation = false;
};

// The state and rules of one game of Klondike: seven tableau columns built
// down in alternating colours, four foundations built up by suit from the
// ace, and a stock dealt one or three cards at a time with unlimited passes.
class Klondike {
public:
    Klondike() = default;

    // Shuffles a deck with `seed` and deals the standard layout.
    static Klondike deal(std::uint64_t seed, int drawCount);

    int drawCount() const noexcept { return m_drawCount; }
    const Pile& pile(PileRef ref) const;
    const Pile& stock() const noexcept { return m_stock; }
    const Pile& waste() const noexcept { return m_waste; }
    const Pile& foundation(int suit) const { return m_foundations.at(suit); }
    const Pile& tableau(int column) const { return m_tableau.at(column); }

    // Deals from the stock to the waste, or turns the waste back over.
    DrawResult draw();

    bool canMove(const Move& move) const;
    MoveResult move(const Move& move);

    // Where a double tap on the card at `start` of `from` should send it:
    // its foundation if possible, otherwise the leftmost column it fits on.
    std::optional<Move> bestMove(PileRef from, int start) const;

    // A helpful next step, or nothing if no move makes progress.
    std::optional<Hint> hint() const;

    // Every tableau card is face up. With draw one the game can then always
    // be finished; with draw three the stock may still need care.
    bool canAutoFinish() const;
    // The next foundation move (or draw) for finishing a draw-one game.
    std::optional<Hint> autoFinishStep() const;

    int foundationCards() const;
    bool won() const { return foundationCards() == DeckSize; }

    // Compact text form, e.g. for saving: "1|<stock>|<waste>|<f0>...|<t6>".
    // Each card is rank+suit ("Ts", "Qh"), upper-case suit when face up.
    std::string serialize() const;
    static std::optional<Klondike> parse(std::string_view text);

    bool operator==(const Klondike&) const = default;

private:
    Pile& mutablePile(PileRef ref);
    bool fitsOn(const Card& card, PileRef to) const;
    // Tableau moves that turn a card over or free a column.
    bool isProductive(const Move& move) const;

    int m_drawCount = 1;
    Pile m_stock;
    Pile m_waste;
    std::array<Pile, SuitCount> m_foundations;
    std::array<Pile, TableauCount> m_tableau;
};

} // namespace solitaire
