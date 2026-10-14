#include <mahjong/Solver.h>
#include <mahjong/Tiles.h>

#include <calm/Rng.h>

#include <algorithm>
#include <map>
#include <string>
#include <unordered_set>

namespace mahjong {

namespace {

struct Search {
    const Board& board;
    calm::Rng& rng;
    int budget;
    int visited = 0;
    bool exhausted = false;
    std::unordered_set<std::string> deadEnds;
    std::vector<std::pair<int, int>> moves;

    // A move that can't spoil the game, if there is one.
    bool safeMove(const std::vector<char>& presence, std::pair<int, int>& move) const
    {
        std::map<int, std::vector<int>> present;
        std::map<int, std::vector<int>> free;
        for (int i = 0; i < board.size(); ++i) {
            if (!presence[std::size_t(i)])
                continue;
            const int group = matchGroup(board.kind(i));
            present[group].push_back(i);
            if (board.isFree(i, presence))
                free[group].push_back(i);
        }
        for (const auto& [group, tiles] : free) {
            const std::size_t onTable = present[group].size();
            if (tiles.size() >= 2 && (onTable == 2 || tiles.size() == onTable)) {
                move = { tiles[0], tiles[1] };
                return true;
            }
        }
        return false;
    }

    bool run(std::vector<char>& presence, int left)
    {
        if (left == 0)
            return true;
        if (++visited > budget) {
            exhausted = true;
            return false;
        }
        std::pair<int, int> forced;
        if (safeMove(presence, forced)) {
            presence[std::size_t(forced.first)] = presence[std::size_t(forced.second)] = 0;
            moves.push_back(forced);
            if (run(presence, left - 2))
                return true;
            moves.pop_back();
            presence[std::size_t(forced.first)] = presence[std::size_t(forced.second)] = 1;
            return false;
        }

        const std::string key(presence.begin(), presence.end());
        if (deadEnds.count(key))
            return false;

        // Try the highest pairs first (they uncover the most), in a random
        // order among equals so each restart explores differently.
        auto pairs = board.availablePairs(presence);
        rng.shuffle(pairs.begin(), pairs.end());
        std::stable_sort(pairs.begin(), pairs.end(), [this](const auto& a, const auto& b) {
            return board.slot(a.first).z + board.slot(a.second).z > board.slot(b.first).z + board.slot(b.second).z;
        });
        for (const auto& pair : pairs) {
            presence[std::size_t(pair.first)] = presence[std::size_t(pair.second)] = 0;
            moves.push_back(pair);
            if (run(presence, left - 2))
                return true;
            moves.pop_back();
            presence[std::size_t(pair.first)] = presence[std::size_t(pair.second)] = 1;
            if (exhausted)
                return false;
        }
        deadEnds.insert(key);
        return false;
    }
};

} // namespace

Solution solve(const Board& board, int budget)
{
    // Searches get unlucky: a few early choices can lead into a huge dead
    // subtree. Restarting with a fresh random order (keeping the dead ends
    // already proven) finds real deals' solutions far more reliably.
    calm::Rng rng(0x5EEDull + std::uint64_t(board.remaining()));
    std::unordered_set<std::string> deadEnds;
    int spent = 0;
    for (int restart = 0; spent < budget; ++restart) {
        const int slice = std::min(budget - spent, 2000 << std::min(restart / 4, 6));
        Search search { board, rng, slice, 0, false, std::move(deadEnds), {} };
        std::vector<char> presence = board.presence();
        const bool solved = search.run(presence, board.remaining());
        spent += search.visited;
        if (solved)
            return { Solution::Solved, std::move(search.moves) };
        if (!search.exhausted)
            return { Solution::Unsolvable, {} };
        deadEnds = std::move(search.deadEnds);
    }
    return { Solution::Unknown, {} };
}

} // namespace mahjong
