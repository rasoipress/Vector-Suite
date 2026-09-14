#ifndef VECTOR_SUITE_STYLE_MATH_H
#define VECTOR_SUITE_STYLE_MATH_H

#include <algorithm>
#include <cmath>

enum VSWidthMode {
	kVSWidthAbsolute = 0,
	kVSWidthMultiply = 1
};

struct VSWidthSettings {
	int mode;
	double value;
	int cap;
	int join;
};

struct VSLiveStyleSettings {
	double opacity;
	int blendMode;
	int isolated;
};

struct VSColorSettings {
	double brightness;
	double contrast;
	double saturation;
	double hueDegrees;
};

struct VSRGBColor {
	double red;
	double green;
	double blue;
};

inline double VSUnit(double value)
{
	return std::max(0.0, std::min(1.0, value));
}

inline VSWidthSettings VSWidthDefaults()
{
	return {kVSWidthAbsolute, 2.0, 1, 1};
}

inline VSLiveStyleSettings VSLiveStyleDefaults()
{
	return {1.0, 0, 0};
}

inline VSColorSettings VSColorDefaults()
{
	return {0.0, 0.0, 0.0, 0.0};
}

inline VSWidthSettings VSSanitizeWidth(VSWidthSettings value)
{
	value.mode = value.mode == kVSWidthMultiply ? kVSWidthMultiply : kVSWidthAbsolute;
	value.value = value.mode == kVSWidthMultiply
		? std::max(0.01, std::min(100.0, value.value))
		: std::max(0.01, std::min(1000.0, value.value));
	value.cap = std::max(0, std::min(2, value.cap));
	value.join = std::max(0, std::min(2, value.join));
	return value;
}

inline VSLiveStyleSettings VSSanitizeLiveStyle(VSLiveStyleSettings value)
{
	value.opacity = VSUnit(value.opacity);
	value.blendMode = std::max(0, std::min(15, value.blendMode));
	value.isolated = value.isolated ? 1 : 0;
	return value;
}

inline VSColorSettings VSSanitizeColor(VSColorSettings value)
{
	value.brightness = std::max(-1.0, std::min(1.0, value.brightness));
	value.contrast = std::max(-1.0, std::min(1.0, value.contrast));
	value.saturation = std::max(-1.0, std::min(1.0, value.saturation));
	value.hueDegrees = std::fmod(value.hueDegrees, 360.0);
	if (value.hueDegrees > 180.0) value.hueDegrees -= 360.0;
	if (value.hueDegrees < -180.0) value.hueDegrees += 360.0;
	return value;
}

inline double VSHueToRGB(double p, double q, double hue)
{
	if (hue < 0) hue += 1;
	if (hue > 1) hue -= 1;
	if (hue < 1.0 / 6.0) return p + (q - p) * 6 * hue;
	if (hue < 1.0 / 2.0) return q;
	if (hue < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - hue) * 6;
	return p;
}

inline VSRGBColor VSAdjustRGB(
	VSRGBColor color,
	const VSColorSettings& rawSettings)
{
	const VSColorSettings settings = VSSanitizeColor(rawSettings);
	color.red = VSUnit(color.red);
	color.green = VSUnit(color.green);
	color.blue = VSUnit(color.blue);
	const double maximum = std::max(color.red, std::max(color.green, color.blue));
	const double minimum = std::min(color.red, std::min(color.green, color.blue));
	double hue = 0;
	const double lightness = (maximum + minimum) * 0.5;
	double saturation = 0;
	if (maximum != minimum) {
		const double delta = maximum - minimum;
		saturation = lightness > 0.5
			? delta / (2.0 - maximum - minimum)
			: delta / (maximum + minimum);
		if (maximum == color.red) {
			hue = (color.green - color.blue) / delta +
				(color.green < color.blue ? 6.0 : 0.0);
		}
		else if (maximum == color.green) {
			hue = (color.blue - color.red) / delta + 2.0;
		}
		else {
			hue = (color.red - color.green) / delta + 4.0;
		}
		hue /= 6.0;
	}

	hue = std::fmod(hue + settings.hueDegrees / 360.0 + 1.0, 1.0);
	saturation = settings.saturation >= 0
		? saturation + (1.0 - saturation) * settings.saturation
		: saturation * (1.0 + settings.saturation);

	VSRGBColor adjusted = {lightness, lightness, lightness};
	if (saturation > 0) {
		const double q = lightness < 0.5
			? lightness * (1.0 + saturation)
			: lightness + saturation - lightness * saturation;
		const double p = 2.0 * lightness - q;
		adjusted.red = VSHueToRGB(p, q, hue + 1.0 / 3.0);
		adjusted.green = VSHueToRGB(p, q, hue);
		adjusted.blue = VSHueToRGB(p, q, hue - 1.0 / 3.0);
	}

	auto finish = [&](double channel) {
		channel = (channel - 0.5) * (1.0 + settings.contrast) + 0.5;
		return VSUnit(channel + settings.brightness);
	};
	adjusted.red = finish(adjusted.red);
	adjusted.green = finish(adjusted.green);
	adjusted.blue = finish(adjusted.blue);
	return adjusted;
}

inline VSRGBColor VSCMYKToRGB(double cyan, double magenta, double yellow, double black)
{
	return {
		(1.0 - VSUnit(cyan)) * (1.0 - VSUnit(black)),
		(1.0 - VSUnit(magenta)) * (1.0 - VSUnit(black)),
		(1.0 - VSUnit(yellow)) * (1.0 - VSUnit(black))
	};
}

inline void VSRGBToCMYK(
	const VSRGBColor& color,
	double& cyan,
	double& magenta,
	double& yellow,
	double& black)
{
	black = 1.0 - std::max(color.red, std::max(color.green, color.blue));
	if (black >= 1.0 - 1.0e-12) {
		cyan = magenta = yellow = 0;
		black = 1;
		return;
	}
	cyan = VSUnit((1.0 - color.red - black) / (1.0 - black));
	magenta = VSUnit((1.0 - color.green - black) / (1.0 - black));
	yellow = VSUnit((1.0 - color.blue - black) / (1.0 - black));
	black = VSUnit(black);
}

#endif
