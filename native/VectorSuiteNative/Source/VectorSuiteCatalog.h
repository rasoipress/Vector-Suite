#ifndef VECTOR_SUITE_CATALOG_H
#define VECTOR_SUITE_CATALOG_H

#include "AITypes.h"

enum VSModuleID {
	kVSPrecisionPen = 0,
	kVSFluidSketch,
	kVSInkStudio,
	kVSWidthStudio,
	kVSPathStudio,
	kVSGeometryLab,
	kVSCollisionAlign,
	kVSMirrorStudio,
	kVSShapeReform,
	kVSLiveStyle,
	kVSColorLab,
	kVSTextureLab,
	kVSStippleLab,
	kVSRandomize,
	kVSSmartFind,
	kVSVectorRepair,
	kVSRasterLab,
	kVSAutoSave,
	kVSDirectSettings,
	kVSSuiteCore,
	kVSProjectionStudio,
	kVSFractalGrove,
	kVSModuleCount
};

enum VSToolID {
	kVSToolSuiteCore = 0,
	kVSToolPrecisionPen,
	kVSToolFluidSketch,
	kVSToolInkStudio,
	kVSToolWidthStudio,
	kVSToolPathStudio,
	kVSToolGeometryLab,
	kVSToolCollisionAlign,
	kVSToolMirrorStudio,
	kVSToolShapeReform,
	kVSToolLiveStyle,
	kVSToolColorLab,
	kVSToolTextureLab,
	kVSToolStippleLab,
	kVSToolRandomize,
	kVSToolSmartFind,
	kVSToolVectorRepair,
	kVSToolRasterLab,
	kVSToolAutoSave,
	kVSToolDirectSettings,
	kVSToolProjectionLine,
	kVSToolProjectionRectangle,
	kVSToolProjectionEllipse,
	kVSToolProjectionBox,
	kVSToolFractalGrove,
	kVSToolCount
};

struct VSModuleDefinition {
	VSModuleID id;
	const char* key;
	const char* name;
	const char* category;
	const char* summary;
	ai::uint32 iconResourceID;
};

struct VSToolDefinition {
	VSToolID id;
	VSModuleID module;
	const char* internalName;
	const char* title;
	ai::uint32 iconResourceID;
};

extern const VSModuleDefinition kVSModules[kVSModuleCount];
extern const VSToolDefinition kVSTools[kVSToolCount];

#endif
