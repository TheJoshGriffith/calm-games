#pragma once

#include <calm/Rng.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pinball {

struct Vec2 {
    double x = 0;
    double y = 0;

    Vec2 operator+(Vec2 o) const { return { x + o.x, y + o.y }; }
    Vec2 operator-(Vec2 o) const { return { x - o.x, y - o.y }; }
    Vec2 operator*(double s) const { return { x * s, y * s }; }
    Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
    double dot(Vec2 o) const { return x * o.x + y * o.y; }
    double length() const { return std::sqrt(x * x + y * y); }
};

// Table units: the playfield is 100 wide and 180 tall, y grows downwards.
inline constexpr double TableWidth = 100;
inline constexpr double TableHeight = 180;
inline constexpr double BallRadius = 2.4;

enum class Mode {
    Endless, // drained balls come back; play as long as you like
    Classic, // three balls, then the game ends
};

enum class Side { Left = 0, Right = 1 };

// A wall: a line segment the ball bounces off. A kicker pushes the ball
// back out (slingshots); a gate only stops balls coming from its `open` side.
struct Wall {
    Vec2 a;
    Vec2 b;
    double restitution = 0.45;
    double kick = 0;
    bool gate = false;
    int sling = -1; // index of the slingshot this face belongs to, or -1
    bool drawn = true; // false for guards hidden under the aprons
};

struct Bumper {
    Vec2 centre;
    double radius = 5.5;
};

struct Rollover {
    Vec2 centre;
    double radius = 3.5;
    bool lit = false;
};

struct Flipper {
    Vec2 pivot;
    double length = 17;
    double radius = 1.9;
    double restAngle = 0; // radians, y-down
    double upAngle = 0;
    double angle = 0;
    double omega = 0; // current angular speed, radians per second
    bool pressed = false;

    Vec2 tip() const { return pivot + Vec2 { std::cos(angle), std::sin(angle) } * length; }
};

struct Ball {
    Vec2 position;
    Vec2 velocity;
};

enum class EventType {
    Launch,
    Bumper,      // index = bumper
    Slingshot,   // index = slingshot
    Rollover,    // index = rollover lane
    RolloverSet, // all lanes lit; index = new multiplier
    Flipper,     // the ball was struck by a flipper; index = side
    Wall,        // a firm knock against a wall
    Drain,
    GameOver,
};

struct Event {
    EventType type;
    int index = 0;
    double strength = 0; // impact speed, for volume or haptics
};

// The scoring table and its physics. Pure C++ so it can be tested headless;
// the app steps it from a frame timer and draws the geometry it exposes.
class Table {
public:
    explicit Table(Mode mode = Mode::Endless, std::uint64_t seed = 1);

    // Advances the simulation by `seconds` of real time (fixed sub-steps).
    void step(double seconds);

    void setFlipper(Side side, bool pressed);
    // Hold to draw the plunger back, release to launch. Only does anything
    // while the ball waits in the launch lane.
    void setPlunger(bool held);

    // Events since the last call, oldest first.
    std::vector<Event> takeEvents();

    Mode mode() const { return m_mode; }
    const Ball& ball() const { return m_ball; }
    bool ballReady() const { return m_ready; }
    double plungerCharge() const { return m_charge; }
    bool gameOver() const { return m_gameOver; }

    long long score() const { return m_score; }
    int multiplier() const { return m_multiplier; }
    int ballsLeft() const { return m_ballsLeft; } // including the ball in play
    int drained() const { return m_drained; }

    const std::vector<Wall>& walls() const { return m_walls; }
    const std::vector<Bumper>& bumpers() const { return m_bumpers; }
    const std::vector<Rollover>& rollovers() const { return m_rollovers; }
    const std::array<Flipper, 2>& flippers() const { return m_flippers; }
    // Slingshot outlines (triangles), for drawing.
    const std::vector<std::array<Vec2, 3>>& slingshots() const { return m_slingshots; }
    Vec2 restPosition() const { return m_restPosition; }
    // Outer table outline as a closed polyline, and the filled aprons either
    // side of the flippers, for drawing.
    const std::vector<Vec2>& outline() const { return m_outline; }
    const std::vector<std::vector<Vec2>>& aprons() const { return m_aprons; }
    double laneLeft() const { return LaneLeft; }

    // Restores a saved game (score etc.); the ball returns to the launch lane.
    void restore(long long score, int multiplier, int ballsLeft, int drained, unsigned litMask);
    unsigned litMask() const;

    // Testing hooks.
    void placeBall(Vec2 position, Vec2 velocity);
    void launch(double strength); // 0..1, as if the plunger was released
    double gravity() const { return m_gravity; }
    static constexpr double SubStep = 1.0 / 480.0;
    static constexpr int StartingBalls = 3;

private:
    static constexpr double LaneLeft = 89;
    static constexpr double LaneRight = 96;

    void build();
    void substep(double dt);
    void updateFlippers(double dt);
    void collideWalls();
    void collideBumpers();
    void collideFlippers();
    void checkSensors();
    void drain();
    void returnToLane();
    void addScore(long long points);
    void record(EventType type, int index = 0, double strength = 0);

    Mode m_mode;
    calm::Rng m_rng;
    double m_gravity;

    std::vector<Wall> m_walls;
    std::vector<Bumper> m_bumpers;
    std::vector<Rollover> m_rollovers;
    std::vector<std::array<Vec2, 3>> m_slingshots;
    std::vector<Vec2> m_outline;
    std::vector<std::vector<Vec2>> m_aprons;
    std::array<Flipper, 2> m_flippers {};
    Vec2 m_restPosition;

    Ball m_ball;
    bool m_ready = true;
    bool m_plungerHeld = false;
    double m_charge = 0;
    double m_accumulator = 0;
    double m_still = 0; // seconds the ball has been nearly motionless
    std::vector<bool> m_insideRollover;
    std::vector<double> m_cooldown; // per bumper/slingshot, to debounce events
    double m_wallSoundCooldown = 0;

    long long m_score = 0;
    int m_multiplier = 1;
    int m_ballsLeft = StartingBalls;
    int m_drained = 0;
    bool m_gameOver = false;
    std::vector<Event> m_events;
};

} // namespace pinball
