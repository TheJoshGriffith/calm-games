#include "SolitaireGame.h"

#include <QRandomGenerator>
#include <QtConcurrent/QtConcurrentRun>

using solitaire::DrawResult;
using solitaire::Klondike;
using solitaire::Move;
using solitaire::PileKind;
using solitaire::PileRef;

namespace {

// Interval between cards when finishing automatically.
constexpr int FinishStepMs = 110;

PileRef pileRef(int pile, int slot)
{
    return { static_cast<PileKind>(std::clamp(pile, 0, 3)), slot };
}

} // namespace

SolitaireGame::SolitaireGame(QObject* parent)
    : GameSession(parent)
{
    m_finishTimer.setInterval(FinishStepMs);
    connect(&m_finishTimer, &QTimer::timeout, this, &SolitaireGame::finishStep);
    // Have a deal of each kind ready so new games start instantly.
    requestDeal(1, winnableDeals());
    requestDeal(3, winnableDeals());
}

SolitaireGame::~SolitaireGame()
{
    for (auto* watcher : std::as_const(m_watchers)) {
        watcher->disconnect(this);
        watcher->waitForFinished();
    }
}

int SolitaireGame::drawCountFor(const QString& level)
{
    return level.contains(QLatin1String("three"), Qt::CaseInsensitive) || level.contains(QLatin1Char('3')) ? 3 : 1;
}

QVariantList SolitaireGame::cards() const
{
    QVariantList out;
    for (int i = 0; i < solitaire::DeckSize; ++i)
        out.append(QVariantMap());

    auto add = [&](const solitaire::Pile& pile, int kind, int slot) {
        const int count = int(pile.size());
        for (int pos = 0; pos < count; ++pos) {
            const solitaire::Card& card = pile[pos];
            bool movable = card.faceUp;
            if (kind == Waste || kind == Foundation)
                movable = pos == count - 1;
            QVariantMap map;
            map.insert(QStringLiteral("rank"), card.rank());
            map.insert(QStringLiteral("suit"), int(card.suit()));
            map.insert(QStringLiteral("red"), card.red());
            map.insert(QStringLiteral("faceUp"), card.faceUp);
            map.insert(QStringLiteral("pile"), kind);
            map.insert(QStringLiteral("slot"), slot);
            map.insert(QStringLiteral("pos"), pos);
            map.insert(QStringLiteral("count"), count);
            map.insert(QStringLiteral("movable"), movable && kind != Stock);
            out[card.id] = map;
        }
    };
    add(m_table.stock(), Stock, 0);
    add(m_table.waste(), Waste, 0);
    for (int s = 0; s < solitaire::SuitCount; ++s)
        add(m_table.foundation(s), Foundation, s);
    for (int c = 0; c < solitaire::TableauCount; ++c)
        add(m_table.tableau(c), Tableau, c);
    return out;
}

std::optional<SolitaireGame::Location> SolitaireGame::locate(int cardId) const
{
    if (cardId < 0 || cardId >= solitaire::DeckSize)
        return std::nullopt;
    auto find = [&](PileRef ref) -> std::optional<Location> {
        const solitaire::Pile& pile = m_table.pile(ref);
        for (int pos = 0; pos < int(pile.size()); ++pos)
            if (pile[pos].id == cardId)
                return Location { ref, pos };
        return std::nullopt;
    };
    if (auto l = find({ PileKind::Waste, 0 }))
        return l;
    for (int s = 0; s < solitaire::SuitCount; ++s)
        if (auto l = find({ PileKind::Foundation, s }))
            return l;
    for (int c = 0; c < solitaire::TableauCount; ++c)
        if (auto l = find({ PileKind::Tableau, c }))
            return l;
    return find({ PileKind::Stock, 0 });
}

void SolitaireGame::draw()
{
    if (!active() || complete() || autoFinishing())
        return;
    if (m_table.stock().empty() && m_table.waste().empty())
        return;
    beginMove();
    clearHint();
    const DrawResult result = m_table.draw();
    ++m_moves;
    emitFeedback(result == DrawResult::Recycled ? QStringLiteral("erase") : QStringLiteral("soft"));
    endMove();
}

