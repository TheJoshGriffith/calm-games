// Dependency-free tests for the pinball table, run by CTest.

#include <pinball/Table.h>

#include <cstdio>
#include <functional>
#include <vector>

using namespace pinball;

namespace {

int g_failures = 0;

#define CHECK(expr)                                                                   \
    do {                                                                              \
        if (!(expr)) {                                                                \
            std::fprintf(stderr, "  FAILED %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            ++g_failures;                                                             \
        }                                                                             \
    } while (0)

constexpr double Frame = 1.0 / 60.0;

int count(const std::vector<Event>& events, EventType type)
{
    int n = 0;
    for (const auto& e : events)
        n += e.type == type;
    return n;
}

void ballStartsInLane()
{
    Table t(Mode::Classic);
    CHECK(t.ballReady());
    CHECK(t.ballsLeft() == 3);
    CHECK(t.score() == 0);
    CHECK(t.ball().position.x > t.laneLeft());
    // Waiting balls stay put however long the player waits.
    for (int i = 0; i < 300; ++i)
        t.step(Frame);
    CHECK(t.ballReady());
    CHECK(t.takeEvents().empty());
}

void everyLaunchReachesThePlayfield()
{
    for (double strength : { 0.0, 0.25, 0.5, 0.75, 1.0 }) {
        Table t(Mode::Endless);
        t.launch(strength);
        CHECK(!t.ballReady());
        bool entered = false;
        for (int i = 0; i < 180 && !entered; ++i) {
            t.step(Frame);
            entered = t.ball().position.x < t.laneLeft() - BallRadius;
        }
        CHECK(entered);
    }
}

void plungerChargesAndLaunches()
{
    Table t(Mode::Endless);
    t.setPlunger(true);
    for (int i = 0; i < 30; ++i)
        t.step(Frame);
    CHECK(t.plungerCharge() > 0.3);
    CHECK(t.ballReady());
    t.setPlunger(false);
    CHECK(!t.ballReady());
    const auto events = t.takeEvents();
    CHECK(count(events, EventType::Launch) == 1);
    CHECK(t.ball().velocity.y < 0);
}

void gateKeepsBallsOutOfTheLane()
{
    // A ball dropped onto the gate from above rolls back into play.
    Table t(Mode::Endless);
    t.placeBall({ 92.5, 38 }, { 0, 0 });
    for (int i = 0; i < 120; ++i)
        t.step(Frame);
    CHECK(!t.ballReady());
    CHECK(t.ball().position.x < t.laneLeft());
}

void unattendedBallDrains()
{
    Table t(Mode::Classic, 3);
    t.launch(0.5);
    std::vector<Event> events;
    for (int i = 0; i < 60 * 120 && t.drained() == 0; ++i) {
        if (t.ballReady())
            t.launch(0.5); // again, if the ball save sent it back
        t.step(Frame);
        for (const auto& e : t.takeEvents())
            events.push_back(e);
    }
    CHECK(t.drained() == 1);
    CHECK(count(events, EventType::Drain) == 1);
    CHECK(t.ballsLeft() == 2);
    CHECK(t.ballReady());
}

void flipperSendsTheBallUpTheTable()
{
    Table t(Mode::Endless);
    t.placeBall({ 40, 145 }, { 0, 0 });
    for (int i = 0; i < 12; ++i)
        t.step(Frame);
    t.setFlipper(Side::Left, true);
    double fastestUp = 0;
    bool struck = false;
    for (int i = 0; i < 120; ++i) {
        t.step(Frame);
        fastestUp = std::min(fastestUp, t.ball().velocity.y);
        struck = struck || count(t.takeEvents(), EventType::Flipper) > 0;
    }
    CHECK(struck);
    CHECK(fastestUp < -200); // a real shot, up into the drop targets
}

void raisedFlipperCradlesTheBall()
{
    Table t(Mode::Endless);
    t.setFlipper(Side::Right, true);
    t.step(0.2);
    t.placeBall({ 62, 120 }, { 0, 0 });
    for (int i = 0; i < 60 * 8; ++i)
        t.step(Frame);
    CHECK(t.drained() == 0);
    CHECK(t.ball().position.y < 150);
    CHECK(t.ball().velocity.length() < 5);

    // Letting go rolls it off the flipper and away.
    t.setFlipper(Side::Right, false);
    for (int i = 0; i < 60 * 5 && t.drained() == 0; ++i)
        t.step(Frame);
    CHECK(t.drained() == 1);
}

void bumperScoresAndKicks()
{
    Table t(Mode::Endless);
    const Bumper b = t.bumpers()[0];
    t.placeBall({ b.centre.x, b.centre.y - b.radius - 6 }, { 0, 40 });
    std::vector<Event> events;
    for (int i = 0; i < 10; ++i) {
        t.step(Frame);
        for (const auto& e : t.takeEvents())
            events.push_back(e);
    }
    CHECK(count(events, EventType::Bumper) == 1);
    CHECK(t.score() == 100);
    CHECK(t.ball().velocity.y < -50); // kicked back up
}

void rolloversLightAndRaiseTheMultiplier()
{
    Table t(Mode::Endless);
    std::vector<Event> events;
    for (const auto& r : std::vector<Rollover>(t.rollovers())) {
        t.placeBall(r.centre + Vec2 { 0, -6 }, { 0, 60 });
        for (int i = 0; i < 8; ++i) {
            t.step(Frame);
            for (const auto& e : t.takeEvents())
                events.push_back(e);
        }
    }
    CHECK(count(events, EventType::Rollover) == 3);
    CHECK(count(events, EventType::RolloverSet) == 1);
    CHECK(t.multiplier() == 2);
    CHECK(t.litMask() == 0); // lights reset for the next set
    CHECK(t.score() >= 3 * 250 + 1000);
}

void classicEndsAfterThreeBalls()
{
    Table t(Mode::Classic);
    std::vector<Event> events;
    for (int ball = 0; ball < 3; ++ball) {
        t.placeBall({ 50, 170 }, { 0, 50 });
        for (int i = 0; i < 30; ++i) {
            t.step(Frame);
            for (const auto& e : t.takeEvents())
                events.push_back(e);
        }
    }
    CHECK(count(events, EventType::Drain) == 3);
    CHECK(count(events, EventType::GameOver) == 1);
    CHECK(t.gameOver());
    CHECK(t.ballsLeft() == 0);
    t.launch(1);
    CHECK(!t.ballReady());
    CHECK(t.takeEvents().empty());
}

void endlessNeverEnds()
{
    Table t(Mode::Endless);
    for (int ball = 0; ball < 5; ++ball) {
        t.placeBall({ 50, 170 }, { 0, 50 });
        for (int i = 0; i < 30; ++i)
            t.step(Frame);
    }
    CHECK(t.drained() == 5);
    CHECK(!t.gameOver());
    CHECK(t.ballReady());
}

void restoreRoundTrips()
{
    Table t(Mode::Classic);
    Progress saved;
    saved.score = 12345;
    saved.multiplier = 3;
    saved.ballsLeft = 2;
    saved.drained = 1;
    saved.lit = 0b101;
    saved.stage = 3;
    saved.stageStars = 2;
    saved.stageBank = true;
    t.restore(saved);
    const Progress p = t.progress();
    CHECK(p.score == 12345);
    CHECK(p.multiplier == 3);
    CHECK(p.ballsLeft == 2);
    CHECK(p.drained == 1);
    CHECK(p.lit == 0b101);
    CHECK(p.stage == 3);
    CHECK(p.stageStars == 2);
    CHECK(p.stageBank);
    CHECK(t.ballReady());

    Progress bad;
    bad.score = -5;
    bad.multiplier = 99;
    bad.ballsLeft = 99;
    bad.stage = -2;
    bad.stageStars = 99;
    t.restore(bad);
    CHECK(t.score() == 0);
    CHECK(t.multiplier() == 5);
    CHECK(t.ballsLeft() == Table::MaxBalls);
    CHECK(t.stage() == 1);
    CHECK(t.stageStars() == Table::goalFor(1).stars);
}

void starSpotsAreOpenFelt()
{
    Table t(Mode::Endless);
    CHECK(t.starSpots().size() > 40);
    for (const Vec2 p : t.starSpots()) {
        CHECK(p.x > 11 && p.x < t.laneLeft());
        for (const auto& b : t.bumpers())
            CHECK((p - b.centre).length() > b.radius + 5);
    }
}

void starsAppearAndCanBeCollected()
{
    Table t(Mode::Endless, 5);
    t.launch(0.5);
    std::vector<Event> events;
    for (int i = 0; i < 60 * 4; ++i) {
        t.step(Frame);
        for (const auto& e : t.takeEvents())
            events.push_back(e);
    }
    CHECK(count(events, EventType::StarAppeared) >= 1);

    // Roll the ball through an active star.
    int slot = -1;
    for (int s = 0; s < Table::StarSlots; ++s)
        if (t.stars()[s].active)
            slot = s;
    CHECK(slot >= 0);
    if (slot < 0)
        return;
    const long long before = t.score();
    t.placeBall(t.stars()[slot].centre + Vec2 { 0, -5 }, { 0, 40 });
    events.clear();
    for (int i = 0; i < 10; ++i) {
        t.step(Frame);
        for (const auto& e : t.takeEvents())
            events.push_back(e);
    }
    CHECK(count(events, EventType::Star) == 1);
    CHECK(!t.stars()[slot].active);
    CHECK(t.stageStars() == 1);
    CHECK(t.score() >= before + 500);
}

void unreachedStarsFade()
{
    Table t(Mode::Endless, 2);
    t.setFlipper(Side::Left, true);
    t.step(0.2);
    t.placeBall({ 40, 120 }, { 0, 0 }); // cradled, far from any star
    t.spawnStar(0);
    CHECK(t.stars()[0].active);
    const double life = t.stars()[0].life;
    for (int i = 0; i < int(60 * (life + 1)); ++i)
        t.step(Frame);
    // The first star faded; others may have appeared since, but not in its place
    // without a fresh spawn resetting its age.
    CHECK(!t.stars()[0].active || t.stars()[0].age < life);
}

void dropBankClearsAndResets()
{
    Table t(Mode::Endless);
    std::vector<Event> events;
    for (const auto& d : std::vector<DropTarget>(t.dropTargets())) {
        const Vec2 mid = (d.a + d.b) * 0.5;
        t.placeBall(mid + Vec2 { 0, 6 }, { 0, -80 });
        for (int i = 0; i < 6; ++i) {
            t.step(Frame);
            for (const auto& e : t.takeEvents())
                events.push_back(e);
        }
    }
    CHECK(count(events, EventType::DropTarget) == 3);
    CHECK(count(events, EventType::BankCleared) == 1);
    CHECK(t.stageBank());
    for (const auto& d : t.dropTargets())
        CHECK(d.down);
    t.placeBall({ 20, 60 }, { 0, 0 });
    for (int i = 0; i < 120; ++i)
        t.step(Frame);
    for (const auto& d : t.dropTargets())
        CHECK(!d.down);
}

void standingTargetsBlockTheBall()
{
    Table t(Mode::Endless);
    const auto d = t.dropTargets()[1];
    t.placeBall((d.a + d.b) * 0.5 + Vec2 { 0, 8 }, { 0, -100 });
    t.step(Frame * 3);
    CHECK(t.ball().velocity.y > -100); // knocked back, not passed through
    CHECK(t.ball().position.y > d.a.y);
}

void stagesNeedStarsThenTheBank()
{
    CHECK(Table::goalFor(1).stars == 3 && !Table::goalFor(1).bank);
    CHECK(Table::goalFor(2).stars == 4 && Table::goalFor(2).bank);
    CHECK(Table::goalFor(20).stars == 8);

    Table t(Mode::Classic);
    Progress p;
    p.stageStars = 2;
    t.restore(p);
    t.launch(0);
    t.spawnStar(0);
    int slot = -1;
    for (int s = 0; s < Table::StarSlots; ++s)
        if (t.stars()[s].active)
            slot = s;
    CHECK(slot >= 0);
    if (slot < 0)
        return;
    t.placeBall(t.stars()[slot].centre + Vec2 { 0, -5 }, { 0, 40 });
    std::vector<Event> events;
    for (int i = 0; i < 10; ++i) {
        t.step(Frame);
        for (const auto& e : t.takeEvents())
            events.push_back(e);
    }
    CHECK(count(events, EventType::StageComplete) == 1);
    CHECK(count(events, EventType::ExtraBall) == 1);
    CHECK(t.stage() == 2);
    CHECK(t.stageStars() == 0);
    CHECK(t.ballsLeft() == 4);
    CHECK(t.score() >= 2500);
}

void classicSavesAnEarlyDrain()
{
    Table t(Mode::Classic);
    t.launch(0.5);
    t.step(0.5);
    CHECK(t.ballSave() > 0);
    t.placeBall({ 50, 175 }, { 0, 60 });
    std::vector<Event> events;
    for (int i = 0; i < 20; ++i) {
        t.step(Frame);
        for (const auto& e : t.takeEvents())
            events.push_back(e);
    }
    CHECK(count(events, EventType::BallSaved) == 1);
    CHECK(count(events, EventType::Drain) == 0);
    CHECK(t.ballsLeft() == 3);
    CHECK(t.ballReady());
}

// Long random sessions: the ball never leaves the table or sticks anywhere.
void randomPlayStaysOnTheTable()
{
    int drained = 0;
    for (std::uint64_t seed = 1; seed <= 40; ++seed) {
        Table t(Mode::Endless, seed);
        calm::Rng rng(seed * 7919);
        bool outside = false;
        double stuck = 0, longestStuck = 0;
        for (int i = 0; i < 60 * 60; ++i) {
            if (t.ballReady())
                t.launch(rng.below(101) / 100.0);
            if (i % 8 == 0) {
                t.setFlipper(Side::Left, rng.below(3) == 0);
                t.setFlipper(Side::Right, rng.below(3) == 0);
            }
            t.step(Frame);
            t.takeEvents();
            const Vec2 p = t.ball().position;
            outside = outside || p.x < 11 || p.x > 96 || p.y < 4;
            const bool holding = t.flippers()[0].pressed || t.flippers()[1].pressed;
            stuck = (!t.ballReady() && !holding && t.ball().velocity.length() < 3) ? stuck + Frame : 0;
            longestStuck = std::max(longestStuck, stuck);
        }
        CHECK(!outside);
        CHECK(longestStuck < 3.0);
        drained += t.drained();
    }
    CHECK(drained > 40); // random flipping is no way to keep a ball alive
}

void sameSeedSameGame()
{
    auto run = [](std::uint64_t seed) {
        Table t(Mode::Endless, seed);
        t.launch(0.6);
        for (int i = 0; i < 60 * 20; ++i) {
            t.setFlipper(Side::Left, (i / 20) % 3 == 0);
            t.setFlipper(Side::Right, (i / 25) % 3 == 0);
            if (t.ballReady())
                t.launch(0.3);
            t.step(Frame);
        }
        return std::make_pair(t.score(), t.ball().position.x);
    };
    CHECK(run(9) == run(9));
}

} // namespace

int main()
{
    const std::vector<std::pair<const char*, std::function<void()>>> tests {
        { "ballStartsInLane", ballStartsInLane },
        { "everyLaunchReachesThePlayfield", everyLaunchReachesThePlayfield },
        { "plungerChargesAndLaunches", plungerChargesAndLaunches },
        { "gateKeepsBallsOutOfTheLane", gateKeepsBallsOutOfTheLane },
        { "unattendedBallDrains", unattendedBallDrains },
        { "flipperSendsTheBallUpTheTable", flipperSendsTheBallUpTheTable },
        { "raisedFlipperCradlesTheBall", raisedFlipperCradlesTheBall },
        { "bumperScoresAndKicks", bumperScoresAndKicks },
        { "rolloversLightAndRaiseTheMultiplier", rolloversLightAndRaiseTheMultiplier },
        { "classicEndsAfterThreeBalls", classicEndsAfterThreeBalls },
        { "endlessNeverEnds", endlessNeverEnds },
        { "restoreRoundTrips", restoreRoundTrips },
        { "starSpotsAreOpenFelt", starSpotsAreOpenFelt },
        { "starsAppearAndCanBeCollected", starsAppearAndCanBeCollected },
        { "unreachedStarsFade", unreachedStarsFade },
        { "dropBankClearsAndResets", dropBankClearsAndResets },
        { "standingTargetsBlockTheBall", standingTargetsBlockTheBall },
        { "stagesNeedStarsThenTheBank", stagesNeedStarsThenTheBank },
        { "classicSavesAnEarlyDrain", classicSavesAnEarlyDrain },
        { "randomPlayStaysOnTheTable", randomPlayStaysOnTheTable },
        { "sameSeedSameGame", sameSeedSameGame },
    };
    for (const auto& [name, test] : tests) {
        const int before = g_failures;
        test();
        std::printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", name);
    }
    return g_failures == 0 ? 0 : 1;
}
