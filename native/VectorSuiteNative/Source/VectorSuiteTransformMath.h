#ifndef VECTOR_SUITE_TRANSFORM_MATH_H
#define VECTOR_SUITE_TRANSFORM_MATH_H

#include <algorithm>
#include <cmath>
#include <cstdint>

enum VSRandomDistribution {
	kVSRandomUniform = 0,
	kVSRandomCentered = 1
};

enum VSMirrorMode {
	kVSMirrorVertical = 0,
	kVSMirrorHorizontal = 1,
	kVSMirrorBoth = 2,
	kVSMirrorRadial = 3
};

enum VSCollisionDirection {
	kVSCollisionRight = 0,
	kVSCollisionLeft = 1,
	kVSCollisionUp = 2,
	kVSCollisionDown = 3
};

struct VSRandomizeSettings {
	std::uint32_t seed;
	double positionX;
	double positionY;
	double rotation;
	double scaleMinimum;
	double scaleMaximum;
	int uniformScale;
	int distribution;
};

struct VSMirrorSettings {
	int mode;
	int copies;
	double axisOffset;
};

struct VSCollisionSettings {
	int direction;
	double gap;
	int alignCenters;
};

struct VSTransformSample {
	double translateX;
	double translateY;
	double rotation;
	double scaleX;
	double scaleY;
};

struct VSBounds {
	double left;
	double top;
	double right;
	double bottom;
};

inline VSRandomizeSettings VSRandomizeDefaults()
{
	return {999u, 24.0, 24.0, 15.0, 0.85, 1.15, 1, kVSRandomUniform};
}

inline VSMirrorSettings VSMirrorDefaults()
{
	return {kVSMirrorVertical, 2, 0.0};
}

inline VSCollisionSettings VSCollisionDefaults()
{
	return {kVSCollisionRight, 0.0, 1};
}

inline VSRandomizeSettings VSSanitizeRandomize(VSRandomizeSettings value)
{
	value.seed = std::max<std::uint32_t>(1u, value.seed);
	value.positionX = std::max(0.0, std::min(10000.0, value.positionX));
	value.positionY = std::max(0.0, std::min(10000.0, value.positionY));
	value.rotation = std::max(0.0, std::min(360.0, value.rotation));
	value.scaleMinimum = std::max(0.01, std::min(10.0, value.scaleMinimum));
	value.scaleMaximum = std::max(0.01, std::min(10.0, value.scaleMaximum));
	if (value.scaleMinimum > value.scaleMaximum) {
		std::swap(value.scaleMinimum, value.scaleMaximum);
	}
	value.uniformScale = value.uniformScale ? 1 : 0;
	value.distribution = value.distribution == kVSRandomCentered
		? kVSRandomCentered
		: kVSRandomUniform;
	return value;
}

inline VSMirrorSettings VSSanitizeMirror(VSMirrorSettings value)
{
	value.mode = std::max(
		static_cast<int>(kVSMirrorVertical),
		std::min(static_cast<int>(kVSMirrorRadial), value.mode));
	value.copies = std::max(2, std::min(32, value.copies));
	value.axisOffset = std::max(-10000.0, std::min(10000.0, value.axisOffset));
	return value;
}

inline VSCollisionSettings VSSanitizeCollision(VSCollisionSettings value)
{
	value.direction = std::max(
		static_cast<int>(kVSCollisionRight),
		std::min(static_cast<int>(kVSCollisionDown), value.direction));
	value.gap = std::max(-10000.0, std::min(10000.0, value.gap));
	value.alignCenters = value.alignCenters ? 1 : 0;
	return value;
}

class VSDeterministicRandom
{
public:
	explicit VSDeterministicRandom(std::uint32_t seed)
		: fState(seed ? seed : 1u)
	{
		for (int index = 0; index < 4; ++index) NextBits();
	}

	double Unit()
	{
		return static_cast<double>(NextBits()) / 4294967296.0;
	}

	double Bipolar() { return Unit() * 2.0 - 1.0; }

	// Averages three uniform values. The bounded bell-shaped result remains
	// deterministic and cannot produce the extreme outliers of a Gaussian.
	double Centered()
	{
		return (Bipolar() + Bipolar() + Bipolar()) / 3.0;
	}

private:
	std::uint32_t NextBits()
	{
		fState = fState * 1664525u + 1013904223u;
		return fState;
	}

	std::uint32_t fState;
};

inline VSTransformSample VSSampleTransform(
	const VSRandomizeSettings& rawSettings,
	std::uint32_t itemIndex)
{
	const VSRandomizeSettings settings = VSSanitizeRandomize(rawSettings);
	VSDeterministicRandom random(
		settings.seed ^ (0x9E3779B9u * (itemIndex + 1u)));
	auto randomSigned = [&]() {
		return settings.distribution == kVSRandomCentered
			? random.Centered()
			: random.Bipolar();
	};
	auto scaleValue = [&]() {
		return settings.scaleMinimum +
			(randomSigned() * 0.5 + 0.5) *
			(settings.scaleMaximum - settings.scaleMinimum);
	};

	VSTransformSample result = {
		randomSigned() * settings.positionX,
		randomSigned() * settings.positionY,
		randomSigned() * settings.rotation,
		scaleValue(),
		1.0
	};
	result.scaleY = settings.uniformScale ? result.scaleX : scaleValue();
	return result;
}

inline int VSMirrorGeneratedCopyCount(const VSMirrorSettings& rawSettings)
{
	const VSMirrorSettings settings = VSSanitizeMirror(rawSettings);
	if (settings.mode == kVSMirrorBoth) return 3;
	if (settings.mode == kVSMirrorRadial) return settings.copies - 1;
	return 1;
}

inline double VSMirrorRadialAngle(
	const VSMirrorSettings& rawSettings,
	int generatedCopyIndex)
{
	const VSMirrorSettings settings = VSSanitizeMirror(rawSettings);
	return 360.0 * static_cast<double>(generatedCopyIndex + 1) /
		static_cast<double>(settings.copies);
}

inline VSTransformSample VSCollisionPlacement(
	const VSBounds& anchor,
	const VSBounds& moving,
	const VSCollisionSettings& rawSettings)
{
	const VSCollisionSettings settings = VSSanitizeCollision(rawSettings);
	const double anchorCenterX = (anchor.left + anchor.right) * 0.5;
	const double anchorCenterY = (anchor.top + anchor.bottom) * 0.5;
	const double movingCenterX = (moving.left + moving.right) * 0.5;
	const double movingCenterY = (moving.top + moving.bottom) * 0.5;
	VSTransformSample result = {0, 0, 0, 1, 1};

	switch (settings.direction) {
		case kVSCollisionRight:
			result.translateX = anchor.right + settings.gap - moving.left;
			if (settings.alignCenters) result.translateY = anchorCenterY - movingCenterY;
			break;
		case kVSCollisionLeft:
			result.translateX = anchor.left - settings.gap - moving.right;
			if (settings.alignCenters) result.translateY = anchorCenterY - movingCenterY;
			break;
		case kVSCollisionUp:
			result.translateY = anchor.top + settings.gap - moving.bottom;
			if (settings.alignCenters) result.translateX = anchorCenterX - movingCenterX;
			break;
		case kVSCollisionDown:
			result.translateY = anchor.bottom - settings.gap - moving.top;
			if (settings.alignCenters) result.translateX = anchorCenterX - movingCenterX;
			break;
	}
	return result;
}

#endif
