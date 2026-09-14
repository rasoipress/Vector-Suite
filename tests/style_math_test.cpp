#include "../native/VectorSuiteNative/Source/VectorSuiteStyleMath.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool Near(double a, double b, double tolerance = 1.0e-8)
{
	return std::abs(a - b) <= tolerance;
}

void TestNeutralColorAdjustment()
{
	const VSRGBColor source = {0.2, 0.5, 0.8};
	const VSRGBColor adjusted = VSAdjustRGB(source, VSColorDefaults());
	assert(Near(adjusted.red, source.red));
	assert(Near(adjusted.green, source.green));
	assert(Near(adjusted.blue, source.blue));
}

void TestHueSaturationAndLimits()
{
	VSColorSettings settings = VSColorDefaults();
	settings.hueDegrees = 120;
	VSRGBColor adjusted = VSAdjustRGB({1, 0, 0}, settings);
	assert(Near(adjusted.red, 0));
	assert(Near(adjusted.green, 1));
	assert(Near(adjusted.blue, 0));

	settings = VSColorDefaults();
	settings.saturation = -1;
	adjusted = VSAdjustRGB({1, 0, 0}, settings);
	assert(Near(adjusted.red, adjusted.green));
	assert(Near(adjusted.green, adjusted.blue));

	settings.brightness = 4;
	settings.contrast = -4;
	settings.hueDegrees = 725;
	settings = VSSanitizeColor(settings);
	assert(Near(settings.brightness, 1));
	assert(Near(settings.contrast, -1));
	assert(Near(settings.hueDegrees, 5));
}

void TestCMYKRoundTrip()
{
	const VSRGBColor rgb = VSCMYKToRGB(0.1, 0.4, 0.8, 0.2);
	double cyan = 0;
	double magenta = 0;
	double yellow = 0;
	double black = 0;
	VSRGBToCMYK(rgb, cyan, magenta, yellow, black);
	const VSRGBColor roundTrip = VSCMYKToRGB(cyan, magenta, yellow, black);
	assert(Near(rgb.red, roundTrip.red));
	assert(Near(rgb.green, roundTrip.green));
	assert(Near(rgb.blue, roundTrip.blue));
}

void TestStyleSanitization()
{
	VSWidthSettings width = {99, -2, 9, -4};
	width = VSSanitizeWidth(width);
	assert(width.mode == kVSWidthAbsolute);
	assert(Near(width.value, 0.01));
	assert(width.cap == 2);
	assert(width.join == 0);

	VSLiveStyleSettings live = {4, 99, 8};
	live = VSSanitizeLiveStyle(live);
	assert(Near(live.opacity, 1));
	assert(live.blendMode == 15);
	assert(live.isolated == 1);
}

} // namespace

int main()
{
	TestNeutralColorAdjustment();
	TestHueSaturationAndLimits();
	TestCMYKRoundTrip();
	TestStyleSanitization();
	std::cout << "Style math: width, opacity, blend and color adjustments verified.\n";
	return 0;
}
