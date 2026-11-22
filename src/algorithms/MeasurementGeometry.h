#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace MeasurementGeometry {
using Point = std::array<double, 3>;
inline double distance(const Point &a, const Point &b) {
    return std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
}
// A simple polygon on an axis-aligned physical slice. Reject crossings rather
// than presenting the algebraic area of a bow-tie as a clinical measurement.
inline double area(const std::vector<Point> &points, int normalAxis) {
    if (points.size() < 3 || normalAxis < 0 || normalAxis > 2)
        throw std::runtime_error("A polygon needs at least three points");
    const int u = (normalAxis + 1) % 3, v = (normalAxis + 2) % 3;
    auto cross = [u, v](const Point &a, const Point &b, const Point &c) {
        return (b[u] - a[u]) * (c[v] - a[v]) - (b[v] - a[v]) * (c[u] - a[u]);
    };
    constexpr double epsilon = 1e-8;
    for (size_t i = 0; i < points.size(); ++i) {
        const auto &a = points[i], &b = points[(i + 1) % points.size()];
        if (distance(a, b) < epsilon)
            throw std::runtime_error("Repeated polygon vertex");
        if (std::abs(a[normalAxis] - points.front()[normalAxis]) > 1e-5)
            throw std::runtime_error("Polygon points must be coplanar");
        for (size_t j = i + 1; j < points.size(); ++j) {
            if (j == i + 1 || (i == 0 && j == points.size() - 1))
                continue;
            const auto &c = points[j], &d = points[(j + 1) % points.size()];
            auto on = [u, v](const Point &p, const Point &q, const Point &r) {
                return r[u] >= std::min(p[u], q[u]) - epsilon &&
                       r[u] <= std::max(p[u], q[u]) + epsilon &&
                       r[v] >= std::min(p[v], q[v]) - epsilon &&
                       r[v] <= std::max(p[v], q[v]) + epsilon;
            };
            const double abC = cross(a, b, c), abD = cross(a, b, d);
            const double cdA = cross(c, d, a), cdB = cross(c, d, b);
            if ((abC * abD < 0 && cdA * cdB < 0) ||
                (std::abs(abC) < epsilon && on(a, b, c)) ||
                (std::abs(abD) < epsilon && on(a, b, d)) ||
                (std::abs(cdA) < epsilon && on(c, d, a)) ||
                (std::abs(cdB) < epsilon && on(c, d, b)))
                throw std::runtime_error("Polygon edges intersect");
        }
    }
    // Subtract a local origin to avoid cancellation with large world origins.
    double twiceArea = 0;
    for (size_t i = 1; i + 1 < points.size(); ++i)
        twiceArea += cross(points.front(), points[i], points[i + 1]);
    if (std::abs(twiceArea) < epsilon)
        throw std::runtime_error("Polygon area is zero");
    return std::abs(twiceArea) / 2;
}
} // namespace MeasurementGeometry
