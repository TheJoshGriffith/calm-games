#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace mahjong {

// Where a tile sits, in half-tile units: a tile covers x..x+2 and y..y+2 on
// layer z, so tiles can sit half a tile across or down from each other.
struct Slot {
    int x = 0;
    int y = 0;
    int z = 0;
};

// A named arrangement of tile slots (a "pattern" to the player).
struct Layout {
    std::string name;       // also the level id, e.g. "Turtle"
    int difficulty = 1;     // 1 (gentle) to 4 (hardest)
    std::vector<Slot> places; // ordered by layer, then row, then column
    int width = 0;  // half-tile units across the whole layout
    int height = 0;
    int layers = 0;
};

// Builds a layout from one picture per layer. Each character is one tile
// cell: '#' places a tile there, '>' half a tile to the right, 'v' half a
// tile down, '+' both; anything else is empty.
Layout parseLayout(std::string name, int difficulty, const std::vector<std::vector<std::string>>& layers);

// Every built-in layout, gentlest first.
const std::vector<Layout>& layouts();
// The layout with this name (case-insensitive), or nullptr.
const Layout* findLayout(std::string_view name);

} // namespace mahjong
