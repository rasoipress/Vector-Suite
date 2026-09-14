#include "../native/VectorSuiteNative/Source/VectorSuiteWorkflow.h"

#include <cassert>
#include <iostream>

int main()
{
	VSAutoSaveSettings settings = {9, -2, 0, 3};
	settings = VSSanitizeAutoSave(settings);
	assert(settings.enabled == 1);
	assert(settings.intervalMinutes == 1);
	assert(settings.modifiedOnly == 0);
	assert(settings.createVersionCopy == 1);

	settings.intervalMinutes = 500;
	assert(VSSanitizeAutoSave(settings).intervalMinutes == 120);
	assert(VSBackupFileName("Disegno", "ai", 1234) == "Disegno — 1234.ai");
	assert(VSBackupFileName("", "", 9) == "Documento — 9");
	VSRasterSettings raster = {-4, 99};
	raster = VSSanitizeRaster(raster);
	assert(raster.resolution == 1);
	assert(raster.resampling == 2);
	VSPathSettings path = {400, 7};
	path = VSSanitizePath(path);
	assert(path.tolerance == 100);
	assert(path.preserveCurves == 1);
	std::cout << "Workflow: autosave validation and version naming verified.\n";
	return 0;
}
