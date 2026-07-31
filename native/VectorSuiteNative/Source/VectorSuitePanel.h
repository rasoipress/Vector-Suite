#ifndef VECTOR_SUITE_PANEL_H
#define VECTOR_SUITE_PANEL_H

typedef void (*VSPanelActivateToolProc)(void* context, int moduleID);

struct VSFractalSettings {
	double seed;
	double initialLength;
	double lengthFalloff;
	double initialAngle;
	double angleFalloff;
	double minimumLength;
	int maximumIterations;
	int maximumPaths;
	double initialWidth;
	double widthFalloff;
	double lengthRandomness;
	double angleRandomness;
	int wave;
	bool automatic;
	bool advanced;
	bool group;
	bool replace;
};

typedef int (*VSPanelGenerateFractalProc)(
	void* context,
	const VSFractalSettings* settings);

#ifdef __cplusplus
extern "C" {
#endif

void* VSCreatePanelController(
	void* parentView,
	VSPanelActivateToolProc activateTool,
	VSPanelGenerateFractalProc generateFractal,
	void* context);
void VSDestroyPanelController(void* controller);
void VSResizePanelController(void* controller);
void VSSelectPanelModule(void* controller, int moduleID);

#ifdef __cplusplus
}
#endif

#endif
