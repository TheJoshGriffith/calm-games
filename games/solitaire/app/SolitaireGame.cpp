#include "SolitaireGame.h"

#include <algorithm>
#include <iterator>

#include <QRandomGenerator>

#if CALM_SOLITAIRE_THREADS
#include <QtConcurrent/QtConcurrentRun>
#endif

using solitaire::DrawResult;
using solitaire::Game;
using solitaire::Move;
using solitaire::PileKind;
using solitaire::PileRef;
using solitaire::Variant;

namespace {

// Interval between cards when finishing automatically.
constexpr int FinishStepMs = 110;

struct Level {
    const char* name;
    Variant variant;
    int option;
};

constexpr Level Levels[] = {
    { "Klondike", Variant::Klondike, 1 },
    { "Klondike · Draw three", Variant::Klondike, 3 },
    { "FreeCell", Variant::FreeCell, 0 },
    { "Spider · One suit", Variant::Spider, 1 },
    { "Spider · Two suits", Variant::Spider, 2 },
    { "Spider · Four suits", Variant::Spider, 4 },
    { "Yukon", Variant::Yukon, 0 },
    { "Pyramid", Variant::Pyramid, 0 },
    { "TriPeaks", Variant::TriPeaks, 0 },
    { "Golf", Variant::Golf, 0 },
};
constexpr int LevelCount = int(std::size(Levels));

int levelIndex(const QString& name)
{
    for (int i = 0; i < LevelCount; ++i)
        if (name == QString::fromUtf8(Levels[i].name))
            return i;
    // Saves from before there were other games.
    if (name.contains(QLatin1String("three"), Qt::CaseInsensitive))
        return 1;
    return 0;
}

// How hard to look for a winnable deal: tries, and positions per try.
struct DealPlan {
    int attempts;
    int budget;
};

DealPlan dealPlan(Variant variant)
{
    switch (variant) {
    case Variant::FreeCell:
        return { 10, 60000 };
    case Variant::Spider:
        return { 1, 0 };
    case Variant::Golf:
        return { 25, 400000 };
    case Variant::Klondike:
        return { 40, 150000 };
    default:
        return { 20, 150000 };
    }
}

solitaire::Deal makeDeal(int level, quint64 seed, bool winnable)
{
    const Level& l = Levels[level];
    if (!winnable)
        return { Game::deal(l.variant, seed, l.option), seed, false };
    const DealPlan plan = dealPlan(l.variant);
    return solitaire::dealWinnable(l.variant, seed, l.option, plan.attempts, plan.budget);
}

// Positions a losing-move check may look through.
constexpr int AnalysisBudget = 400000;

// Spider's search space is too large for the solver to prove anything.
bool solverHelps(Variant variant)
{
    return variant != Variant::Spider;
}

// Positions searched for a hint: enough for an easy line, quick to find.
int hintBudget(Variant variant)
{
    return variant == Variant::Klondike ? 60000 : 30000;
}

PileRef pileRef(int pile, int slot)
{
    return { static_cast<PileKind>(std::clamp(pile, 0, 4)), slot };
}

// Foundations alternate colours left to right: spades, diamonds, clubs,
// hearts. Suits are numbered spades, hearts, diamonds, clubs.
constexpr int FoundationPlace[] = { 0, 3, 1, 2 };

QVariantMap place(int pile, int slot, qreal x, qreal y, int fan = 0, int mark = -1, bool outline = true, qreal bottom = 0)
{
    return {
        { QStringLiteral("pile"), pile },
        { QStringLiteral("slot"), slot },
        { QStringLiteral("x"), x },
        { QStringLiteral("y"), y },
        { QStringLiteral("fan"), fan },
        { QStringLiteral("bottom"), bottom },
        { QStringLiteral("mark"), mark },
        { QStringLiteral("outline"), outline },
    };
}

// The row of columns below the top row of piles.
constexpr qreal ColumnsTop = 1.18;

} // namespace

