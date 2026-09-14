#ifndef VECTOR_SUITE_SMART_FIND_H
#define VECTOR_SUITE_SMART_FIND_H

#include <cmath>
#include <cstdint>

enum VSFindCriterion {
	kVSFindByAppearance = 0,
	kVSFindByGeometry,
	kVSFindByAppearanceAndGeometry
};

struct VSFindColor {
	int kind;
	double first;
	double second;
	double third;
	double fourth;
	std::uintptr_t identity;
};

struct VSFindAppearance {
	bool fillPaint;
	VSFindColor fill;
	bool strokePaint;
	VSFindColor stroke;
	double strokeWidth;
	double opacity;
};

struct VSFindGeometry {
	int segmentCount;
	bool closed;
	double width;
	double height;
};

struct VSFindDescriptor {
	VSFindAppearance appearance;
	VSFindGeometry geometry;
};

inline bool VSFindNear(double first, double second, double tolerance)
{
	return std::abs(first - second) <= std::abs(tolerance);
}

inline bool VSFindColorMatches(
	const VSFindColor& reference,
	const VSFindColor& candidate,
	double tolerance)
{
	if (reference.kind != candidate.kind) return false;
	if (reference.identity != 0 || candidate.identity != 0) {
		return reference.identity == candidate.identity &&
			VSFindNear(reference.first, candidate.first, tolerance);
	}
	return VSFindNear(reference.first, candidate.first, tolerance) &&
		VSFindNear(reference.second, candidate.second, tolerance) &&
		VSFindNear(reference.third, candidate.third, tolerance) &&
		VSFindNear(reference.fourth, candidate.fourth, tolerance);
}

inline bool VSFindAppearanceMatches(
	const VSFindAppearance& reference,
	const VSFindAppearance& candidate,
	double tolerance)
{
	if (reference.fillPaint != candidate.fillPaint ||
		reference.strokePaint != candidate.strokePaint) {
		return false;
	}
	if (reference.fillPaint &&
		!VSFindColorMatches(reference.fill, candidate.fill, tolerance)) {
		return false;
	}
	if (reference.strokePaint &&
		(!VSFindColorMatches(reference.stroke, candidate.stroke, tolerance) ||
		 !VSFindNear(reference.strokeWidth, candidate.strokeWidth, tolerance))) {
		return false;
	}
	return VSFindNear(reference.opacity, candidate.opacity, tolerance);
}

inline bool VSFindGeometryMatches(
	const VSFindGeometry& reference,
	const VSFindGeometry& candidate,
	double tolerance)
{
	return reference.segmentCount == candidate.segmentCount &&
		reference.closed == candidate.closed &&
		VSFindNear(reference.width, candidate.width, tolerance) &&
		VSFindNear(reference.height, candidate.height, tolerance);
}

inline bool VSFindDescriptorMatches(
	const VSFindDescriptor& reference,
	const VSFindDescriptor& candidate,
	VSFindCriterion criterion,
	double geometryTolerance,
	double appearanceTolerance)
{
	if (criterion == kVSFindByAppearance) {
		return VSFindAppearanceMatches(
			reference.appearance,
			candidate.appearance,
			appearanceTolerance);
	}
	if (criterion == kVSFindByGeometry) {
		return VSFindGeometryMatches(
			reference.geometry,
			candidate.geometry,
			geometryTolerance);
	}
	return VSFindAppearanceMatches(
			reference.appearance,
			candidate.appearance,
			appearanceTolerance) &&
		VSFindGeometryMatches(
			reference.geometry,
			candidate.geometry,
			geometryTolerance);
}

#endif
