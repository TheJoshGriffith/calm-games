#pragma once

#include <string>
#include <utility>
#include <vector>

namespace mahjong {

// The 42 tile kinds of a 144-tile set, as small ints:
//   0-8   Dots 1-9          (four of each)
//   9-17  Bamboo 1-9        (four of each)
//   18-26 Characters 1-9    (four of each)
//   27-30 Winds: east, south, west, north (four of each)
//   31-33 Dragons: red, green, white      (four of each)
//   34-37 Flowers 1-4       (one of each; any flower matches any flower)
//   38-41 Seasons 1-4       (one of each; any season matches any season)
enum class Suit { Dots, Bamboo, Characters, Winds, Dragons, Flowers, Seasons };

constexpr int KindCount = 42;
constexpr int FullSetSize = 144;

Suit suitOf(int kind);
// 1-based position within the suit (1-9 for numbered suits, 1-4 for winds,
// flowers and seasons, 1-3 for dragons).
int rankOf(int kind);
// Tiles match when their groups are equal: each kind is its own group,
// except that all flowers share one group and all seasons another.
int matchGroup(int kind);
bool matches(int a, int b);
// A short English name, e.g. "3 of Bamboo", "East wind", "Red dragon".
std::string kindName(int kind);

// The 72 matching pairs of a full set: two pairs of each ordinary kind, two
// flower pairs and two season pairs.
std::vector<std::pair<int, int>> fullSetPairs();

} // namespace mahjong