SolitaireGame::SolitaireGame(QObject* parent)
    : GameSession(parent)
{
    m_finishTimer.setInterval(FinishStepMs);
    connect(&m_finishTimer, &QTimer::timeout, this, &SolitaireGame::finishStep);
#if !CALM_SOLITAIRE_THREADS
    m_workTimer.setSingleShot(true);
    connect(&m_workTimer, &QTimer::timeout, this, &SolitaireGame::workNext);
#endif
    // Have a deal of the last game played ready, so it starts instantly.
    requestDeal(levelIndex(m_settings.value(QStringLiteral("solitaire/lastLevel")).toString()), winnableDeals());
}

SolitaireGame::~SolitaireGame()
{
#if CALM_SOLITAIRE_THREADS
    m_stopAnalysis = true;
    if (m_analysis) {
        m_analysis->disconnect(this);
        m_analysis->waitForFinished();
    }
    for (auto* watcher : std::as_const(m_watchers)) {
        watcher->disconnect(this);
        watcher->waitForFinished();
    }
#endif
}

QString SolitaireGame::levelFor(const QString& variant, int option)
{
    const auto v = solitaire::variantFromKey(variant.toStdString());
    if (!v)
        return QString::fromUtf8(Levels[0].name);
    for (const Level& level : Levels)
        if (level.variant == *v && (level.option == option || level.option == 0))
            return QString::fromUtf8(level.name);
    for (const Level& level : Levels)
        if (level.variant == *v)
            return QString::fromUtf8(level.name);
    return QString::fromUtf8(Levels[0].name);
}

int SolitaireGame::klondikeDraw() const
{
    return m_settings.value(QStringLiteral("solitaire/klondikeDraw"), 1).toInt() == 3 ? 3 : 1;
}

void SolitaireGame::setKlondikeDraw(int draw)
{
    if (draw == klondikeDraw())
        return;
    m_settings.setValue(QStringLiteral("solitaire/klondikeDraw"), draw == 3 ? 3 : 1);
    emit optionsChanged();
}

int SolitaireGame::spiderSuits() const
{
    const int suits = m_settings.value(QStringLiteral("solitaire/spiderSuits"), 1).toInt();
    return suits == 2 || suits == 4 ? suits : 1;
}

void SolitaireGame::setSpiderSuits(int suits)
{
    if (suits == spiderSuits())
        return;
    m_settings.setValue(QStringLiteral("solitaire/spiderSuits"), suits == 2 || suits == 4 ? suits : 1);
    emit optionsChanged();
}

QString SolitaireGame::variant() const
{
    return QString::fromUtf8(solitaire::variantKey(m_table.variant()).data());
}

void SolitaireGame::setTable(const Game& table)
{
    const bool reshape = m_cardIndex.empty() || table.variant() != m_table.variant() || table.option() != m_table.option();
    m_table = table;
    if (reshape) {
        const std::vector<int> deck = m_table.deck();
        m_cardIndex.assign(deck.back() + 1, -1);
        for (int i = 0; i < int(deck.size()); ++i)
            m_cardIndex[deck[i]] = i;
    }
    setSelected(-1);
}

int SolitaireGame::indexOf(const solitaire::Card& card) const
{
    return card.id < m_cardIndex.size() ? m_cardIndex[card.id] : -1;
}

QVariantList SolitaireGame::cards() const
{
    QVariantList out;
    for (int i = 0; i < m_table.deckSize(); ++i)
        out.append(QVariantMap());

    auto add = [&](PileRef ref) {
        const solitaire::Pile& pile = m_table.pile(ref);
        const int count = int(pile.size());
        for (int pos = 0; pos < count; ++pos) {
            const solitaire::Card& card = pile[pos];
            const int index = indexOf(card);
            if (index < 0)
                continue;
            QVariantMap map;
            map.insert(QStringLiteral("rank"), card.rank());
            map.insert(QStringLiteral("suit"), int(card.suit()));
            map.insert(QStringLiteral("red"), card.red());
            map.insert(QStringLiteral("faceUp"), card.faceUp);
            map.insert(QStringLiteral("pile"), int(ref.kind));
            map.insert(QStringLiteral("slot"), ref.index);
            map.insert(QStringLiteral("pos"), pos);
            map.insert(QStringLiteral("count"), count);
            map.insert(QStringLiteral("movable"), m_table.canPick(ref, pos));
            out[index] = map;
        }
    };
    add({ PileKind::Stock, 0 });
    add({ PileKind::Waste, 0 });
    for (int i = 0; i < m_table.foundationCount(); ++i)
        add({ PileKind::Foundation, i });
    for (int i = 0; i < m_table.tableauCount(); ++i)
        add({ PileKind::Tableau, i });
    for (int i = 0; i < m_table.cellCount(); ++i)
        add({ PileKind::Cell, i });
    return out;
}

