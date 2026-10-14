#include "MahjongGame.h"

#include <mahjong/Solver.h>
#include <mahjong/Tiles.h>

#include <calm/Rng.h>

#include <QRandomGenerator>
#include <QRegularExpression>

#include <algorithm>

using mahjong::Board;

namespace {

QByteArray encodeKinds(const Board& board)
{
    QByteArray bytes;
    bytes.reserve(board.size());
    for (int kind : board.kinds())
        bytes.append(char(kind));
    return bytes;
}

QByteArray encodePresence(const Board& board)
{
    const auto& presence = board.presence();
    return QByteArray(presence.data(), qsizetype(presence.size()));
}

bool decodeInto(Board& board, const QByteArray& kinds, const QByteArray& presence)
{
    if (kinds.size() != board.size() || presence.size() != board.size())
        return false;
    std::vector<int> k;
    k.reserve(std::size_t(kinds.size()));
    for (char c : kinds) {
        if (c < 0 || c >= mahjong::KindCount)
            return false;
        k.push_back(c);
    }
    std::vector<char> p(presence.begin(), presence.end());
    for (char& c : p)
        c = c ? 1 : 0;
    board.setKinds(std::move(k));
    board.setPresence(std::move(p));
    return true;
}

} // namespace

MahjongGame::MahjongGame(QObject* parent)
    : GameSession(parent)
{
    setLayout(mahjong::layouts().front());
}

QVariantList MahjongGame::layouts()
{
    QVariantList list;
    for (const mahjong::Layout& layout : mahjong::layouts()) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), QString::fromStdString(layout.name));
        entry.insert(QStringLiteral("difficulty"), layout.difficulty);
        entry.insert(QStringLiteral("tiles"), int(layout.places.size()));
        entry.insert(QStringLiteral("layers"), layout.layers);
        list.append(entry);
    }
    return list;
}

void MahjongGame::setLayout(const mahjong::Layout& layout)
{
    m_layout = &layout;
    m_board = Board(layout);
    m_plan.clear();
    m_width = layout.width;
    m_height = layout.height;
    m_layers = layout.layers;
    emit layoutChanged();
}

QVariantList MahjongGame::slotList() const
{
    QVariantList list;
    list.reserve(m_board.size());
    for (const mahjong::Slot& slot : m_board.places())
        list.append(QVariant(QVariantList { slot.x, slot.y, slot.z }));
    return list;
}

QVariantList MahjongGame::kinds() const
{
    QVariantList list;
    list.reserve(m_board.size());
    for (int kind : m_board.kinds())
        list.append(kind);
    return list;
}

QVariantList MahjongGame::present() const
{
    QVariantList list;
    list.reserve(m_board.size());
    for (char p : m_board.presence())
        list.append(p != 0);
    return list;
}

QVariantList MahjongGame::freeList() const
{
    QVariantList list;
    list.reserve(m_board.size());
    for (int i = 0; i < m_board.size(); ++i)
        list.append(m_board.isFree(i));
    return list;
}

QString MahjongGame::dealCode() const
{
    if (!m_layout || !m_seedKnown)
        return {};
    return QString::fromStdString(m_layout->name).toLower() + QLatin1Char('-') + QString::number(m_seed, 36);
}

QString MahjongGame::shareUrl() const
{
    const QString code = dealCode();
    return code.isEmpty() ? QString() : ShareUrlBase + code;
}

bool MahjongGame::openShared(const QString& link)
{
    // Accept a whole link (any host, with or without /s/) or a bare code.
    static const QRegularExpression pattern(QStringLiteral("(?:^|/)([A-Za-z]+)-([0-9a-zA-Z]{1,13})/?$"));
    const QRegularExpressionMatch match = pattern.match(link.trimmed());
    if (!match.hasMatch())
        return false;
    const mahjong::Layout* layout = mahjong::findLayout(match.captured(1).toStdString());
    bool ok = false;
    const quint64 seed = match.captured(2).toLower().toULongLong(&ok, 36);
    if (!layout || !ok)
        return false;
    m_pendingSeed = seed;
    m_pendingSeedSet = true;
    startNewGame(QString::fromStdString(layout->name));
    return active() && m_seed == seed;
}

