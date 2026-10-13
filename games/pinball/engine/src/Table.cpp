#include <pinball/Table.h>

#include <algorithm>
#include <numbers>

namespace pinball {

namespace {

constexpr double Pi = std::numbers::pi;
constexpr double deg(double degrees) { return degrees * Pi / 180.0; }

constexpr double MaxSpeed = 300;
constexpr double FlipperUpSpeed = 24;   // radians per second
constexpr double FlipperDownSpeed = 14;
constexpr double FlipperRestitution = 0.35;
constexpr double BumperRestitution = 0.6;
constexpr double BumperKick = 80;       // minimum speed leaving a bumper
constexpr double SlingKick = 55;
constexpr double ChargeSeconds = 0.9;   // time to draw the plunger fully back
constexpr double LaneFloor = 170;

constexpr long long BumperPoints = 100;
constexpr long long SlingPoints = 25;
constexpr long long RolloverPoints = 250;
constexpr long long RolloverSetPoints = 1000;
constexpr int MaxMultiplier = 5;

// Closest point to p on segment ab.
Vec2 closestPoint(Vec2 p, Vec2 a, Vec2 b)
{
    const Vec2 ab = b - a;
    const double len2 = ab.dot(ab);
    const double t = len2 > 0 ? std::clamp((p - a).dot(ab) / len2, 0.0, 1.0) : 0.0;
    return a + ab * t;
}

} // namespace

Table::Table(Mode mode, std::uint64_t seed)
    : m_mode(mode)
    , m_rng(seed)
    , m_gravity(mode == Mode::Endless ? 115 : 135)
{
    build();
    returnToLane();
}

void Table::build()
{
    auto wall = [this](Vec2 a, Vec2 b, double restitution = 0.45) {
        m_walls.push_back({ a, b, restitution });
    };

    // The top arch: half an ellipse from the left wall over to the launch lane,
    // with a low bounce so a launched ball rides it round into the playfield.
    const Vec2 archCentre { 53.5, 34 };
    const double rx = 42.5, ry = 30;
    constexpr int ArchSegments = 28;
    std::vector<Vec2> arch;
    for (int i = 0; i <= ArchSegments; ++i) {
        const double a = Pi + Pi * i / ArchSegments;
        arch.push_back({ archCentre.x + rx * std::cos(a), archCentre.y + ry * std::sin(a) });
    }
    for (int i = 0; i < ArchSegments; ++i)
        wall(arch[i], arch[i + 1], 0.2);

    const Vec2 leftTop { 11, 34 }, leftBottom { 11, 124 };
    const Vec2 rightBottom { 89, 124 };
    const Vec2 leftPivot { 30, 148 }, rightPivot { 70, 148 };
    const Vec2 leftSlopeEnd { 28.4, 146.6 }, rightSlopeEnd { 71.6, 146.6 };

    wall(leftTop, leftBottom);
    wall(leftBottom, leftSlopeEnd, 0.3);
    wall(rightBottom, rightSlopeEnd, 0.3);
    // Under the flippers, so a ball that slips past drains straight down.
    m_walls.push_back({ leftSlopeEnd, { leftSlopeEnd.x, TableHeight + 10 }, 0.2, 0, false, -1, false });
    m_walls.push_back({ rightSlopeEnd, { rightSlopeEnd.x, TableHeight + 10 }, 0.2, 0, false, -1, false });
    wall({ LaneRight, 34 }, { LaneRight, LaneFloor });         // outer wall
    wall({ LaneLeft, 52 }, { LaneLeft, LaneFloor });           // lane / playfield divider
    wall({ LaneLeft, LaneFloor }, { LaneRight, LaneFloor }, 0.05); // plunger tip
    // One-way gate over the lane: launched balls pass up, nothing falls back in.
    m_walls.push_back({ { LaneLeft, 52 }, { LaneRight, 46 }, 0.3, 0, true });

    // Slingshots above the flippers; the face towards the middle kicks.
    const std::array<Vec2, 3> leftSling { Vec2 { 17, 102 }, Vec2 { 17, 118 }, Vec2 { 26, 127 } };
    const std::array<Vec2, 3> rightSling { Vec2 { 83, 102 }, Vec2 { 83, 118 }, Vec2 { 74, 127 } };
    for (const auto& s : { leftSling, rightSling }) {
        const int index = int(m_slingshots.size());
        m_slingshots.push_back(s);
        wall(s[0], s[1], 0.3);
        wall(s[1], s[2], 0.3);
        m_walls.push_back({ s[0], s[2], 0.5, SlingKick, false, index });
    }

    m_bumpers = { { { 37, 58 } }, { { 63, 58 } }, { { 50, 79 } } };
    m_rollovers = { { { 38, 30 } }, { { 50, 27 } }, { { 62, 30 } } };
    m_insideRollover.assign(m_rollovers.size(), false);
    m_cooldown.assign(m_bumpers.size() + m_slingshots.size(), 0);

    auto& left = m_flippers[0];
    left.pivot = leftPivot;
    left.restAngle = deg(28);
    left.upAngle = deg(-22);
    left.angle = left.restAngle;
    auto& right = m_flippers[1];
    right.pivot = rightPivot;
    right.restAngle = deg(180 - 28);
    right.upAngle = deg(180 + 22);
    right.angle = right.restAngle;

    m_restPosition = { (LaneLeft + LaneRight) / 2, LaneFloor - BallRadius };

    // Outline of the whole table, for drawing the felt.
    constexpr double Foot = LaneFloor + 6;
    m_outline = { { leftTop.x, Foot } };
    m_outline.insert(m_outline.end(), arch.begin(), arch.end());
    m_outline.push_back({ LaneRight, Foot });
    m_aprons = {
        { leftBottom, leftSlopeEnd, { leftSlopeEnd.x, Foot }, { leftTop.x, Foot } },
        { rightBottom, rightSlopeEnd, { rightSlopeEnd.x, Foot }, { LaneLeft, Foot } },
    };
}

void Table::step(double seconds)
{
    if (m_gameOver)
        return;
    m_accumulator += std::min(seconds, 0.1); // never try to catch up after a stall
    while (m_accumulator >= SubStep && !m_gameOver) {
        substep(SubStep);
        m_accumulator -= SubStep;
    }
}

void Table::setFlipper(Side side, bool pressed)
{
    m_flippers[int(side)].pressed = pressed && !m_gameOver;
}

void Table::setPlunger(bool held)
{
    if (held == m_plungerHeld)
        return;
    m_plungerHeld = held;
    if (!held && m_ready && !m_gameOver)
        launch(m_charge);
    if (!held)
        m_charge = 0;
}

std::vector<Event> Table::takeEvents()
{
    std::vector<Event> out;
    out.swap(m_events);
    return out;
}

void Table::launch(double strength)
{
    if (!m_ready || m_gameOver)
        return;
    strength = std::clamp(strength, 0.0, 1.0);
    // The weakest launch just reaches the arch; the strongest whips round it.
    const double minimum = std::sqrt(2 * m_gravity * 160);
    const double speed = minimum * (1.0 + 0.3 * strength);
    m_ready = false;
    m_charge = 0;
    m_ball.position = m_restPosition;
    m_ball.velocity = { 0, -speed };
    m_still = 0;
    record(EventType::Launch, 0, strength);
}

void Table::placeBall(Vec2 position, Vec2 velocity)
{
    m_ready = false;
    m_ball = { position, velocity };
    m_still = 0;
}

void Table::restore(long long score, int multiplier, int ballsLeft, int drained, unsigned litMask)
{
    m_score = std::max(0LL, score);
    m_multiplier = std::clamp(multiplier, 1, MaxMultiplier);
    m_ballsLeft = std::clamp(ballsLeft, 1, StartingBalls);
    m_drained = std::max(0, drained);
    for (std::size_t i = 0; i < m_rollovers.size(); ++i)
        m_rollovers[i].lit = (litMask >> i) & 1u;
    m_gameOver = false;
    returnToLane();
}

unsigned Table::litMask() const
{
    unsigned mask = 0;
    for (std::size_t i = 0; i < m_rollovers.size(); ++i)
        if (m_rollovers[i].lit)
            mask |= 1u << i;
    return mask;
}

void Table::substep(double dt)
{
    updateFlippers(dt);
    for (double& c : m_cooldown)
        c = std::max(0.0, c - dt);
    m_wallSoundCooldown = std::max(0.0, m_wallSoundCooldown - dt);

    if (m_ready) {
        if (m_plungerHeld)
            m_charge = std::min(1.0, m_charge + dt / ChargeSeconds);
        m_ball.position = m_restPosition + Vec2 { 0, 3.0 * m_charge };
        m_ball.velocity = {};
        return;
    }

    m_ball.velocity.y += m_gravity * dt;
    m_ball.velocity = m_ball.velocity * (1.0 - 0.02 * dt);
    const double speed = m_ball.velocity.length();
    if (speed > MaxSpeed)
        m_ball.velocity = m_ball.velocity * (MaxSpeed / speed);
    m_ball.position += m_ball.velocity * dt;

    collideWalls();
    collideBumpers();
    collideFlippers();
    checkSensors();
    if (m_ready)
        return;

    // Safety net: a ball that comes to rest anywhere other than a raised
    // flipper gets a gentle nudge rather than leaving the player stuck.
    const bool holding = m_flippers[0].pressed || m_flippers[1].pressed;
    if (m_ball.velocity.length() < 3 && !holding)
        m_still += dt;
    else
        m_still = 0;
    if (m_still > 2.5) {
        m_ball.velocity = { double(m_rng.below(81)) - 40.0, -70 };
        m_still = 0;
    }
}

void Table::updateFlippers(double dt)
{
    for (auto& f : m_flippers) {
        const double target = f.pressed ? f.upAngle : f.restAngle;
        const double diff = target - f.angle;
        const double speed = f.pressed ? FlipperUpSpeed : FlipperDownSpeed;
        const double move = speed * dt;
        if (std::abs(diff) <= move) {
            f.angle = target;
            f.omega = 0;
        } else {
            const double dir = diff > 0 ? 1.0 : -1.0;
            f.angle += dir * move;
            f.omega = dir * speed;
        }
    }
}

void Table::collideWalls()
{
    for (const auto& w : m_walls) {
        Vec2& p = m_ball.position;
        Vec2& v = m_ball.velocity;
        const Vec2 q = closestPoint(p, w.a, w.b);
        const Vec2 d = p - q;
        const double dist = d.length();
        if (dist >= BallRadius || dist <= 1e-9)
            continue;
        if (w.gate) {
            const Vec2 ab = w.b - w.a;
            const Vec2 open { ab.y, -ab.x };
            if ((p - w.a).dot(open) < 0)
                continue; // coming from below: let it through
        }
        const Vec2 n = d * (1.0 / dist);
        p = q + n * BallRadius;
        const double vn = v.dot(n);
        if (vn >= 0)
            continue;
        v -= n * ((1 + w.restitution) * vn);
        if (w.sling >= 0 && -vn > 12) {
            v += n * w.kick;
            const int key = int(m_bumpers.size()) + w.sling;
            if (m_cooldown[key] <= 0) {
                m_cooldown[key] = 0.12;
                addScore(SlingPoints);
                record(EventType::Slingshot, w.sling, -vn);
            }
        } else if (-vn > 130 && m_wallSoundCooldown <= 0) {
            m_wallSoundCooldown = 0.25;
            record(EventType::Wall, 0, -vn);
        }
    }
}

void Table::collideBumpers()
{
    for (std::size_t i = 0; i < m_bumpers.size(); ++i) {
        const auto& b = m_bumpers[i];
        Vec2& p = m_ball.position;
        Vec2& v = m_ball.velocity;
        const Vec2 d = p - b.centre;
        const double dist = d.length();
        const double reach = b.radius + BallRadius;
        if (dist >= reach || dist <= 1e-9)
            continue;
        const Vec2 n = d * (1.0 / dist);
        p = b.centre + n * reach;
        const double vn = v.dot(n);
        if (vn < 0)
            v -= n * ((1 + BumperRestitution) * vn);
        const double out = v.dot(n);
        if (out < BumperKick)
            v += n * (BumperKick - out);
        if (m_cooldown[i] <= 0) {
            m_cooldown[i] = 0.1;
            addScore(BumperPoints);
            record(EventType::Bumper, int(i), -vn);
        }
    }
}

void Table::collideFlippers()
{
    for (int side = 0; side < 2; ++side) {
        const auto& f = m_flippers[side];
        Vec2& p = m_ball.position;
        Vec2& v = m_ball.velocity;
        const Vec2 q = closestPoint(p, f.pivot, f.tip());
        const Vec2 d = p - q;
        const double dist = d.length();
        const double reach = f.radius + BallRadius;
        if (dist >= reach || dist <= 1e-9)
            continue;
        const Vec2 n = d * (1.0 / dist);
        p = q + n * reach;
        // Velocity of the flipper's surface where the ball touches it.
        const Vec2 r = q - f.pivot;
        const Vec2 surface { -f.omega * r.y, f.omega * r.x };
        Vec2 relative = v - surface;
        const double vn = relative.dot(n);
        if (vn >= 0)
            continue;
        relative -= n * ((1 + FlipperRestitution) * vn);
        v = relative + surface;
        if (surface.length() > 20)
            record(EventType::Flipper, side, -vn);
    }
}

void Table::checkSensors()
{
    const Vec2 p = m_ball.position;
    for (std::size_t i = 0; i < m_rollovers.size(); ++i) {
        auto& r = m_rollovers[i];
        const bool inside = (p - r.centre).length() < r.radius;
        if (inside && !m_insideRollover[i]) {
            addScore(RolloverPoints);
            if (!r.lit) {
                r.lit = true;
                record(EventType::Rollover, int(i));
                if (std::all_of(m_rollovers.begin(), m_rollovers.end(), [](const Rollover& x) { return x.lit; })) {
                    addScore(RolloverSetPoints);
                    m_multiplier = std::min(MaxMultiplier, m_multiplier + 1);
                    for (auto& x : m_rollovers)
                        x.lit = false;
                    record(EventType::RolloverSet, m_multiplier);
                }
            }
        }
        m_insideRollover[i] = inside;
    }

    // Fell back down the launch lane after a weak launch: wait on the plunger.
    if (p.x > LaneLeft && p.y > LaneFloor - BallRadius - 6 && m_ball.velocity.length() < 6) {
        returnToLane();
        return;
    }
    const bool escaped = p.x < -10 || p.x > TableWidth + 10 || p.y < -20;
    if (p.y > TableHeight + BallRadius || escaped)
        drain();
}

void Table::drain()
{
    ++m_drained;
    m_multiplier = 1;
    for (auto& r : m_rollovers)
        r.lit = false;
    record(EventType::Drain, m_drained);
    if (m_mode == Mode::Classic) {
        --m_ballsLeft;
        if (m_ballsLeft <= 0) {
            m_ballsLeft = 0;
            m_gameOver = true;
            m_ready = false;
            m_ball.velocity = {};
            for (auto& f : m_flippers)
                f.pressed = false;
            record(EventType::GameOver);
            return;
        }
    }
    returnToLane();
}

void Table::returnToLane()
{
    m_ready = true;
    m_charge = 0;
    m_ball = { m_restPosition, {} };
    m_still = 0;
    std::fill(m_insideRollover.begin(), m_insideRollover.end(), false);
}

void Table::addScore(long long points)
{
    m_score += points * m_multiplier;
}

void Table::record(EventType type, int index, double strength)
{
    m_events.push_back({ type, index, strength });
}

} // namespace pinball