bool SolitaireGame::perform(const Move& move)
{
    if (!m_table.canMove(move))
        return false;
    beginMove();
    clearHint();
    const solitaire::MoveResult result = m_table.move(move);
    ++m_moves;
    if (m_table.won()) {
        endMove();
        stopAutoFinish();
        finishGame();
        return true;
    }
    if (result.toFoundation)
        emitFeedback(QStringLiteral("complete"), m_table.foundationCards() * 8 / (solitaire::DeckSize + 1));
    else
        emitFeedback(QStringLiteral("tap"));
    endMove();
    return true;
}

bool SolitaireGame::tapCard(int cardId)
{
    if (!active() || complete() || autoFinishing())
        return false;
    const auto where = locate(cardId);
    if (!where)
        return false;
    if (where->pile.kind == PileKind::Stock) {
        draw();
        return true;
    }
    if (const auto best = m_table.bestMove(where->pile, where->pos))
        return perform(*best);
    emitFeedback(QStringLiteral("error"));
    return false;
}

bool SolitaireGame::canDrop(int cardId, int pile, int slot) const
{
    const auto where = locate(cardId);
    return where && m_table.canMove({ where->pile, where->pos, pileRef(pile, slot) });
}

bool SolitaireGame::dropCard(int cardId, int pile, int slot)
{
    if (!active() || complete() || autoFinishing())
        return false;
    const auto where = locate(cardId);
    if (!where)
        return false;
    PileRef to = pileRef(pile, slot);
    // Any foundation will do: cards go to their own suit's.
    if (to.kind == PileKind::Foundation) {
        const solitaire::Pile& source = m_table.pile(where->pile);
        if (where->pos < int(source.size()))
            to.index = int(source[where->pos].suit());
    }
    if (to == where->pile)
        return false;
    if (perform({ where->pile, where->pos, to }))
        return true;
    emitFeedback(QStringLiteral("error"));
    return false;
}

bool SolitaireGame::showHint()
{
    if (!active() || complete() || autoFinishing())
        return false;
    // A step on a winning line if the solver finds one quickly, otherwise
    // the best-looking move.
    auto hint = plannedStep();
    if (!hint)
        hint = m_table.hint();
    QVariantMap map;
    if (hint && hint->draw) {
        map.insert(QStringLiteral("stock"), true);
        map.insert(QStringLiteral("cards"), QVariantList());
    } else if (hint) {
        QVariantList ids;
        const solitaire::Pile& source = m_table.pile(hint->move.from);
        for (int pos = hint->move.start; pos < int(source.size()); ++pos)
            ids.append(int(source[pos].id));
        map.insert(QStringLiteral("stock"), false);
        map.insert(QStringLiteral("cards"), ids);
        map.insert(QStringLiteral("pile"), int(hint->move.to.kind));
        map.insert(QStringLiteral("slot"), hint->move.to.index);
    }
    m_hint = map;
    emit hintChanged();
    emitFeedback(hint ? QStringLiteral("soft") : QStringLiteral("error"));
    return hint.has_value();
}

std::optional<solitaire::Hint> SolitaireGame::plannedStep()
{
    // Following the plan step by step never revisits a position, so hints
    // can't lead the player round in circles.
    const std::string now = m_table.serialize();
    for (const auto& [state, step] : m_plan)
        if (state == now)
            return step;
    m_plan.clear();
    const auto path = solitaire::solvePath(m_table);
    if (!path || path->empty())
        return std::nullopt;
    Klondike replay = m_table;
    for (const solitaire::Hint& step : *path) {
        m_plan.emplace_back(replay.serialize(), step);
        if (step.draw)
            replay.draw();
        else
            replay.move(step.move);
    }
    return m_plan.front().second;
}

void SolitaireGame::clearHint()
{
    if (m_hint.isEmpty())
        return;
    m_hint.clear();
    emit hintChanged();
}

void SolitaireGame::autoFinish()
{
    if (!canAutoFinish() || autoFinishing())
        return;
    clearHint();
    m_finishTimer.start();
    emit autoFinishingChanged();
    finishStep();
}