QVariantList SolitaireGame::piles() const
{
    QVariantList out;
    const Variant v = m_table.variant();
    switch (v) {
    case Variant::Klondike:
    case Variant::Yukon:
    case Variant::FreeCell: {
        const bool freecell = v == Variant::FreeCell;
        const int homeX = freecell ? 4 : 3;
        if (v == Variant::Klondike) {
            out.append(place(Stock, 0, 0, 0));
            out.append(place(Waste, 0, 1, 0, m_table.drawCount() > 1 ? 2 : 0, -1, false));
        }
        for (int c = 0; c < m_table.cellCount(); ++c)
            out.append(place(Cell, c, c, 0));
        for (int s = 0; s < solitaire::SuitCount; ++s)
            out.append(place(Foundation, s, homeX + FoundationPlace[s], 0, 0, s));
        for (int c = 0; c < m_table.tableauCount(); ++c)
            out.append(place(Tableau, c, c, ColumnsTop, 1));
        break;
    }
    case Variant::Spider:
        out.append(place(Stock, 0, 0, 0));
        for (int f = 0; f < m_table.foundationCount(); ++f)
            out.append(place(Foundation, f, 2 + f, 0));
        for (int c = 0; c < m_table.tableauCount(); ++c)
            out.append(place(Tableau, c, c, ColumnsTop, 1));
        break;
    case Variant::Pyramid:
        for (int row = 0, index = 0; row < 7; ++row)
            for (int column = 0; column <= row; ++column, ++index)
                out.append(place(Tableau, index, 3 - row * 0.5 + column, row * 0.5, 0, -1, false));
        out.append(place(Stock, 0, 0, 4.25));
        out.append(place(Waste, 0, 1, 4.25, 0, -1, false));
        out.append(place(Foundation, 0, 6, 4.25));
        break;
    case Variant::TriPeaks: {
        auto add = [&](int index, qreal x, int row) { out.append(place(Tableau, index, x, row * 0.5, 0, -1, false)); };
        for (int p = 0; p < 3; ++p)
            add(p, p * 3 + 1.5, 0);
        for (int j = 0; j < 6; ++j)
            add(3 + j, (j / 2) * 3 + j % 2 + 1, 1);
        for (int j = 0; j < 9; ++j)
            add(9 + j, j + 0.5, 2);
        for (int j = 0; j < 10; ++j)
            add(18 + j, j, 3);
        out.append(place(Stock, 0, 3.5, 2.8));
        out.append(place(Waste, 0, 5.5, 2.8, 0, -1, false));
        break;
    }
    case Variant::Golf:
        for (int c = 0; c < m_table.tableauCount(); ++c)
            out.append(place(Tableau, c, c, 0, 1, -1, true, 2.35));
        out.append(place(Stock, 0, 2, 2.6));
        out.append(place(Waste, 0, 4, 2.6, 0, -1, false));
        break;
    }
    return out;
}

qreal SolitaireGame::tableColumns() const
{
    switch (m_table.variant()) {
    case Variant::FreeCell:
        return 8;
    case Variant::Spider:
    case Variant::TriPeaks:
        return 10;
    default:
        return 7;
    }
}

