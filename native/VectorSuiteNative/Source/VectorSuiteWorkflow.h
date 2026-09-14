#ifndef VECTOR_SUITE_WORKFLOW_H
#define VECTOR_SUITE_WORKFLOW_H

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

struct VSAutoSaveSettings {
	int enabled;
	int intervalMinutes;
	int modifiedOnly;
	int createVersionCopy;
};

struct VSRasterSettings {
	double resolution;
	int resampling;
};

struct VSPathSettings {
	double tolerance;
	int preserveCurves;
};

inline VSAutoSaveSettings VSAutoSaveDefaults()
{
	return {0, 5, 1, 1};
}

inline VSAutoSaveSettings VSSanitizeAutoSave(VSAutoSaveSettings value)
{
	value.enabled = value.enabled ? 1 : 0;
	value.intervalMinutes = std::max(1, std::min(120, value.intervalMinutes));
	value.modifiedOnly = value.modifiedOnly ? 1 : 0;
	value.createVersionCopy = value.createVersionCopy ? 1 : 0;
	return value;
}

inline VSRasterSettings VSRasterDefaults()
{
	return {150.0, 2};
}

inline VSRasterSettings VSSanitizeRaster(VSRasterSettings value)
{
	value.resolution = std::max(1.0, std::min(2400.0, value.resolution));
	value.resampling = std::max(0, std::min(2, value.resampling));
	return value;
}

inline VSPathSettings VSPathDefaults()
{
	return {1.0, 1};
}

inline VSPathSettings VSSanitizePath(VSPathSettings value)
{
	value.tolerance = std::max(0.01, std::min(100.0, value.tolerance));
	value.preserveCurves = value.preserveCurves ? 1 : 0;
	return value;
}

inline std::string VSBackupFileName(
	const std::string& stem,
	const std::string& extension,
	std::uint64_t unixSeconds)
{
	std::ostringstream name;
	name << (stem.empty() ? "Documento" : stem)
		 << " — " << unixSeconds;
	if (!extension.empty()) name << "." << extension;
	return name.str();
}

#endif
