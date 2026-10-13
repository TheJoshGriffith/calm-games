#include "PinballGame.h"

#include <QRandomGenerator>

#include <cmath>
#include <numbers>

using pinball::EventType;
using pinball::Mode;

namespace {

QVariantMap point(pinball::Vec2 p)
{
    return { { QStringLiteral("x"), p.x }, { QStringLiteral("y"), p.y } };
}

qreal degrees(double radians)
{
    return radians * 180.0 / std::numbers::pi;
}

} // namespace

PinballGame::PinballGame(QObject* parent)
    : GameSession(parent)
    , m_table(std::make_unique<pinball::Table>())
    , m_settings()
{
    m_frameTimer.setTimerType(Qt::PreciseTimer);
    m_frameTimer.setInterval(16);
    connect(&m_frameTimer, &QTimer::timeout, this, &PinballGame::onFrame);
    connect(this, &GameSession::activeChanged, this, &PinballGame::updateRunning);
    connect(this, &GameSession::completeChanged, this, &PinballGame::updateRunning);
    connect(this, &GameSession::pausedChanged, this, &PinballGame::updateRunning);
}

PinballGame::~PinballGame() = default;

QVariantList PinballGame::outline() const
{
    QVariantList out;
    for (const auto& p : m_table->outline())
        out.append(point(p));
    return out;
}

QVariantList PinballGame::aprons() const
{
    QVariantList out;
    for (const auto& apron : m_table->aprons()) {
        QVariantList points;
        for (const auto& p : apron)
            points.append(point(p));
        out.append(QVariant(points));
    }
    return out;
}

QVariantList PinballGame::walls() const
{
    QVariantList out;
    for (const auto& w : m_table->walls()) {
        if (w.sling >= 0 || !w.drawn)
            continue; // slingshots are drawn whole; guards stay hidden
        out.append(QVariant(QVariantList { point(w.a), point(w.b) }));
    }
    return out;
}

QVariantList PinballGame::bumpers() const
{
    QVariantList out;
    for (const auto& b : m_table->bumpers()) {
        QVariantMap m = point(b.centre);
        m.insert(QStringLiteral("r"), b.radius);
        out.append(m);
    }
    return out;
}

QVariantList PinballGame::slingshots() const
{
    QVariantList out;
    for (const auto& s : m_table->slingshots())
        out.append(QVariant(QVariantList { point(s[0]), point(s[1]), point(s[2]) }));
    return out;
}

QVariantList PinballGame::rollovers() const
{
    QVariantList out;
    for (const auto& r : m_table->rollovers()) {
        QVariantMap m = point(r.centre);
        m.insert(QStringLiteral("r"), r.radius);
        out.append(m);
    }
    return out;
}

QVariantList PinballGame::flippers() const
{
    QVariantList out;
    for (const auto& f : m_table->flippers()) {
        QVariantMap m = point(f.pivot);
        m.insert(QStringLiteral("length"), f.length);
        m.insert(QStringLiteral("radius"), f.radius);
        out.append(m);
    }
    return out;
}

qreal PinballGame::leftAngle() const
{
    return degrees(m_table->flippers()[0].angle);
}

qreal PinballGame::rightAngle() const
{
    return degrees(m_table->flippers()[1].angle);
}

QVariantList PinballGame::lights() const
{
    QVariantList out;
    for (const auto& r : m_table->rollovers())
        out.append(r.lit);
    return out;
}

qreal PinballGame::best() const
{
    return bestFor(level());
}

qreal PinballGame::bestFor(const QString& level) const
{
    return m_settings.value(QStringLiteral("pinball/best/") + level, 0).toDouble();
}

void PinballGame::setOnTable(bool on)
{
    if (m_onTable == on)
        return;
    m_onTable = on;
    if (!on) {
        m_table->setFlipper(pinball::Side::Left, false);
        m_table->setFlipper(pinball::Side::Right, false);
    }
    emit onTableChanged();
    if (active() && !complete())
        setPaused(!on);
    updateRunning();
}

void PinballGame::setFlipper(int side, bool pressed)
{
    if (side != 0 && side != 1)
        return;
    const bool was = m_table->flippers()[side].pressed;
    m_table->setFlipper(side == 0 ? pinball::Side::Left : pinball::Side::Right, pressed && active() && !complete());
    if (!was && m_table->flippers()[side].pressed)
        emit flipped(side);
}