qreal SolitaireGame::tableRows() const
{
    switch (m_table.variant()) {
    case Variant::Pyramid:
        return 5.25;
    case Variant::TriPeaks:
        return 3.8;
    case Variant::Golf:
        return 3.6;
    default:
        return ColumnsTop + 3.2; // room for the columns to fan
    }
}

bool SolitaireGame::tapToPlay() const
{
    const Variant v = m_table.variant();
    return v == Variant::Pyramid || v == Variant::TriPeaks || v == Variant::Golf;
}

std::optional<SolitaireGame::Location> SolitaireGame::locate(int card) const
{
    if (card < 0 || card >= m_table.deckSize())
        return std::nullopt;
    auto find = [&](PileRef ref) -> std::optional<Location> {
        const solitaire::Pile& pile = m_table.pile(ref);
        for (int pos = 0; pos < int(pile.size()); ++pos)
            if (indexOf(pile[pos]) == card)
                return Location { ref, pos };
        return std::nullopt;
    };
    if (auto l = find({ PileKind::Waste, 0 }))
        return l;
    for (int i = 0; i < m_table.cellCount(); ++i)
        if (auto l = find({ PileKind::Cell, i }))
            return l;
    for (int i = 0; i < m_table.foundationCount(); ++i)
        if (auto l = find({ PileKind::Foundation, i }))
            return l;
    for (int i = 0; i < m_table.tableauCount(); ++i)
        if (auto l = find({ PileKind::Tableau, i }))
            return l;
    return find({ PileKind::Stock, 0 });
}

void SolitaireGame::draw()
{
    if (!active() || complete() || autoFinishing() || !m_table.canDraw())
        return;
    const std::string before = m_table.serialize();
    beginMove();
    clearHint();
    setSelected(-1);
    setLosingMove(false);
    const DrawResult result = m_table.draw();
    ++m_moves;
    emitFeedback(result == DrawResult::Recycled ? QStringLiteral("erase") : QStringLiteral("soft"));
    endMove();
    // Running out of stock can lose TriPeaks, Golf and Pyramid.
    if (m_table.variant() != Variant::Klondike)
        checkForLosingMove(before);
}

bool SolitaireGame::perform(const Move& move)
{
    if (!m_table.canMove(move))
        return false;
    const std::string before = m_table.serialize();
    beginMove();
    clearHint();
    setSelected(-1);
    setLosingMove(false);
    const solitaire::MoveResult result = m_table.move(move);
    ++m_moves;
    if (m_table.won()) {
        endMove();
        stopAutoFinish();
        finishGame();
        return true;
    }
    if (result.toFoundation)
        emitFeedback(QStringLiteral("complete"), m_table.progress() * 8 / (m_table.goal() + 1));
    else
        emitFeedback(QStringLiteral("tap"));
    endMove();
    if (!autoFinishing())
        checkForLosingMove(before);
    return true;
}

bool SolitaireGame::quickMove(int card)
{
    if (!active() || complete() || autoFinishing())
        return false;
    const auto where = locate(card);
    if (!where || where->pile.kind == PileKind::Stock)
        return false;
    if (const auto best = m_table.bestMove(where->pile, where->pos))
        return perform(*best);
    emitFeedback(QStringLiteral("error"));
    return false;
}

void SolitaireGame::setSelected(int card)
{
    if (m_selected == card)
        return;
    m_selected = card;
    emit selectedCardChanged();
}

bool SolitaireGame::tapCard(int card)
{
    if (!active() || complete() || autoFinishing())
        return false;
    if (m_table.variant() != Variant::Pyramid)
        return quickMove(card);
    const auto where = locate(card);
    if (!where || !m_table.canPick(where->pile, where->pos)) {
        emitFeedback(QStringLiteral("error"));
        return false;
    }
    if (m_table.pile(where->pile)[where->pos].rank() == solitaire::RankCount)
        return perform({ where->pile, where->pos, { PileKind::Foundation, 0 } });
    if (m_selected == card) {
        setSelected(-1);
        return true;
    }
    if (const auto first = m_selected >= 0 ? locate(m_selected) : std::nullopt) {
        const Move pair { first->pile, first->pos, where->pile };
        if (perform(pair))
            return true;
    }
    setSelected(card);
    emitFeedback(QStringLiteral("tap"));
    return true;
}

