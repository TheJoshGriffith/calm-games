#include <mahjong/Generator.h>
#include <mahjong/Tiles.h>

#include <map>

namespace mahjong {

namespace {

// Picks a free tile, favouring tiles stacked high: taking the tops of stacks
// off early keeps tall stacks from being the last tiles left (a dead end).
int pickFree(const Board& board, const std::vector<int>& free, int except, calm::Rng& rng)
{
    std::uint32_t total = 0;
    for (int i : free)
        if (i != except)
            total += std::uint32_t(1 + 3 * board.slot(i).z);
    if (total == 0)
        return -1;
    std::uint32_t roll = rng.below(total);
    for (int i : free) {
        if (i == except)
            continue;
        const auto weight = std::uint32_t(1 + 3 * board.slot(i).z);
        if (roll < weight)
            return i;
        roll -= weight;
    }
    return -1;
}

} // namespace

bool arrange(Board& board, std::vector<std::pair<int, int>> pairs, calm::Rng& rng, Plan* plan, int attempts)
{
    const int onTable = board.remaining();
    if (onTable % 2 != 0 || int(pairs.size()) * 2 != onTable)
        return false;

    std::vector<int> kinds = board.kinds();
    for (int attempt = 0; attempt < attempts; ++attempt) {
        std::vector<char> presence = board.presence();
        Plan order;
        rng.shuffle(pairs.begin(), pairs.end());
        bool stuck = false;
        for (const auto& [first, second] : pairs) {
            std::vector<int> free;
            for (int i = 0; i < board.size(); ++i)
                if (board.isFree(i, presence))
                    free.push_back(i);
            const int a = pickFree(board, free, -1, rng);
            const int b = pickFree(board, free, a, rng);
            if (a < 0 || b < 0) {
                stuck = true;
                break;
            }
            const bool swap = rng.below(2) == 1;
            kinds[std::size_t(a)] = swap ? second : first;
            kinds[std::size_t(b)] = swap ? first : second;
            presence[std::size_t(a)] = 0;
            presence[std::size_t(b)] = 0;
            order.emplace_back(a, b);
        }
        if (!stuck) {
            board.setKinds(kinds);
            if (plan)
                *plan = std::move(order);
            return true;
        }
    }
    return false;
}

bool deal(Board& board, std::uint64_t seed, Plan* plan)
{
    if (board.size() % 2 != 0 || board.size() > FullSetSize)
        return false;
    board.setPresence(std::vector<char>(std::size_t(board.size()), 1));
    calm::Rng rng(seed);
    // A smaller layout uses a random selection of the set's pairs.
    std::vector<std::pair<int, int>> pairs = fullSetPairs();
    rng.shuffle(pairs.begin(), pairs.end());
    pairs.resize(std::size_t(board.size() / 2));
    return arrange(board, pairs, rng, plan);
}

bool reshuffle(Board& board, calm::Rng& rng, Plan* plan)
{
    // Pair up the faces still on the table within their match groups (each
    // group has an even count left, since tiles leave in matching pairs).
    std::map<int, std::vector<int>> groups;
    for (int i = 0; i < board.size(); ++i)
        if (board.present(i))
            groups[matchGroup(board.kind(i))].push_back(board.kind(i));
    std::vector<std::pair<int, int>> pairs;
    for (const auto& [group, kinds] : groups) {
        if (kinds.size() % 2 != 0)
            return false;
        for (std::size_t k = 0; k + 1 < kinds.size(); k += 2)
            pairs.emplace_back(kinds[k], kinds[k + 1]);
    }
    return arrange(board, pairs, rng, plan);
}

bool planWorks(Board board, const Plan& plan)
{
    for (const auto& [a, b] : plan)
        if (!board.removePair(a, b))
            return false;
    return board.cleared();
}

} // namespace mahjong
