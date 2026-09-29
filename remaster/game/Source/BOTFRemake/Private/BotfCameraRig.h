#pragma once
// Engine-independent tactical camera for the 3D battle scene (ABotfCombatScene).
//
// Ships are drawn at true scale, from a 60 m escort to a 1 km warbird, so no single fixed view works. The scene
// films the battle through one orbit rig: a focus point, a distance, an azimuth around the battle plane's normal and an
// elevation above the plane. Each camera mode proposes a target rig (frame every ship, follow one hull sized to its
// bounding sphere, a director shot); the player's orbit, zoom and pan adjust it; RigSmoother eases the shown rig
// toward the target, lifting the camera on long moves so a fly-to arcs out and back in rather than sliding through
// hulls. The same view projects hulls for the tactical markers and turns a click into a picked ship.
//
// Frame: the tactical frame, in scene units. x and y lie across the battle plane, z is the height above it. The
// scene maps it to its own axes (x -> X, y -> Z, z -> -Y). Unit-tested in Tools/client-logic-tests.cpp.

#include <algorithm>
#include <cmath>
#include <vector>

namespace botf::camera {

inline constexpr double kPi = 3.14159265358979323846;
inline double radians(double degrees) { return degrees * kPi / 180.0; }
inline double degrees(double radiansValue) { return radiansValue * 180.0 / kPi; }

struct Vec3 { double x = 0.0, y = 0.0, z = 0.0; };
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
inline double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline double length(Vec3 a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalized(Vec3 a) { const double l = length(a); return l > 1e-12 ? a * (1.0 / l) : Vec3{}; }

inline constexpr double kMinElevation = -80.0; // the camera may look up at the battle from below
inline constexpr double kMaxElevation = 88.0;  // straight down would lose the screen's up direction

// Signed difference b - a folded into (-180, 180].
inline double angleDelta(double a, double b) {
    double d = std::fmod(b - a, 360.0);
    if (d > 180.0) d -= 360.0;
    if (d <= -180.0) d += 360.0;
    return d;
}

struct Rig {
    Vec3 focus;
    double distance = 2000.0;
    double azimuth = -90.0;   // degrees around the plane normal: the camera sits at focus + (cos az, sin az) across the plane
    double elevation = 58.0;  // degrees above the plane
};

// A camera: position, unit axes (right = up x forward, as UE's MakeFromXZ builds it) and the lens. fovDegrees is the
// horizontal field of view (UE's FOVAngle); aspect is width / height of the rendered image.
struct View {
    Vec3 eye, forward{1, 0, 0}, right{0, 1, 0}, up{0, 0, 1};
    double fovDegrees = 55.0, aspect = 16.0 / 9.0;
};

inline View viewOf(const Rig& rig, double fovDegrees, double aspect) {
    const double el = radians(std::clamp(rig.elevation, kMinElevation, kMaxElevation));
    const double az = radians(rig.azimuth);
    const Vec3 offset{std::cos(el) * std::cos(az), std::cos(el) * std::sin(az), std::sin(el)};
    View v;
    v.eye = rig.focus + offset * std::max(1.0, rig.distance);
    v.forward = offset * -1.0;
    v.right = normalized(cross(Vec3{0, 0, 1}, v.forward));
    v.up = cross(v.forward, v.right);
    v.fovDegrees = fovDegrees;
    v.aspect = std::max(0.1, aspect);
    return v;
}

inline double halfTanH(const View& v) { return std::tan(radians(v.fovDegrees) * 0.5); }
inline double halfTanV(const View& v) { return halfTanH(v) / v.aspect; }

// Distance at which a sphere fills the narrower half of the frame, divided by margin (1.15: a little space around it).
inline double frameDistance(double radius, double fovDegrees, double aspect, double margin = 1.15) {
    const double halfH = radians(fovDegrees) * 0.5;
    const double halfV = std::atan(std::tan(halfH) / std::max(0.1, aspect));
    return std::max(1.0, radius) * margin / std::sin(std::min(halfH, halfV));
}

// Screen position: u across the image (0 left, 1 right), v down it (0 top, 1 bottom); radius is the sphere's projected
// radius as a fraction of the image height. Points behind the camera are not visible.
struct Projection { bool visible = false; double u = 0.0, v = 0.0, radius = 0.0, depth = 0.0; };
inline Projection project(const View& view, Vec3 point, double worldRadius = 0.0) {
    const Vec3 d = point - view.eye;
    const double depth = dot(d, view.forward);
    if (depth <= 1e-3) return {};
    Projection p;
    p.depth = depth;
    p.u = 0.5 + 0.5 * dot(d, view.right) / (depth * halfTanH(view));
    p.v = 0.5 - 0.5 * dot(d, view.up) / (depth * halfTanV(view));
    p.radius = worldRadius / (2.0 * depth * halfTanV(view));
    p.visible = p.u > -0.1 && p.u < 1.1 && p.v > -0.1 && p.v < 1.1;
    return p;
}

// Unit ray from the eye through a screen point (u, v as in Projection).
inline Vec3 rayThrough(const View& view, double u, double v) {
    const double sx = (2.0 * u - 1.0) * halfTanH(view);
    const double sy = (1.0 - 2.0 * v) * halfTanV(view);
    return normalized(view.forward + view.right * sx + view.up * sy);
}

// Where the ray through (u, v) meets the horizontal plane z = height; false when it runs parallel or away.
inline bool rayPlane(const View& view, double u, double v, double height, Vec3& out) {
    const Vec3 dir = rayThrough(view, u, v);
    if (std::abs(dir.z) < 1e-6) return false;
    const double t = (height - view.eye.z) / dir.z;
    if (t <= 0.0) return false;
    out = view.eye + dir * t;
    return true;
}

struct Body { int id = 0; Vec3 center; double radius = 0.0; };

// The ship under the cursor: the nearest hull whose sphere the ray enters; failing that, the hull whose projected
// outline lies within slack (a fraction of the image height, ~18 px at 1080p) of the cursor, so a speck-sized escort
// seen from across the battle can still be clicked. 0 = nothing.
inline int pick(const View& view, double u, double v, const std::vector<Body>& bodies, double slack = 0.017) {
    const Vec3 dir = rayThrough(view, u, v);
    int best = 0;
    double bestT = 1e300;
    for (const auto& b : bodies) {
        const Vec3 oc = view.eye - b.center;
        const double half = dot(oc, dir);
        const double c = dot(oc, oc) - b.radius * b.radius;
        const double disc = half * half - c;
        if (disc < 0.0) continue;
        const double t = -half - std::sqrt(disc);
        const double hit = t > 0.0 ? t : (-half + std::sqrt(disc) > 0.0 ? 0.0 : -1.0); // inside the sphere counts
        if (hit >= 0.0 && hit < bestT) { bestT = hit; best = b.id; }
    }
    if (best) return best;
    double bestGap = slack;
    for (const auto& b : bodies) {
        const Projection p = project(view, b.center, b.radius);
        if (p.depth <= 0.0) continue;
        const double du = (p.u - u) * view.aspect, dv = p.v - v; // in image-height units
        const double gap = std::sqrt(du * du + dv * dv) - p.radius;
        if (gap < bestGap) { bestGap = gap; best = b.id; }
    }
    return best;
}

// A sphere around every body: the centre of their bounding box and the farthest hull edge from it.
struct Sphere { Vec3 center; double radius = 0.0; };
inline Sphere boundsOf(const std::vector<Body>& bodies) {
    if (bodies.empty()) return {};
    Vec3 lo = bodies.front().center, hi = lo;
    for (const auto& b : bodies) {
        lo = {std::min(lo.x, b.center.x - b.radius), std::min(lo.y, b.center.y - b.radius), std::min(lo.z, b.center.z - b.radius)};
        hi = {std::max(hi.x, b.center.x + b.radius), std::max(hi.y, b.center.y + b.radius), std::max(hi.z, b.center.z + b.radius)};
    }
    Sphere s;
    s.center = (lo + hi) * 0.5;
    for (const auto& b : bodies) s.radius = std::max(s.radius, length(b.center - s.center) + b.radius);
    return s;
}

// Exponential easing toward a target: after halfLife seconds half the remaining gap is left.
inline double approach(double current, double target, double dt, double halfLife) {
    if (dt <= 0.0) return current;
    if (halfLife <= 0.0) return target;
    return target + (current - target) * std::exp2(-dt / halfLife);
}
inline double approachAngle(double current, double target, double dt, double halfLife) {
    const double gap = angleDelta(current, target);
    return current + gap - approach(gap, 0.0, dt, halfLife); // approach() leaves the part of the gap still to close
}

// Eases the shown rig toward the target rig. Distance eases in log space (zooming feels even from a hull close-up to
// the whole battle); angles take the short way round. While the focus still has far to travel the camera stands off
// by part of that gap, so a jump across the battle arcs out and back in.
struct RigSmoother {
    Rig current;
    bool primed = false;
    double travelLift = 0.35;
    // focusHalfLife < 0: the focus eases like everything else. A shorter one keeps a moving subject centred.
    Rig step(const Rig& target, double dt, double halfLife, double focusHalfLife = -1.0) {
        const double fh = focusHalfLife < 0.0 ? halfLife : focusHalfLife;
        if (!primed) { current = target; primed = true; }
        else {
            current.focus = {approach(current.focus.x, target.focus.x, dt, fh), approach(current.focus.y, target.focus.y, dt, fh),
                             approach(current.focus.z, target.focus.z, dt, fh)};
            current.distance = std::exp(approach(std::log(std::max(1.0, current.distance)), std::log(std::max(1.0, target.distance)), dt, halfLife));
            current.azimuth = approachAngle(current.azimuth, target.azimuth, dt, halfLife);
            current.elevation = approach(current.elevation, target.elevation, dt, halfLife);
        }
        Rig shown = current;
        shown.distance += travelLift * length(target.focus - current.focus);
        return shown;
    }
    void snap(const Rig& target) { current = target; primed = true; }
};

// Wheel zoom: each notch toward the screen (positive) brings the camera 15% closer.
inline double zoomFactorAfter(double factor, double notches) { return factor * std::pow(0.85, notches); }

} // namespace botf::camera
