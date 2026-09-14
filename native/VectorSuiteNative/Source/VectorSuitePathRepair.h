#ifndef VECTOR_SUITE_PATH_REPAIR_H
#define VECTOR_SUITE_PATH_REPAIR_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

struct VSRepairAnchor {
	double x;
	double y;
};

inline double VSRepairDistanceSquared(
	const VSRepairAnchor& first,
	const VSRepairAnchor& second)
{
	const double dx = second.x - first.x;
	const double dy = second.y - first.y;
	return dx * dx + dy * dy;
}

inline bool VSRepairAnchorIsProtected(
	const std::vector<bool>& protectedAnchors,
	size_t index)
{
	return index < protectedAnchors.size() && protectedAnchors[index];
}

inline double VSRepairDistanceFromLine(
	const VSRepairAnchor& point,
	const VSRepairAnchor& start,
	const VSRepairAnchor& end)
{
	const double dx = end.x - start.x;
	const double dy = end.y - start.y;
	const double length = std::sqrt(dx * dx + dy * dy);
	if (length < 1e-12) return std::sqrt(VSRepairDistanceSquared(point, start));
	return std::abs(
		dy * point.x - dx * point.y + end.x * start.y - end.y * start.x) /
		length;
}

inline bool VSRepairAnchorFallsBetween(
	const VSRepairAnchor& point,
	const VSRepairAnchor& start,
	const VSRepairAnchor& end,
	double tolerance)
{
	const double fromStartX = point.x - start.x;
	const double fromStartY = point.y - start.y;
	const double fromEndX = point.x - end.x;
	const double fromEndY = point.y - end.y;
	return fromStartX * fromEndX + fromStartY * fromEndY <=
		tolerance * tolerance;
}

inline std::vector<size_t> VSComputeRepairKeepIndices(
	const std::vector<VSRepairAnchor>& anchors,
	const std::vector<bool>& protectedAnchors,
	bool closed,
	double tolerance,
	bool removeDuplicates,
	bool removeCollinear)
{
	std::vector<size_t> keep;
	if (anchors.empty()) return keep;
	tolerance = std::max(0.0, tolerance);
	const double toleranceSquared = tolerance * tolerance;
	keep.reserve(anchors.size());

	for (size_t index = 0; index < anchors.size(); ++index) {
		const bool duplicate = removeDuplicates && !keep.empty() &&
			VSRepairDistanceSquared(anchors[keep.back()], anchors[index]) <=
				toleranceSquared;
		if (duplicate && !VSRepairAnchorIsProtected(protectedAnchors, index)) {
			continue;
		}
		keep.push_back(index);
	}
	if (closed && removeDuplicates && keep.size() > 3) {
		const size_t last = keep.back();
		if (!VSRepairAnchorIsProtected(protectedAnchors, last) &&
			VSRepairDistanceSquared(anchors[keep.front()], anchors[last]) <=
				toleranceSquared) {
			keep.pop_back();
		}
	}

	const size_t minimum = closed ? 3 : 2;
	if (!removeCollinear || keep.size() <= minimum) return keep;
	bool changed = true;
	while (changed && keep.size() > minimum) {
		changed = false;
		const size_t firstPosition = closed ? 0 : 1;
		const size_t endPosition = closed ? keep.size() : keep.size() - 1;
		for (size_t position = firstPosition; position < endPosition; ++position) {
			const size_t currentIndex = keep[position];
			if (VSRepairAnchorIsProtected(protectedAnchors, currentIndex)) continue;
			const size_t previousPosition = position == 0
				? keep.size() - 1
				: position - 1;
			const size_t nextPosition = position + 1 == keep.size()
				? 0
				: position + 1;
			const VSRepairAnchor& previous = anchors[keep[previousPosition]];
			const VSRepairAnchor& current = anchors[currentIndex];
			const VSRepairAnchor& next = anchors[keep[nextPosition]];
			if (VSRepairDistanceFromLine(current, previous, next) <= tolerance &&
				VSRepairAnchorFallsBetween(current, previous, next, tolerance)) {
				keep.erase(keep.begin() + static_cast<std::ptrdiff_t>(position));
				changed = true;
				break;
			}
		}
	}
	return keep;
}

#endif
