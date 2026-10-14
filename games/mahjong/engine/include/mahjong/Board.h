#pragma once

#include <mahjong/Layout.h>

#include <utility>
#include <vector>

namespace mahjong {

// The tiles of one game on a layout: which kind sits in each slot and which
// are still on the table, plus the rules for which tiles are free.
//
// A tile is free when no tile lies on top of it (on any higher layer,
// overlapping it) and it can slide out to the left or to the right (no tile
// directly beside it on the same layer, on at least one side).
class Board {
public:
    Board() = default;
    explicit Board(const Layout& layout);

    int size() const { return int(m_slots.size()); }
    const Slot& slot(int i) const { return m_slots[std::size_t(i)]; }
    const std::vector<Slot>& places() const { return m_slots; }

    // The tile face in each slot, or -1 before dealing.
    int kind(int i) const { return m_kinds[std::size_t(i)]; }
    const std::vector<int>& kinds() const { return m_kinds; }
    void setKinds(std::vector<int> kinds);

    bool present(int i) const { return m_present[std::size_t(i)] != 0; }
    const std::vector<char>& presence() const { return m_present; }
    void setPresence(std::vector<char> presence);

    bool isFree(int i) const { return isFree(i, m_present); }
    // The same rule for any set of tiles on the table (for deals and solving).
    bool isFree(int i, const std::vector<char>& presence) const;

    // Both tiles on the table, free, different and matching.
    bool canMatch(int a, int b) const;
    // Takes a matching pair off the table; false (and no change) if not allowed.
    bool removePair(int a, int b);

    int remaining() const;
    bool cleared() const { return remaining() == 0; }
    // Every pair the player could take right now.
    std::vector<std::pair<int, int>> availablePairs() const;
    std::vector<std::pair<int, int>> availablePairs(const std::vector<char>& presence) const;

private:
    std::vector<Slot> m_slots;
    std::vector<int> m_kinds;
    std::vector<char> m_present;
    // Per slot: tiles that cover it, and tiles directly left and right of it.
    std::vector<std::vector<int>> m_above;
    std::vector<std::vector<int>> m_left;
    std::vector<std::vector<int>> m_right;
};

} // namespace mahjong
