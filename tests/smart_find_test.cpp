#include "../native/VectorSuiteNative/Source/VectorSuiteSmartFind.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message)
{
	if (condition) return;
	std::cerr << message << '\n';
	std::exit(EXIT_FAILURE);
}

VSFindDescriptor Reference()
{
	VSFindDescriptor descriptor = {};
	descriptor.appearance.fillPaint = true;
	descriptor.appearance.fill = {5, 0.2, 0.4, 0.6, 0.0, 0};
	descriptor.appearance.strokePaint = true;
	descriptor.appearance.stroke = {0, 0.8, 0.0, 0.0, 0.0, 0};
	descriptor.appearance.strokeWidth = 2.0;
	descriptor.appearance.opacity = 0.75;
	descriptor.geometry.segmentCount = 4;
	descriptor.geometry.closed = true;
	descriptor.geometry.width = 120.0;
	descriptor.geometry.height = 80.0;
	return descriptor;
}

void TestAppearanceTolerance()
{
	const VSFindDescriptor reference = Reference();
	VSFindDescriptor candidate = reference;
	candidate.appearance.fill.first += 0.0005;
	candidate.appearance.strokeWidth += 0.005;
	Expect(VSFindAppearanceMatches(reference.appearance, candidate.appearance, 0.01),
		"appearance within tolerance should match");
	candidate.appearance.strokeWidth = 2.1;
	Expect(!VSFindAppearanceMatches(reference.appearance, candidate.appearance, 0.01),
		"different stroke width must not match");
}

void TestGeometryTolerance()
{
	const VSFindDescriptor reference = Reference();
	VSFindDescriptor candidate = reference;
	candidate.geometry.width += 0.05;
	candidate.geometry.height -= 0.05;
	Expect(VSFindGeometryMatches(reference.geometry, candidate.geometry, 0.1),
		"geometry within tolerance should match");
	candidate.geometry.segmentCount = 5;
	Expect(!VSFindGeometryMatches(reference.geometry, candidate.geometry, 0.1),
		"different point count must not match");
}

void TestCriteriaRemainIndependent()
{
	const VSFindDescriptor reference = Reference();
	VSFindDescriptor candidate = reference;
	candidate.geometry.width = 300.0;
	Expect(VSFindDescriptorMatches(
		reference, candidate, kVSFindByAppearance, 0.1, 0.01),
		"appearance criterion must ignore geometry");
	Expect(!VSFindDescriptorMatches(
		reference, candidate, kVSFindByAppearanceAndGeometry, 0.1, 0.01),
		"combined criterion must require geometry");
}

void TestResourceColorsUseIdentity()
{
	VSFindColor first = {2, 0, 0, 0, 0, 123};
	VSFindColor second = first;
	Expect(VSFindColorMatches(first, second, 0.01),
		"same pattern identity should match");
	second.identity = 456;
	Expect(!VSFindColorMatches(first, second, 0.01),
		"different pattern identity must not match");
}

} // namespace

int main()
{
	TestAppearanceTolerance();
	TestGeometryTolerance();
	TestCriteriaRemainIndependent();
	TestResourceColorsUseIdentity();
	std::cout << "Smart Find: appearance, geometry and criteria verified.\n";
	return EXIT_SUCCESS;
}