QString MahjongGame::kindName(int kind)
{
    if (kind < 0 || kind >= mahjong::KindCount)
        return {};
    return QString::fromStdString(mahjong::kindName(kind));
}

void MahjongGame::setSelected(int tile)
{
    if (m_selected == tile)
        return;
    m_selected = tile;
    emit selectionChanged();
}

void MahjongGame::clearHint()
{
    if (m_hint.isEmpty())
        return;
    m_hint.clear();
    emit hintChanged();
}

void MahjongGame::tap(int tile)
{
    if (!active() || complete() || loading() || tile < 0 || tile >= m_board.size() || !m_board.present(tile))
        return;
    if (!m_board.isFree(tile)) {
        emitFeedback(QStringLiteral("error"));
        emit blockedTapped(tile);
        return;
    }
    if (m_selected == tile) {
        setSelected(-1);
        emitFeedback(QStringLiteral("erase"));
        return;
    }
    if (m_selected < 0 || !m_board.canMatch(m_selected, tile)) {
        setSelected(tile);
        emitFeedback(QStringLiteral("tap"));
        return;
    }

    const int first = m_selected;
    beginMove();
    m_board.removePair(first, tile);
    setSelected(-1);
    clearHint();
    followPlan(first, tile);
    emit matched(first, tile);
    if (m_board.cleared()) {
        endMove();
        finishGame();
        return;
    }
    // Each pair chimes a note higher, climbing through the scale and round again.
    const int taken = (m_board.size() - m_board.remaining()) / 2;
    emitFeedback(QStringLiteral("complete"), (taken - 1) % 8);
    endMove();
}

void MahjongGame::followPlan(int a, int b)
{
    if (m_plan.empty())
        return;
    // The player took a pair the plan also takes: drop it from the plan.
    // Otherwise try swapping partners between the plan's two pairs for these
    // tiles (all four match), then check the plan still clears the table.
    auto it = std::find_if(m_plan.begin(), m_plan.end(), [a, b](const auto& p) {
        return (p.first == a && p.second == b) || (p.first == b && p.second == a);
    });
    if (it != m_plan.end()) {
        m_plan.erase(it);
    } else {
        auto partnerOf = [this](int tile) -> std::pair<std::size_t, int> {
            for (std::size_t i = 0; i < m_plan.size(); ++i) {
                if (m_plan[i].first == tile)
                    return { i, m_plan[i].second };
                if (m_plan[i].second == tile)
                    return { i, m_plan[i].first };
            }
            return { m_plan.size(), -1 };
        };
        const auto [ia, pa] = partnerOf(a);
        const auto [ib, pb] = partnerOf(b);
        if (pa < 0 || pb < 0 || !mahjong::matches(m_board.kind(pa), m_board.kind(pb))) {
            m_plan.clear();
            return;
        }
        m_plan[std::max(ia, ib)] = { pa, pb };
        m_plan.erase(m_plan.begin() + std::ptrdiff_t(std::min(ia, ib)));
    }
    if (!mahjong::planWorks(m_board, m_plan))
        m_plan.clear();
}

bool MahjongGame::hint()
{
    if (!active() || complete() || loading())
        return false;
    if (m_plan.empty() || !mahjong::planWorks(m_board, m_plan)) {
        const mahjong::Solution solution = mahjong::solve(m_board, 60000);
        m_plan = solution.outcome == mahjong::Solution::Solved ? solution.moves : mahjong::Plan {};
    }
    std::pair<int, int> pair { -1, -1 };
    if (!m_plan.empty()) {
        pair = m_plan.front();
    } else {
        // No known way to win from here: any pair is still worth showing.
        const auto pairs = m_board.availablePairs();
        if (!pairs.empty())
            pair = pairs.front();
    }
    if (pair.first < 0) {
        emitFeedback(QStringLiteral("error"));
        return false;
    }
    noteHintUsed();
    setSelected(-1);
    m_hint = { pair.first, pair.second };
    emit hintChanged();
    emitFeedback(QStringLiteral("soft"));
    return true;
}

