#include <mahjong/Board.h>
#include <mahjong/Tiles.h>

#include <cstdlib>

namespace mahjong {

Board::Board(const Layout& layout)
    : m_slots(layout.places)
    , m_kinds(layout.places.size(), -1)
    , m_present(layout.places.size(), 1)
    , m_above(layout.places.size())
    , m_left(layout.places.size())
    , m_right(layout.places.size())
{
    for (std::size_t i = 0; i < m_slots.size(); ++i) {
        const Slot& a = m_slots[i];
        for (std::size_t j = 0; j < m_slots.size(); ++j) {
            if (i == j)
                continue;
            const Slot& b = m_slots[j];
            const bool overlapY = std::abs(a.y - b.y) < 2;
            if (b.z > a.z && std::abs(a.x - b.x) < 2 && overlapY)
                m_above[i].push_back(int(j));
            else if (b.z == a.z && overlapY && b.x == a.x - 2)
                m_left[i].push_back(int(j));
            else if (b.z == a.z && overlapY && b.x == a.x + 2)
                m_right[i].push_back(int(j));
        }
    }
}

void Board::setKinds(std::vector<int> kinds)
{
    if (kinds.size() == m_slots.size())
        m_kinds = std::move(kinds);
}

void Board::setPresence(std::vector<char> presence)
{
    if (presence.size() == m_slots.size())
        m_present = std::move(presence);
}

bool Board::isFree(int i, const std::vector<char>& presence) const
{
    const auto index = std::size_t(i);
    if (!presence[index])
        return false;
    for (int j : m_above[index])
        if (presence[std::size_t(j)])
            return false;
    auto blocked = [&presence](const std::vector<int>& side) {
        for (int j : side)
            if (presence[std::size_t(j)])
                return true;
        return false;
    };
    return !blocked(m_left[index]) || !blocked(m_right[index]);
}

bool Board::canMatch(int a, int b) const
{
    return a != b && a >= 0 && b >= 0 && a < size() && b < size() && isFree(a) && isFree(b)
        && matches(kind(a), kind(b));
}

bool Board::removePair(int a, int b)
{
    if (!canMatch(a, b))
        return false;
    m_present[std::size_t(a)] = 0;
    m_present[std::size_t(b)] = 0;
    return true;
}

int Board::remaining() const
{
    int n = 0;
    for (char p : m_present)
        n += p != 0;
    return n;
}

std::vector<std::pair<int, int>> Board::availablePairs() const
{
    return availablePairs(m_present);
}

std::vector<std::pair<int, int>> Board::availablePairs(const std::vector<char>& presence) const
{
    std::vector<int> free;
    for (int i = 0; i < size(); ++i)
        if (isFree(i, presence))
            free.push_back(i);
    std::vector<std::pair<int, int>> pairs;
    for (std::size_t a = 0; a < free.size(); ++a)
        for (std::size_t b = a + 1; b < free.size(); ++b)
            if (matches(m_kinds[std::size_t(free[a])], m_kinds[std::size_t(free[b])]))
                pairs.emplace_back(free[a], free[b]);
    return pairs;
}

} // namespace mahjong