void PinballGame::setPlunger(bool held)
{
    if (!active() || complete()) {
        m_table->setPlunger(false);
        return;
    }
    m_table->setPlunger(held);
    handleEvents();
}

void PinballGame::advance(double seconds)
{
    if (!active() || complete())
        return;
    m_table->step(seconds);
    handleEvents();
    emit frame();
}

void PinballGame::createGame(const QString& level)
{
    m_table = std::make_unique<pinball::Table>(modeFor(level), QRandomGenerator::global()->generate64());
    m_lastScore = 0;
    emit scoreChanged();
    emit bestChanged();
    emit frame();
    beginGame();
}

QVariantMap PinballGame::saveState() const
{
    return {
        { QStringLiteral("mode"), endless() ? QStringLiteral("endless") : QStringLiteral("classic") },
        { QStringLiteral("score"), qreal(m_table->score()) },
        { QStringLiteral("multiplier"), m_table->multiplier() },
        { QStringLiteral("ballsLeft"), m_table->ballsLeft() },
        { QStringLiteral("drained"), m_table->drained() },
        { QStringLiteral("lit"), m_table->litMask() },
    };
}

bool PinballGame::restoreState(const QVariantMap& state)
{
    const QString mode = state.value(QStringLiteral("mode")).toString();
    if (mode != QLatin1String("endless") && mode != QLatin1String("classic"))
        return false;
    m_table = std::make_unique<pinball::Table>(mode == QLatin1String("endless") ? Mode::Endless : Mode::Classic,
        QRandomGenerator::global()->generate64());
    m_table->restore(qint64(state.value(QStringLiteral("score")).toDouble()),
        state.value(QStringLiteral("multiplier"), 1).toInt(),
        state.value(QStringLiteral("ballsLeft"), pinball::Table::StartingBalls).toInt(),
        state.value(QStringLiteral("drained")).toInt(),
        state.value(QStringLiteral("lit")).toUInt());
    m_lastScore = m_table->score();
    emit scoreChanged();
    emit bestChanged();
    emit frame();
    return true;
}

// Pinball has no undo, but the session keeps a history of each ball's
// starting point; a snapshot is just the saved scoreboard.
QVariant PinballGame::snapshot() const
{
    return saveState();
}

void PinballGame::applySnapshot(const QVariant& snapshot)
{
    restoreState(snapshot.toMap());
}

Mode PinballGame::modeFor(const QString& level)
{
    return level == QLatin1String("Classic") ? Mode::Classic : Mode::Endless;
}

void PinballGame::handleEvents()
{
    const auto events = m_table->takeEvents();
    for (const auto& e : events) {
        switch (e.type) {
        case EventType::Launch:
            emitFeedback(QStringLiteral("soft"));
            break;
        case EventType::Bumper:
            emit bumperHit(e.index);
            emitFeedback(QStringLiteral("tap"));
            break;
        case EventType::Slingshot:
            emit slingshotHit(e.index);
            emitFeedback(QStringLiteral("soft"));
            break;
        case EventType::Rollover:
            emitFeedback(QStringLiteral("tap"));
            break;
        case EventType::RolloverSet:
            // Each completed set chimes a little higher, like Sudoku's units.
            emitFeedback(QStringLiteral("complete"), e.index - 2);
            break;
        case EventType::Flipper:
        case EventType::Wall:
            break; // the table's own knocks stay quiet
        case EventType::Drain:
            emit ballDrained();
            emitFeedback(QStringLiteral("erase"));
            updateBest();
            // Autosave between balls (and keep the scoreboard for resume).
            beginMove();
            endMove();
            break;
        case EventType::GameOver:
            updateBest();
            finishGame();
            break;
        }
    }
    if (m_table->score() != m_lastScore || !events.empty()) {
        m_lastScore = m_table->score();
        emit scoreChanged();
    }
}

void PinballGame::updateBest()
{
    const QString key = QStringLiteral("pinball/best/") + level();
    if (qreal(m_table->score()) > m_settings.value(key, 0).toDouble()) {
        m_settings.setValue(key, qreal(m_table->score()));
        emit bestChanged();
    }
}

void PinballGame::updateRunning()
{
    const bool run = m_onTable && active() && !complete() && !paused();
    if (run && !m_frameTimer.isActive()) {
        m_clock.start();
        m_frameTimer.start();
    } else if (!run && m_frameTimer.isActive()) {
        m_frameTimer.stop();
        updateBest();
    }
}

void PinballGame::onFrame()
{
    const double seconds = m_clock.restart() / 1000.0;
    advance(seconds);
}
