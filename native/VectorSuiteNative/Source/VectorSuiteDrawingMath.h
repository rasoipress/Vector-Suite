#ifndef VECTOR_SUITE_DRAWING_MATH_H
#define VECTOR_SUITE_DRAWING_MATH_H

#include <algorithm>
#include <cmath>
#include <vector>

constexpr double kVSDrawingPi = 3.14159265358979323846;

struct VSDrawingPoint {
	double x;
	double y;
};

struct VSPrecisionSettings {
	double angleStep;
	double fixedLength;
	double curveAmount;
	double strokeWidth;
};

struct VSFluidSettings {
	double smoothing;
	double strokeWidth;
	double sampleDistance;
	int closePath;
};

struct VSInkSettings {
	double smoothing;
	double nibWidth;
	double nibAspect;
	double nibAngle;
};

struct VSTextureSettings {
	double spacing;
	double angle;
	double strokeWidth;
	int crosshatch;
};

struct VSStippleSettings {
	double spacing;
	double radius;
	double variation;
	int maximumDots;
};

struct VSGeometrySettings {
	int mode;
	double strokeWidth;
};

struct VSShapeSettings {
	double radius;
	double strength;
	int preserveHandles;
};

inline VSPrecisionSettings VSPrecisionDefaults()
{
	return {15.0, 0.0, 0.0, 1.0};
}

inline VSFluidSettings VSFluidDefaults()
{
	return {0.55, 1.5, 1.5, 0};
}

inline VSInkSettings VSInkDefaults()
{
	return {0.45, 8.0, 0.35, 35.0};
}

inline VSTextureSettings VSTextureDefaults()
{
	return {10.0, 0.0, 0.7, 0};
}

inline VSStippleSettings VSStippleDefaults()
{
	return {10.0, 1.2, 0.35, 800};
}

inline VSGeometrySettings VSGeometryDefaults()
{
	return {0, 1.0};
}

inline VSShapeSettings VSShapeDefaults()
{
	return {48.0, 1.0, 1};
}

inline VSPrecisionSettings VSSanitizePrecision(VSPrecisionSettings value)
{
	value.angleStep = std::max(0.0, std::min(180.0, value.angleStep));
	value.fixedLength = std::max(0.0, std::min(100000.0, value.fixedLength));
	value.curveAmount = std::max(-1.0, std::min(1.0, value.curveAmount));
	value.strokeWidth = std::max(0.01, std::min(1000.0, value.strokeWidth));
	return value;
}

inline VSFluidSettings VSSanitizeFluid(VSFluidSettings value)
{
	value.smoothing = std::max(0.0, std::min(1.0, value.smoothing));
	value.strokeWidth = std::max(0.01, std::min(1000.0, value.strokeWidth));
	value.sampleDistance = std::max(0.1, std::min(100.0, value.sampleDistance));
	value.closePath = value.closePath ? 1 : 0;
	return value;
}

inline VSInkSettings VSSanitizeInk(VSInkSettings value)
{
	value.smoothing = std::max(0.0, std::min(1.0, value.smoothing));
	value.nibWidth = std::max(0.1, std::min(1000.0, value.nibWidth));
	value.nibAspect = std::max(0.02, std::min(1.0, value.nibAspect));
	value.nibAngle = std::fmod(value.nibAngle, 360.0);
	if (value.nibAngle < 0) value.nibAngle += 360.0;
	return value;
}

inline VSTextureSettings VSSanitizeTexture(VSTextureSettings value)
{
	value.spacing = std::max(0.5, std::min(1000.0, value.spacing));
	value.angle = std::fmod(value.angle, 180.0);
	if (value.angle < 0) value.angle += 180.0;
	value.strokeWidth = std::max(0.01, std::min(1000.0, value.strokeWidth));
	value.crosshatch = value.crosshatch ? 1 : 0;
	return value;
}

inline VSStippleSettings VSSanitizeStipple(VSStippleSettings value)
{
	value.spacing = std::max(1.0, std::min(1000.0, value.spacing));
	value.radius = std::max(0.05, std::min(500.0, value.radius));
	value.variation = std::max(0.0, std::min(1.0, value.variation));
	value.maximumDots = std::max(1, std::min(20000, value.maximumDots));
	return value;
}

inline VSGeometrySettings VSSanitizeGeometry(VSGeometrySettings value)
{
	value.mode = std::max(0, std::min(3, value.mode));
	value.strokeWidth = std::max(0.01, std::min(1000.0, value.strokeWidth));
	return value;
}

inline VSShapeSettings VSSanitizeShape(VSShapeSettings value)
{
	value.radius = std::max(0.1, std::min(10000.0, value.radius));
	value.strength = std::max(0.0, std::min(2.0, value.strength));
	value.preserveHandles = value.preserveHandles ? 1 : 0;
	return value;
}