void SolitaireGame::finishStep()
{
    const auto step = m_table.autoFinishStep();
    if (!step || paused()) {
        if (!step)
            stopAutoFinish();
        return;
    }
    if (step->draw) {
        beginMove();
        m_table.draw();
        ++m_moves;
        endMove();
        return;
    }
    perform(step->move);
}

void SolitaireGame::stopAutoFinish()
{
    if (!m_finishTimer.isActive())
        return;
    m_finishTimer.stop();
    emit autoFinishingChanged();
}

void SolitaireGame::createGame(const QString& level)
{
    stopAutoFinish();
    const int draws = drawCountFor(level);
    const int key = bufferKey(draws, winnableDeals());
    auto& queue = m_buffer[key];
    if (!queue.isEmpty()) {
        startWith(queue.takeFirst());
        requestDeal(draws, winnableDeals());
        return;
    }
    m_waitingFor = key;
    if (m_inFlight.value(key) == 0)
        requestDeal(draws, winnableDeals());
}

void SolitaireGame::requestDeal(int drawCount, bool winnable)
{
    const int key = bufferKey(drawCount, winnable);
    if (m_buffer.value(key).size() + m_inFlight.value(key) >= 1)
        return;
    ++m_inFlight[key];

    const quint64 seed = QRandomGenerator::global()->generate64();
    auto* watcher = new QFutureWatcher<solitaire::Deal>(this);
    m_watchers.append(watcher);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, key] {
        m_watchers.removeOne(watcher);
        const solitaire::Deal deal = watcher->result();
        watcher->deleteLater();
        onDealReady(key, deal);
    });
    watcher->setFuture(QtConcurrent::run([seed, drawCount, winnable] {
        if (winnable)
            return solitaire::dealWinnable(seed, drawCount);
        return solitaire::Deal { Klondike::deal(seed, drawCount), seed, false };
    }));
}

void SolitaireGame::onDealReady(int key, const solitaire::Deal& deal)
{
    --m_inFlight[key];
    if (m_waitingFor == key) {
        m_waitingFor.reset();
        startWith(deal);
        requestDeal(key / 2, key % 2 == 1);
    } else {
        m_buffer[key].append(deal);
    }
}

void SolitaireGame::startWith(const solitaire::Deal& deal)
{
    m_table = deal.game;
    m_seed = deal.seed;
    m_moves = 0;
    clearHint();
    beginGame();
}

QVariantMap SolitaireGame::saveState() const
{
    QVariantMap state;
    state.insert(QStringLiteral("table"), QString::fromStdString(m_table.serialize()));
    state.insert(QStringLiteral("seed"), QString::number(m_seed));
    state.insert(QStringLiteral("moves"), m_moves);
    return state;
}

bool SolitaireGame::restoreState(const QVariantMap& state)
{
    const auto table = Klondike::parse(state.value(QStringLiteral("table")).toString().toStdString());
    if (!table || table->won())
        return false;
    stopAutoFinish();
    m_table = *table;
    m_seed = state.value(QStringLiteral("seed")).toString().toULongLong();
    m_moves = std::max(0, state.value(QStringLiteral("moves")).toInt());
    clearHint();
    return true;
}

QVariant SolitaireGame::snapshot() const
{
    return QVariantList { QString::fromStdString(m_table.serialize()), m_moves };
}

void SolitaireGame::applySnapshot(const QVariant& snapshot)
{
    const QVariantList parts = snapshot.toList();
    if (parts.size() != 2)
        return;
    const auto table = Klondike::parse(parts.at(0).toString().toStdString());
    if (!table)
        return;
    stopAutoFinish();
    clearHint();
    m_table = *table;
    m_moves = parts.at(1).toInt();
}

bool SolitaireGame::option(const QString& key, bool fallback) const
{
    return m_settings.value(QStringLiteral("solitaire/") + key, fallback).toBool();
}

void SolitaireGame::setOption(const QString& key, bool on)
{
    if (option(key, !on) == on && m_settings.contains(QStringLiteral("solitaire/") + key))
        return;
    m_settings.setValue(QStringLiteral("solitaire/") + key, on);
    emit optionsChanged();
    if (key == QLatin1String("winnableDeals")) {
        requestDeal(1, on);
        requestDeal(3, on);
    }
}
