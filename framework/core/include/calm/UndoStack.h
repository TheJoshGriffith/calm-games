#pragma once

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace calm {

// Snapshot-based undo/redo. Games push their state before each move; game
// states in Calm games are small (a Sudoku board is 81 bytes plus notes), so
// snapshots keep undo trivially correct.
template <typename State>
class UndoStack {
public:
    explicit UndoStack(std::size_t limit = 500)
        : m_limit(limit)
    {
    }

    void push(State state)
    {
        m_undo.push_back(std::move(state));
        if (m_undo.size() > m_limit)
            m_undo.erase(m_undo.begin());
        m_redo.clear();
    }

    // Returns the state to restore, given the current one (kept for redo).
    std::optional<State> undo(State current)
    {
        if (m_undo.empty())
            return std::nullopt;
        State previous = std::move(m_undo.back());
        m_undo.pop_back();
        m_redo.push_back(std::move(current));
        return previous;
    }

    std::optional<State> redo(State current)
    {
        if (m_redo.empty())
            return std::nullopt;
        State next = std::move(m_redo.back());
        m_redo.pop_back();
        m_undo.push_back(std::move(current));
        return next;
    }

    bool canUndo() const noexcept { return !m_undo.empty(); }
    bool canRedo() const noexcept { return !m_redo.empty(); }

    void clear()
    {
        m_undo.clear();
        m_redo.clear();
    }

private:
    std::size_t m_limit;
    std::vector<State> m_undo;
    std::vector<State> m_redo;
};

} // namespace calm
