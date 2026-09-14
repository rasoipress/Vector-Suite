#include "../native/VectorSuiteNative/Source/VectorSuiteTransformMath.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool Near(double a, double b, double tolerance = 1.0e-9)
{
	return std::abs(a - b) <= tolerance;
}

void TestDeterministicRandomize()
{
	VSRandomizeSettings settings = VSRandomizeDefaults();
	settings.seed = 42;
	settings.positionX = 20;
	settings.positionY = 12;
	settings.rotation = 30;
	settings.scaleMinimum = 0.8;
	settings.scaleMaximum = 1.25;

	const VSTransformSample first = VSSampleTransform(settings, 3);
	const VSTransformSample repeated = VSSampleTransform(settings, 3);
	assert(Near(first.translateX, repeated.translateX));
	assert(Near(first.translateY, repeated.translateY));
	assert(Near(first.rotation, repeated.rotation));
	assert(Near(first.scaleX, repeated.scaleX));
	assert(Near(first.scaleX, first.scaleY));
	assert(std::abs(first.translateX) <= settings.positionX);
	assert(std::abs(first.translateY) <= settings.positionY);
	assert(std::abs(first.rotation) <= settings.rotation);
	assert(first.scaleX >= settings.scaleMinimum);
	assert(first.scaleX <= settings.scaleMaximum);

	settings.uniformScale = 0;
	const VSTransformSample independent = VSSampleTransform(settings, 3);
	assert(!Near(independent.scaleX, independent.scaleY));
	assert(!Near(first.translateX, VSSampleTransform(settings, 4).translateX));
}

void TestSanitization()
{
	VSRandomizeSettings randomize = {0, -4, 12000, 900, 4, 0.2, 8, 99};
	randomize = VSSanitizeRandomize(randomize);
	assert(randomize.seed == 1);
	assert(Near(randomize.positionX, 0));
	assert(Near(randomize.positionY, 10000));
	assert(Near(randomize.rotation, 360));
	assert(Near(randomize.scaleMinimum, 0.2));
	assert(Near(randomize.scaleMaximum, 4));
	assert(randomize.uniformScale == 1);
	assert(randomize.distribution == kVSRandomUniform);

	VSMirrorSettings mirror = {99, 100, 20000};
	mirror = VSSanitizeMirror(mirror);
	assert(mirror.mode == kVSMirrorRadial);
	assert(mirror.copies == 32);
	assert(Near(mirror.axisOffset, 10000));
}

void TestMirrorSeries()
{
	VSMirrorSettings mirror = VSMirrorDefaults();
	assert(VSMirrorGeneratedCopyCount(mirror) == 1);
	mirror.mode = kVSMirrorBoth;
	assert(VSMirrorGeneratedCopyCount(mirror) == 3);
	mirror.mode = kVSMirrorRadial;
	mirror.copies = 6;
	assert(VSMirrorGeneratedCopyCount(mirror) == 5);
	assert(Near(VSMirrorRadialAngle(mirror, 0), 60));
	assert(Near(VSMirrorRadialAngle(mirror, 4), 300));
}

void TestCollisionPlacement()
{
	const VSBounds anchor = {0, 20, 10, 0};
	const VSBounds moving = {40, 15, 50, 5};
	VSCollisionSettings settings = VSCollisionDefaults();
	settings.gap = 3;

	VSTransformSample sample = VSCollisionPlacement(anchor, moving, settings);
	assert(Near(sample.translateX, -27));
	assert(Near(sample.translateY, 0));

	settings.direction = kVSCollisionLeft;
	sample = VSCollisionPlacement(anchor, moving, settings);
	assert(Near(sample.translateX, -53));

	settings.direction = kVSCollisionUp;
	sample = VSCollisionPlacement(anchor, moving, settings);
	assert(Near(sample.translateX, -40));
	assert(Near(sample.translateY, 18));

	settings.direction = kVSCollisionDown;
	sample = VSCollisionPlacement(anchor, moving, settings);
	assert(Near(sample.translateY, -18));

	settings.alignCenters = 0;
	sample = VSCollisionPlacement(anchor, moving, settings);
	assert(Near(sample.translateX, 0));
}

} // namespace

int main()
{
	TestDeterministicRandomize();
	TestSanitization();
	TestMirrorSeries();
	TestCollisionPlacement();
	std::cout << "Transform math: deterministic randomize, mirror and collision verified.\n";
	return 0;
}