bool SolitaireGame::canDrop(int card, int pile, int slot) const
{
    const auto where = locate(card);
    return where && m_table.canMove({ where->pile, where->pos, pileRef(pile, slot) });
}

bool SolitaireGame::dropCard(int card, int pile, int slot)
{
    if (!active() || complete() || autoFinishing())
        return false;
    const auto where = locate(card);
    if (!where)
        return false;
    PileRef to = pileRef(pile, slot);
    // Any foundation will do: cards go to their own suit's.
    const Variant v = m_table.variant();
    if (to.kind == PileKind::Foundation && (v == Variant::Klondike || v == Variant::FreeCell || v == Variant::Yukon)) {
        const solitaire::Pile& source = m_table.pile(where->pile);
        if (where->pos < int(source.size()))
            to.index = int(source[where->pos].suit());
    }
    // Any empty cell will do.
    if (to.kind == PileKind::Cell && to.index < m_table.cellCount() && !m_table.cell(to.index).empty()) {
        for (int c = 0; c < m_table.cellCount(); ++c)
            if (m_table.cell(c).empty())
                to.index = c;
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
            ids.append(indexOf(source[pos]));
        // A Pyramid pair shows both cards.
        const PileRef to = hint->move.to;
        if (m_table.variant() == Variant::Pyramid && to.kind != PileKind::Foundation && !m_table.pile(to).empty())
            ids.append(indexOf(m_table.pile(to).back()));
        map.insert(QStringLiteral("stock"), false);
        map.insert(QStringLiteral("cards"), ids);
        map.insert(QStringLiteral("pile"), int(to.kind));
        map.insert(QStringLiteral("slot"), to.index);
    }
    m_hint = map;
    emit hintChanged();
    emitFeedback(hint ? QStringLiteral("soft") : QStringLiteral("error"));
    return hint.has_value();
}

