#ifndef VECTOR_SUITE_PLUGIN_H
#define VECTOR_SUITE_PLUGIN_H

#include "Plugin.hpp"
#include "SDKAboutPluginsHelper.h"
#include "SDKDef.h"

#include "VectorSuiteCatalog.h"
#include "VectorSuiteID.h"
#include "VectorSuitePanel.h"
#include "VectorSuiteSuites.h"

#include <vector>

Plugin* AllocatePlugin(SPPluginRef pluginRef);
void FixupReload(Plugin* plugin);

class VectorSuitePlugin : public Plugin
{
public:
	explicit VectorSuitePlugin(SPPluginRef pluginRef);
	~VectorSuitePlugin() override {}

	FIXUP_VTABLE_EX(VectorSuitePlugin, Plugin);

public:
	ASErr Notify(AINotifierMessage* message) override;
	void ActivateModuleFromPanel(int moduleID);
	int GenerateFractalFromPanel(const VSFractalSettings* settings);
	void PanelDidResize();
	void PanelVisibilityChanged(AIBoolean visible);

protected:
	ASErr Message(char* caller, char* selector, void* message) override;
	ASErr StartupPlugin(SPInterfaceMessage* message) override;
	ASErr ShutdownPlugin(SPInterfaceMessage* message) override;
	ASErr GoMenuItem(AIMenuMessage* message) override;
	ASErr TrackToolCursor(AIToolMessage* message) override;
	ASErr ToolMouseDown(AIToolMessage* message) override;
	ASErr ToolMouseUp(AIToolMessage* message) override;
	ASErr ToolMouseDrag(AIToolMessage* message) override;
	ASErr SelectTool(AIToolMessage* message) override;

private:
	AIMenuItemHandle fAboutPluginMenu;
	AIMenuItemHandle fPanelMenuItem;
	AIToolHandle fToolHandle[kVSToolCount];

	AIPanelRef fPanel;
	AIPanelFlyoutMenuRef fPanelFlyoutMenu;
	void* fPanelController;

	AIRealPoint fStartingPoint;
	AIRealPoint fEndPoint;
	AIRect oldAnnotatorRect;
	AIAnnotatorHandle fAnnotatorHandle;
	AINotifierHandle fShutdownApplicationNotifier;
	AINotifierHandle fNotifySelectionChanged;
	AIResourceManagerHandle fResourceManagerHandle;
	AIArtHandle fLastFractalGroup;
	std::vector<AIRealPoint> fGesturePoints;

	ASErr AddTools(SPInterfaceMessage* message);
	ASErr AddMenus(SPInterfaceMessage* message);
	ASErr AddPanel(SPInterfaceMessage* message);
	ASErr EnsurePanelController();
	ASErr AddAnnotator(SPInterfaceMessage* message);
	ASErr AddNotifier(SPInterfaceMessage* message);
	ASErr ShowPanel(AIBoolean show, VSModuleID selectedModule);
	ASErr CreateProjectionArt(AIToolMessage* message);
	ASErr BeginModuleGesture(AIToolMessage* message, int toolIndex);
	ASErr ContinueModuleGesture(AIToolMessage* message, int toolIndex);
	ASErr EndModuleGesture(AIToolMessage* message, int toolIndex);
	ASErr ExecuteModuleCommand(VSModuleID module);
	ASErr DrawAnnotator(AIAnnotatorMessage* message);
	ASErr InvalAnnotator(AIAnnotatorMessage* message);
	ASErr GetPointString(const AIRealPoint& point, ai::UnicodeString& pointStr);
	ASErr PostStartupPlugin() override;
	ASErr ArtworkBoundsToViewBounds(const AIRealRect& artworkBounds, AIRect& viewBounds);
	ASErr InvalidateRect(const AIRealRect& invalRealRect);
	ASErr InvalidateRect(const AIRect& invalRect);

	int ToolIndex(AIToolHandle handle) const;
	int ToolIndexForModule(VSModuleID module) const;
	bool IsProjectionTool(int toolIndex) const;
};

#endif
