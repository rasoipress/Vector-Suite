#ifndef VECTOR_SUITE_PANEL_H
#define VECTOR_SUITE_PANEL_H

#include "VectorSuiteTransformMath.h"
#include "VectorSuiteStyleMath.h"
#include "VectorSuiteWorkflow.h"
#include "VectorSuiteDrawingMath.h"
#include "VectorSuiteSnap.h"

typedef void (*VSPanelActivateToolProc)(void* context, int moduleID);

// Values outside VSModuleID used by the panel to select one of Projection
// Studio's four internal tools without exposing them in Illustrator's toolbar.
enum {
	kVSPanelProjectionToolBase = 1000,
	kVSPanelProjectionToolCount = 4,
	kVSPanelProjectionProjectBase = 1100,
	kVSPanelProjectionUnprojectBase = 1110,
	kVSPanelProjectionCommandCount = 4,
	kVSPanelProjectionMoveBase = 1120,
	kVSPanelProjectionExtrudeBase = 1130,
	kVSPanelProjectionAxisCount = 3,
	kVSPanelProjectionScale = 1140,
	kVSPanelProjectionRotate = 1141,
	kVSPanelProjectionShear = 1142,
	kVSPanelProjectionMeasure = 1143,
	kVSPanelProjectionGrid = 1144,
	kVSPanelNativePen = 1145,
	kVSPanelNativeSnapPreferences = 1146,
	kVSPanelProjectionCopyOffset = 100,
	kVSPanelSmartFindAppearance = 2000,
	kVSPanelSmartFindGeometry = 2001,
	kVSPanelSmartFindExact = 2002,
	kVSPanelSmartFindApplyStyle = 2003,
	kVSPanelCollisionApply = 2100,
	kVSPanelMirrorApply = 2110,
	kVSPanelRandomizeApply = 2120,
	kVSPanelWidthApply = 2130,
	kVSPanelLiveStyleApply = 2140,
	kVSPanelColorApply = 2150,
	kVSPanelAutoSaveConfigure = 2160,
	kVSPanelAutoSaveNow = 2161,
	kVSPanelRasterSelect = 2170,
	kVSPanelRasterEmbed = 2171,
	kVSPanelRasterResample = 2172,
	kVSPanelRasterSetResolution = 2173,
	kVSPanelPathSimplify = 2180,
	kVSPanelPathCorner = 2181,
	kVSPanelPathSmooth = 2182,
	kVSPanelPathReverse = 2183,
	kVSPanelPrecisionConfigure = 2190,
	kVSPanelFluidConfigure = 2191,
	kVSPanelInkConfigure = 2192,
	kVSPanelTextureConfigure = 2200,
	kVSPanelStippleConfigure = 2201,
	kVSPanelGeometryConfigure = 2210,
	kVSPanelShapeConfigure = 2211
};

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
void VSRandomizeSet(const VSRandomizeSettings* settings);
void VSMirrorSet(const VSMirrorSettings* settings);
void VSCollisionSet(const VSCollisionSettings* settings);
void VSWidthSet(const VSWidthSettings* settings);
void VSLiveStyleSet(const VSLiveStyleSettings* settings);
void VSColorSet(const VSColorSettings* settings);
void VSAutoSaveSet(const VSAutoSaveSettings* settings);
VSAutoSaveSettings VSAutoSaveGet(void);
void VSRasterSet(const VSRasterSettings* settings);
VSRasterSettings VSRasterGet(void);
void VSPathSet(const VSPathSettings* settings);
VSPathSettings VSPathGet(void);
void VSPrecisionSet(const VSPrecisionSettings* settings);
void VSFluidSet(const VSFluidSettings* settings);
void VSInkSet(const VSInkSettings* settings);
void VSTextureSet(const VSTextureSettings* settings);
void VSStippleSet(const VSStippleSettings* settings);
void VSGeometrySet(const VSGeometrySettings* settings);
void VSShapeSet(const VSShapeSettings* settings);
void VSSnapSet(const VSSnap::Settings* settings);
VSSnap::Settings VSSnapGet(void);

#ifdef __cplusplus
}
#endif

#endif