std::optional<solitaire::Hint> SolitaireGame::plannedStep()
{
    if (!solverHelps(m_table.variant()))
        return std::nullopt;
    // Following the plan step by step never revisits a position, so hints
    // can't lead the player round in circles.
    const std::string now = m_table.serialize();
    for (const auto& [state, step] : m_plan)
        if (state == now)
            return step;
    m_plan.clear();
    const auto path = solitaire::solvePath(m_table, hintBudget(m_table.variant()));
    if (!path || path->empty())
        return std::nullopt;
    Game replay = m_table;
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

bool SolitaireGame::canAutoFinish() const
{
    return active() && !complete() && m_table.canAutoFinish() && finishPath().has_value();
}

std::optional<std::vector<solitaire::Hint>> SolitaireGame::finishPath() const
{
    // Only asked once the rest is plain sailing, when the search is short.
    const std::string now = m_table.serialize();
    if (now != m_finishFor) {
        m_finishFor = now;
        m_finishFound = solitaire::solvePath(m_table);
    }
    return m_finishFound;
}

void SolitaireGame::autoFinish()
{
    if (!canAutoFinish() || autoFinishing())
        return;
    clearHint();
    setLosingMove(false);
    m_finishSteps = *finishPath();
    std::reverse(m_finishSteps.begin(), m_finishSteps.end());
    m_finishTimer.start();
    emit autoFinishingChanged();
    finishStep();
}

void SolitaireGame::finishStep()
{
    if (paused())
        return;
    if (m_finishSteps.empty() || complete()) {
        stopAutoFinish();
        return;
    }
    const solitaire::Hint step = m_finishSteps.back();
    m_finishSteps.pop_back();
    if (step.draw) {
        beginMove();
        m_table.draw();
        ++m_moves;
        emitFeedback(QStringLiteral("soft"));
        endMove();
    } else if (!perform(step.move)) {
        stopAutoFinish();
    }
}

void SolitaireGame::stopAutoFinish()
{
    m_finishSteps.clear();
    if (!m_finishTimer.isActive())
        return;
    m_finishTimer.stop();
    emit autoFinishingChanged();
}

void SolitaireGame::setLosingMove(bool losing)
{
    if (m_losingMove == losing)
        return;
    m_losingMove = losing;
    emit losingMoveChanged();
}

void SolitaireGame::checkForLosingMove(const std::string& before)
{
    if (!warnLosingMoves() || !solverHelps(m_table.variant()))
        return;
    // Positions on the hint's winning line are known to be winnable.
    const std::string now = m_table.serialize();
    for (const auto& planned : m_plan)
        if (planned.first == now)
            return;
    m_pendingAnalysis = Analysis { before, m_table };
#if CALM_SOLITAIRE_THREADS
    if (!m_analysis)
        startAnalysis();
#else
    scheduleWork();
#endif
}

void SolitaireGame::startAnalysis()
{
    if (!m_pendingAnalysis)
        return;
    const Analysis job = *m_pendingAnalysis;
    m_pendingAnalysis.reset();

    const QByteArray after = QByteArray::fromStdString(job.after.serialize());
    if (m_verdicts.contains(after)) {
        // Seen before (say, after undo and the same move again).
        if (m_verdicts.value(after) == int(solitaire::Verdict::Unwinnable)
            && m_verdicts.value(QByteArray::fromStdString(job.before), -1) != int(solitaire::Verdict::Unwinnable)
            && m_table.serialize() == job.after.serialize())
            setLosingMove(true);
#if CALM_SOLITAIRE_THREADS
        startAnalysis();
#endif
        return;
    }

#if CALM_SOLITAIRE_THREADS
    m_analysis = new QFutureWatcher<solitaire::Verdict>(this);
    connect(m_analysis, &QFutureWatcherBase::finished, this, [this, job, after] {
        const solitaire::Verdict verdict = m_analysis->result();
        m_analysis->deleteLater();
        m_analysis = nullptr;
        onVerdict(job.before, after, verdict);
        startAnalysis();
    });
    const Game table = job.after;
    std::atomic_bool* stop = &m_stopAnalysis;
    m_analysis->setFuture(QtConcurrent::run([table, stop] { return solitaire::solveExhaustive(table, AnalysisBudget, stop); }));
#else
    m_runningAnalysis.emplace(RunningAnalysis { job.before, after, solitaire::SteppedSolve(job.after, AnalysisBudget) });
    scheduleWork();
#endif
}

void SolitaireGame::onVerdict(const std::string& before, const QByteArray& after, solitaire::Verdict verdict)
{
    if (verdict != solitaire::Verdict::Unknown)
        m_verdicts.insert(after, int(verdict));
    // Warn only while that position is still on the table, and only for the
    // move that lost the game, not every move after it.
    const bool wasLost = m_verdicts.value(QByteArray::fromStdString(before), -1) == int(solitaire::Verdict::Unwinnable);
    if (verdict == solitaire::Verdict::Unwinnable && !wasLost && active() && !complete()
        && QByteArray::fromStdString(m_table.serialize()) == after) {
        setLosingMove(true);
        emitFeedback(QStringLiteral("error"));
    }
}

void SolitaireGame::createGame(const QString& name)
{
    stopAutoFinish();
    const int level = levelIndex(name);
    m_settings.setValue(QStringLiteral("solitaire/lastLevel"), QString::fromUtf8(Levels[level].name));
    const int key = bufferKey(level, winnableDeals());
    auto& queue = m_buffer[key];
    if (!queue.isEmpty()) {
        startWith(queue.takeFirst());
        requestDeal(level, winnableDeals());
        return;
    }
    m_waitingFor = key;
    if (m_inFlight.value(key) == 0)
        requestDeal(level, winnableDeals());
#if !CALM_SOLITAIRE_THREADS
    scheduleWork();
#endif
}

void SolitaireGame::requestDeal(int level, bool winnable)
{
    const int key = bufferKey(level, winnable);
    if (m_buffer.value(key).size() + m_inFlight.value(key) >= 1)
        return;
    ++m_inFlight[key];

    const quint64 seed = QRandomGenerator::global()->generate64();
#if CALM_SOLITAIRE_THREADS
    auto* watcher = new QFutureWatcher<solitaire::Deal>(this);
    m_watchers.append(watcher);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, key] {
        m_watchers.removeOne(watcher);
        const solitaire::Deal deal = watcher->result();
        watcher->deleteLater();
        onDealReady(key, deal);
    });
    watcher->setFuture(QtConcurrent::run([level, seed, winnable] { return makeDeal(level, seed, winnable); }));