bool MahjongGame::shuffle()
{
    if (!active() || complete() || loading())
        return false;
    Board next = m_board;
    calm::Rng rng(QRandomGenerator::global()->generate64());
    mahjong::Plan plan;
    if (!mahjong::reshuffle(next, rng, &plan)) {
        emitFeedback(QStringLiteral("error"));
        return false;
    }
    beginMove();
    noteHintUsed();
    m_board = next;
    m_plan = plan;
    setSelected(-1);
    clearHint();
    emitFeedback(QStringLiteral("soft"));
    endMove();
    return true;
}

void MahjongGame::createGame(const QString& level)
{
    const mahjong::Layout* layout = mahjong::findLayout(level.toStdString());
    if (!layout)
        layout = &mahjong::layouts().front();
    setLayout(*layout);
    m_seed = m_pendingSeedSet ? m_pendingSeed : QRandomGenerator::global()->generate64();
    m_pendingSeedSet = false;
    m_seedKnown = true;
    if (!mahjong::deal(m_board, m_seed, &m_plan)) {
        cancelNewGame();
        return;
    }
    setSelected(-1);
    clearHint();
    beginGame();
}

QVariant MahjongGame::snapshot() const
{
    QVariantMap snap;
    snap.insert(QStringLiteral("kinds"), encodeKinds(m_board));
    snap.insert(QStringLiteral("present"), encodePresence(m_board));
    return snap;
}

void MahjongGame::applySnapshot(const QVariant& snapshot)
{
    const QVariantMap snap = snapshot.toMap();
    decodeInto(m_board, snap.value(QStringLiteral("kinds")).toByteArray(), snap.value(QStringLiteral("present")).toByteArray());
    m_plan.clear();
    setSelected(-1);
    clearHint();
}

QVariantMap MahjongGame::saveState() const
{
    QVariantMap state = snapshot().toMap();
    state.insert(QStringLiteral("layout"), m_layout ? QString::fromStdString(m_layout->name) : QString());
    state.insert(QStringLiteral("seed"), QString::number(m_seed));
    return state;
}

bool MahjongGame::restoreState(const QVariantMap& state)
{
    const mahjong::Layout* layout = mahjong::findLayout(state.value(QStringLiteral("layout")).toString().toStdString());
    if (!layout)
        return false;
    Board board(*layout);
    if (!decodeInto(board, state.value(QStringLiteral("kinds")).toByteArray(), state.value(QStringLiteral("present")).toByteArray()))
        return false;
    setLayout(*layout);
    m_board = board;
    m_seed = state.value(QStringLiteral("seed")).toString().toULongLong(&m_seedKnown);
    setSelected(-1);
    clearHint();
    return true;
}

void MahjongGame::setTileTheme(const QString& theme)
{
    if (tileTheme() == theme && m_settings.contains(QStringLiteral("mahjong/tileTheme")))
        return;
    m_settings.setValue(QStringLiteral("mahjong/tileTheme"), theme);
    emit optionsChanged();
}

bool MahjongGame::option(const QString& key, bool fallback) const
{
    return m_settings.value(QStringLiteral("mahjong/") + key, fallback).toBool();
}

void MahjongGame::setOption(const QString& key, bool on)
{
    if (option(key, !on) == on && m_settings.contains(QStringLiteral("mahjong/") + key))
        return;
    m_settings.setValue(QStringLiteral("mahjong/") + key, on);
    emit optionsChanged();
}
