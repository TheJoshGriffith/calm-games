#include <mahjong/Tiles.h>

namespace mahjong {

Suit suitOf(int kind)
{
    if (kind < 9)
        return Suit::Dots;
    if (kind < 18)
        return Suit::Bamboo;
    if (kind < 27)
        return Suit::Characters;
    if (kind < 31)
        return Suit::Winds;
    if (kind < 34)
        return Suit::Dragons;
    if (kind < 38)
        return Suit::Flowers;
    return Suit::Seasons;
}

int rankOf(int kind)
{
    if (kind < 27)
        return kind % 9 + 1;
    if (kind < 31)
        return kind - 27 + 1;
    if (kind < 34)
        return kind - 31 + 1;
    if (kind < 38)
        return kind - 34 + 1;
    return kind - 38 + 1;
}

int matchGroup(int kind)
{
    if (kind >= 38)
        return 38;
    if (kind >= 34)
        return 34;
    return kind;
}

bool matches(int a, int b)
{
    return a >= 0 && b >= 0 && matchGroup(a) == matchGroup(b);
}

std::string kindName(int kind)
{
    static const char* const winds[] = { "East", "South", "West", "North" };
    static const char* const dragons[] = { "Red", "Green", "White" };
    static const char* const flowers[] = { "Plum", "Orchid", "Chrysanthemum", "Bamboo" };
    static const char* const seasons[] = { "Spring", "Summer", "Autumn", "Winter" };
    const int rank = rankOf(kind);
    switch (suitOf(kind)) {
    case Suit::Dots:
        return std::to_string(rank) + " of Dots";
    case Suit::Bamboo:
        return std::to_string(rank) + " of Bamboo";
    case Suit::Characters:
        return std::to_string(rank) + " of Characters";
    case Suit::Winds:
        return std::string(winds[rank - 1]) + " wind";
    case Suit::Dragons:
        return std::string(dragons[rank - 1]) + " dragon";
    case Suit::Flowers:
        return std::string(flowers[rank - 1]) + " flower";
    case Suit::Seasons:
        return std::string(seasons[rank - 1]);
    }
    return {};
}

std::vector<std::pair<int, int>> fullSetPairs()
{
    std::vector<std::pair<int, int>> pairs;
    pairs.reserve(FullSetSize / 2);
    for (int kind = 0; kind < 34; ++kind) {
        pairs.emplace_back(kind, kind);
        pairs.emplace_back(kind, kind);
    }
    pairs.emplace_back(34, 35);
    pairs.emplace_back(36, 37);
    pairs.emplace_back(38, 39);
    pairs.emplace_back(40, 41);
    return pairs;
}

} // namespace mahjong