#else
    const Level& l = Levels[level];
    const DealPlan plan = dealPlan(l.variant);
    m_pendingDeals.append(std::make_shared<DealJob>(DealJob {
        key, solitaire::SteppedDeal(l.variant, seed, l.option, winnable ? plan.attempts : 1, winnable ? plan.budget : 0) }));
    scheduleWork();
#endif
}

#if !CALM_SOLITAIRE_THREADS
// Slices small enough to keep frames coming while the solver works.
constexpr int SliceNodes = 2500;
constexpr int TickMs = 16;

void SolitaireGame::scheduleWork()
{
    if (m_pendingDeals.isEmpty() && !m_pendingAnalysis && !m_runningAnalysis)
        return;
    // Leave time for frames between slices, and let the first screen appear
    // before warming the buffers. A deal the player is waiting for goes
    // first, with less time between its slices.
    const int delay = m_waitingFor ? 1 : m_warmedUp ? TickMs : 600;
    if (!m_workTimer.isActive() || m_workTimer.remainingTime() > delay)
        m_workTimer.start(delay);
}

void SolitaireGame::workNext()
{
    m_warmedUp = true;
    qsizetype next = -1;
    for (qsizetype i = 0; i < m_pendingDeals.size(); ++i) {
        if (m_pendingDeals.at(i)->key == m_waitingFor) {
            next = i;
            break;
        }
    }
    if (next < 0 && m_runningAnalysis) {
        if (const auto verdict = m_runningAnalysis->search.step(SliceNodes)) {
            const RunningAnalysis done = std::move(*m_runningAnalysis);
            m_runningAnalysis.reset();
            onVerdict(done.before, done.after, *verdict);
        }
    } else if (next < 0 && m_pendingAnalysis) {
        startAnalysis();
    } else if (next >= 0 || !m_pendingDeals.isEmpty()) {
        const auto job = m_pendingDeals.at(std::max<qsizetype>(next, 0));
        if (const auto deal = job->search.step(SliceNodes)) {
            m_pendingDeals.removeOne(job);
            onDealReady(job->key, *deal);
        }
    }
    scheduleWork();
}
#endif

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
    setTable(deal.game);
    m_seed = deal.seed;
    m_moves = 0;
    clearHint();
    m_plan.clear();
    m_verdicts.clear();
    m_pendingAnalysis.reset();
    setLosingMove(false);
    if (deal.proven)
        m_verdicts.insert(QByteArray::fromStdString(m_table.serialize()), int(solitaire::Verdict::Winnable));
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
    const auto table = Game::parse(state.value(QStringLiteral("table")).toString().toStdString());
    if (!table || table->won())
        return false;
    stopAutoFinish();
    setTable(*table);
    m_seed = state.value(QStringLiteral("seed")).toString().toULongLong();
    m_moves = std::max(0, state.value(QStringLiteral("moves")).toInt());
    clearHint();
    m_plan.clear();
    m_verdicts.clear();
    m_pendingAnalysis.reset();
    setLosingMove(false);
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
    const auto table = Game::parse(parts.at(0).toString().toStdString());
    if (!table)
        return;
    stopAutoFinish();
    clearHint();
    setLosingMove(false);
    m_pendingAnalysis.reset();
    setTable(*table);
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
    if (key == QLatin1String("winnableDeals"))
        requestDeal(levelIndex(level()), on);
}
