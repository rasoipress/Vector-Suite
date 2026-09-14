#include "../native/VectorSuiteNative/Source/VectorSuiteDrawingMath.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool Near(double a, double b, double tolerance = 1.0e-8)
{
	return std::abs(a - b) <= tolerance;
}

void TestPrecision()
{
	VSPrecisionSettings settings = VSPrecisionDefaults();
	const VSDrawingPoint end = VSPrecisionEndPoint({0, 0}, {9, 4}, settings);
	const double angle = std::atan2(end.y, end.x) * 180.0 / kVSDrawingPi;
	assert(Near(angle, 30));
	settings.fixedLength = 20;
	const VSDrawingPoint fixed = VSPrecisionEndPoint({3, 2}, {12, 6}, settings);
	assert(Near(VSDrawingDistance({3, 2}, fixed), 20));
}

void TestSmoothing()
{
	const std::vector<VSDrawingPoint> source = {{0, 0}, {5, 10}, {10, 0}};
	const std::vector<VSDrawingPoint> smooth = VSSmoothDrawingPoints(source, 1);
	assert(Near(smooth.front().x, source.front().x));
	assert(Near(smooth.back().x, source.back().x));
	assert(smooth[1].y < source[1].y);
}

void TestCalligraphicOutline()
{
	VSInkSettings settings = VSInkDefaults();
	settings.nibWidth = 10;
	settings.nibAspect = 1;
	const std::vector<VSDrawingPoint> outline =
		VSCalligraphicOutline({{0, 0}, {20, 0}, {40, 0}}, settings);
	assert(outline.size() == 6);
	assert(Near(outline[0].y, 5));
	assert(Near(outline.back().y, -5));
}

void TestTextureAndStippleSettings()
{
	VSTextureSettings texture = {-1, -45, 0, 7};
	texture = VSSanitizeTexture(texture);
	assert(Near(texture.spacing, 0.5));
	assert(Near(texture.angle, 135));
	assert(Near(texture.strokeWidth, 0.01));
	assert(texture.crosshatch == 1);

	VSStippleSettings stipple = {0, 900, -2, 90000};
	stipple = VSSanitizeStipple(stipple);
	assert(Near(stipple.spacing, 1));
	assert(Near(stipple.radius, 500));
	assert(Near(stipple.variation, 0));
	assert(stipple.maximumDots == 20000);
}

void TestGeometryAndShapeSettings()
{
	VSGeometrySettings geometry = {9, 0};
	geometry = VSSanitizeGeometry(geometry);
	assert(geometry.mode == 3);
	assert(Near(geometry.strokeWidth, 0.01));
	VSShapeSettings shape = {100, 1.5, 1};
	assert(Near(VSShapeFalloff(0, shape), 1.5));
	assert(VSShapeFalloff(50, shape) > 0);
	assert(Near(VSShapeFalloff(100, shape), 0));
}

} // namespace

int main()
{
	TestPrecision();
	TestSmoothing();
	TestCalligraphicOutline();
	TestTextureAndStippleSettings();
	TestGeometryAndShapeSettings();
	std::cout << "Drawing math: constrained pen, smoothing and calligraphic outline verified.\n";
	return 0;
}