inline double VSShapeFalloff(double distance, const VSShapeSettings& rawSettings)
{
	const VSShapeSettings settings = VSSanitizeShape(rawSettings);
	if (distance >= settings.radius) return 0;
	const double normalized = 1.0 - std::max(0.0, distance) / settings.radius;
	const double smooth = normalized * normalized * (3.0 - 2.0 * normalized);
	return smooth * settings.strength;
}

inline double VSDrawingDistance(const VSDrawingPoint& a, const VSDrawingPoint& b)
{
	const double dx = b.x - a.x;
	const double dy = b.y - a.y;
	return std::sqrt(dx * dx + dy * dy);
}

inline VSDrawingPoint VSPrecisionEndPoint(
	const VSDrawingPoint& start,
	const VSDrawingPoint& cursor,
	const VSPrecisionSettings& rawSettings)
{
	const VSPrecisionSettings settings = VSSanitizePrecision(rawSettings);
	const double dx = cursor.x - start.x;
	const double dy = cursor.y - start.y;
	double angle = std::atan2(dy, dx);
	if (settings.angleStep > 0) {
		const double step = settings.angleStep * kVSDrawingPi / 180.0;
		angle = std::round(angle / step) * step;
	}
	double length = std::sqrt(dx * dx + dy * dy);
	if (settings.fixedLength > 0) length = settings.fixedLength;
	return {start.x + std::cos(angle) * length, start.y + std::sin(angle) * length};
}

inline std::vector<VSDrawingPoint> VSSmoothDrawingPoints(
	const std::vector<VSDrawingPoint>& source,
	double rawSmoothing,
	bool closed = false)
{
	if (source.size() < 3) return source;
	const double smoothing = std::max(0.0, std::min(1.0, rawSmoothing));
	if (smoothing <= 0) return source;
	std::vector<VSDrawingPoint> result(source);
	const int passes = 1 + static_cast<int>(std::round(smoothing * 3.0));
	const double weight = 0.15 + smoothing * 0.35;
	for (int pass = 0; pass < passes; ++pass) {
		const std::vector<VSDrawingPoint> previous(result);
		for (size_t index = 0; index < result.size(); ++index) {
			if (!closed && (index == 0 || index + 1 == result.size())) continue;
			const size_t before = (index + result.size() - 1) % result.size();
			const size_t after = (index + 1) % result.size();
			const VSDrawingPoint average = {
				(previous[before].x + previous[after].x) * 0.5,
				(previous[before].y + previous[after].y) * 0.5
			};
			result[index].x = previous[index].x * (1.0 - weight) + average.x * weight;
			result[index].y = previous[index].y * (1.0 - weight) + average.y * weight;
		}
	}
	return result;
}

inline std::vector<VSDrawingPoint> VSCalligraphicOutline(
	const std::vector<VSDrawingPoint>& rawCenterline,
	const VSInkSettings& rawSettings)
{
	const VSInkSettings settings = VSSanitizeInk(rawSettings);
	const std::vector<VSDrawingPoint> centerline =
		VSSmoothDrawingPoints(rawCenterline, settings.smoothing, false);
	if (centerline.size() < 2) return {};
	std::vector<VSDrawingPoint> left;
	std::vector<VSDrawingPoint> right;
	left.reserve(centerline.size());
	right.reserve(centerline.size());
	const double nibAngle = settings.nibAngle * kVSDrawingPi / 180.0;
	const VSDrawingPoint major = {std::cos(nibAngle), std::sin(nibAngle)};
	const VSDrawingPoint minor = {-major.y, major.x};
	const double radiusMajor = settings.nibWidth * 0.5;
	const double radiusMinor = radiusMajor * settings.nibAspect;

	for (size_t index = 0; index < centerline.size(); ++index) {
		const VSDrawingPoint before = centerline[index == 0 ? 0 : index - 1];
		const VSDrawingPoint after = centerline[
			index + 1 < centerline.size() ? index + 1 : index];
		double tangentX = after.x - before.x;
		double tangentY = after.y - before.y;
		double tangentLength = std::sqrt(tangentX * tangentX + tangentY * tangentY);
		if (tangentLength < 1.0e-9) tangentLength = 1;
		const VSDrawingPoint normal = {-tangentY / tangentLength, tangentX / tangentLength};
		const double majorProjection = normal.x * major.x + normal.y * major.y;
		const double minorProjection = normal.x * minor.x + normal.y * minor.y;
		const double support = std::sqrt(
			radiusMajor * radiusMajor * majorProjection * majorProjection +
			radiusMinor * radiusMinor * minorProjection * minorProjection);
		left.push_back({
			centerline[index].x + normal.x * support,
			centerline[index].y + normal.y * support
		});
		right.push_back({
			centerline[index].x - normal.x * support,
			centerline[index].y - normal.y * support
		});
	}
	std::vector<VSDrawingPoint> outline;
	outline.reserve(left.size() + right.size());
	outline.insert(outline.end(), left.begin(), left.end());
	for (auto iterator = right.rbegin(); iterator != right.rend(); ++iterator) {
		outline.push_back(*iterator);
	}
	return outline;
}

#endif
