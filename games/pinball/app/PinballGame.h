#pragma once

#include <GameSession.h>

#include <pinball/Table.h>

#include <QElapsedTimer>
#include <QQmlEngine>
#include <QSettings>
#include <QTimer>
#include <QVariantList>

#include <memory>

// The pinball game session: steps the table's physics on a frame timer while
// the table is on screen, and turns table events into score, saves and the
// framework's shared sounds and haptics.
//
// Levels: "Gentle" (endless balls, softer gravity) and "Classic" (three balls).
class PinballGame : public GameSession {
    Q_OBJECT
    QML_ELEMENT

    // Table geometry, in table units (see tableWidth/tableHeight).
    Q_PROPERTY(qreal tableWidth READ tableWidth CONSTANT)
    Q_PROPERTY(qreal tableHeight READ tableHeight CONSTANT)
    Q_PROPERTY(qreal ballRadius READ ballRadius CONSTANT)
    Q_PROPERTY(qreal laneLeft READ laneLeft CONSTANT)
    Q_PROPERTY(QVariantList outline READ outline CONSTANT)  // [{x, y}]
    Q_PROPERTY(QVariantList aprons READ aprons CONSTANT)    // [[{x, y}]]
    Q_PROPERTY(QVariantList walls READ walls CONSTANT)      // [[{x, y}, {x, y}]]
    Q_PROPERTY(QVariantList bumpers READ bumpers CONSTANT)  // [{x, y, r}]
    Q_PROPERTY(QVariantList slingshots READ slingshots CONSTANT) // [[{x, y} x3]]
    Q_PROPERTY(QVariantList rollovers READ rollovers CONSTANT)   // [{x, y, r}]
    Q_PROPERTY(QVariantList flippers READ flippers CONSTANT)     // [{x, y, length, radius}]

    // Moving parts, updated every frame.
    Q_PROPERTY(qreal ballX READ ballX NOTIFY frame)
    Q_PROPERTY(qreal ballY READ ballY NOTIFY frame)
    Q_PROPERTY(qreal leftAngle READ leftAngle NOTIFY frame)   // degrees
    Q_PROPERTY(qreal rightAngle READ rightAngle NOTIFY frame)
    Q_PROPERTY(qreal plungerCharge READ plungerCharge NOTIFY frame)
    Q_PROPERTY(bool ballReady READ ballReady NOTIFY frame)

    Q_PROPERTY(qreal score READ score NOTIFY scoreChanged)
    Q_PROPERTY(int multiplier READ multiplier NOTIFY scoreChanged)
    Q_PROPERTY(int ballsLeft READ ballsLeft NOTIFY scoreChanged)
    Q_PROPERTY(bool endless READ endless NOTIFY scoreChanged)
    Q_PROPERTY(QVariantList lights READ lights NOTIFY scoreChanged)
    Q_PROPERTY(qreal best READ best NOTIFY bestChanged)

    // True while the table is on screen; physics only runs then.
    Q_PROPERTY(bool onTable READ onTable WRITE setOnTable NOTIFY onTableChanged)

public:
    explicit PinballGame(QObject* parent = nullptr);
    ~PinballGame() override;

    QString gameId() const override { return QStringLiteral("pinball"); }

    qreal tableWidth() const { return pinball::TableWidth; }
    qreal tableHeight() const { return pinball::TableHeight; }
    qreal ballRadius() const { return pinball::BallRadius; }
    qreal laneLeft() const { return m_table->laneLeft(); }
    QVariantList outline() const;
    QVariantList aprons() const;
    QVariantList walls() const;
    QVariantList bumpers() const;
    QVariantList slingshots() const;
    QVariantList rollovers() const;
    QVariantList flippers() const;

    qreal ballX() const { return m_table->ball().position.x; }
    qreal ballY() const { return m_table->ball().position.y; }
    qreal leftAngle() const;
    qreal rightAngle() const;
    qreal plungerCharge() const { return m_table->plungerCharge(); }
    bool ballReady() const { return m_table->ballReady(); }

    qreal score() const { return qreal(m_table->score()); }
    int multiplier() const { return m_table->multiplier(); }
    int ballsLeft() const { return m_table->ballsLeft(); }
    bool endless() const { return m_table->mode() == pinball::Mode::Endless; }
    QVariantList lights() const;
    qreal best() const;
    Q_INVOKABLE qreal bestFor(const QString& level) const;

    bool onTable() const { return m_onTable; }
    void setOnTable(bool on);

    // side: 0 = left, 1 = right.
    Q_INVOKABLE void setFlipper(int side, bool pressed);
    Q_INVOKABLE void setPlunger(bool held);
    // Advances the table by `seconds`; the frame timer calls this.
    Q_INVOKABLE void advance(double seconds);

signals:
    void frame();
    void scoreChanged();
    void bestChanged();
    void onTableChanged();
    void bumperHit(int index);
    void slingshotHit(int index);
    void flipped(int side);
    void ballDrained();

protected:
    void createGame(const QString& level) override;
    QVariantMap saveState() const override;
    bool restoreState(const QVariantMap& state) override;
    QVariant snapshot() const override;
    void applySnapshot(const QVariant& snapshot) override;

private:
    static pinball::Mode modeFor(const QString& level);
    void handleEvents();
    void updateBest();
    void updateRunning();
    void onFrame();

    std::unique_ptr<pinball::Table> m_table;
    QTimer m_frameTimer;
    QElapsedTimer m_clock;
    bool m_onTable = false;
    long long m_lastScore = 0;
    mutable QSettings m_settings;
};
