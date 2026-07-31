#import <Foundation/Foundation.h>

#include "IllustratorSDK.h"
#include "SDKErrors.h"
#include "AppContext.hpp"

#include "VectorSuitePlugin.h"

#include <cstring>

namespace {

void PanelActivateTool(void* context, int moduleID)
{
	if (!context) return;
	VectorSuitePlugin* plugin = static_cast<VectorSuitePlugin*>(context);
	AppContext appContext(plugin->GetPluginRef());
	plugin->ActivateModuleFromPanel(moduleID);
}

int PanelGenerateFractal(void* context, const VSFractalSettings* settings)
{
	if (!context || !settings) return -1;
	VectorSuitePlugin* plugin = static_cast<VectorSuitePlugin*>(context);
	AppContext appContext(plugin->GetPluginRef());
	sAIUndo->SetUndoRedoCmdTextUS(
		ai::UnicodeString("Annulla Fractal Grove"),
		ai::UnicodeString("Ripeti Fractal Grove"),
		ai::UnicodeString("Fractal Grove"));
	return plugin->GenerateFractalFromPanel(settings);
}

void AIAPI PanelVisibilityChangedCallback(AIPanelRef panel, AIBoolean visible)
{
	AIPanelUserData data = nullptr;
	if (sAIPanel->GetUserData(panel, data) == kNoErr && data) {
		static_cast<VectorSuitePlugin*>(data)->PanelVisibilityChanged(visible);
	}
}

void AIAPI PanelSizeChangedCallback(AIPanelRef panel)
{
	AIPanelUserData data = nullptr;
	if (sAIPanel->GetUserData(panel, data) == kNoErr && data) {
		static_cast<VectorSuitePlugin*>(data)->PanelDidResize();
	}
}

} // namespace

Plugin* AllocatePlugin(SPPluginRef pluginRef)
{
	return new VectorSuitePlugin(pluginRef);
}

void FixupReload(Plugin* plugin)
{
	VectorSuitePlugin::FixupVTable(static_cast<VectorSuitePlugin*>(plugin));
}

VectorSuitePlugin::VectorSuitePlugin(SPPluginRef pluginRef)
	: Plugin(pluginRef),
	  fAboutPluginMenu(nullptr),
	  fPanelMenuItem(nullptr),
	  fPanel(nullptr),
	  fPanelFlyoutMenu(nullptr),
	  fPanelController(nullptr),
	  fStartingPoint{0, 0},
	  fEndPoint{0, 0},
	  oldAnnotatorRect{0, 0, 0, 0},
	  fAnnotatorHandle(nullptr),
	  fShutdownApplicationNotifier(nullptr),
	  fNotifySelectionChanged(nullptr),
	  fResourceManagerHandle(nullptr),
	  fLastFractalGroup(nullptr)
{
	std::memset(fToolHandle, 0, sizeof(fToolHandle));
	std::strncpy(fPluginName, kVectorSuitePluginName, kMaxStringLength);
}

ASErr VectorSuitePlugin::Message(char* caller, char* selector, void* message)
{
	ASErr error = kNoErr;
	try {
		error = Plugin::Message(caller, selector, message);
	}
	catch (ai::Error& exception) {
		error = exception;
	}
	catch (...) {
		error = kCantHappenErr;
	}

	if (error == kUnhandledMsgErr && std::strcmp(caller, kCallerAIAnnotation) == 0) {
		if (std::strcmp(selector, kSelectorAIDrawAnnotation) == 0) {
			error = DrawAnnotator(static_cast<AIAnnotatorMessage*>(message));
		}
		else if (std::strcmp(selector, kSelectorAIInvalAnnotation) == 0) {
			error = InvalAnnotator(static_cast<AIAnnotatorMessage*>(message));
		}
	}
	else if (error && error != kUnhandledMsgErr) {
		Plugin::ReportError(error, caller, selector, message);
	}
	return error;
}

ASErr VectorSuitePlugin::StartupPlugin(SPInterfaceMessage* message)
{
	ASErr error = Plugin::StartupPlugin(message);
	if (error) return error;

	ai::int32 pluginOptions = 0;
	error = sAIPlugin->GetPluginOptions(message->d.self, &pluginOptions);
	if (error) return error;
	error = sAIPlugin->SetPluginOptions(
		message->d.self,
		pluginOptions | kPluginWantsResultsAutoSelectedOption);
	if (error) return error;

	error = AddTools(message);
	if (error) return error;
	error = AddAnnotator(message);
	if (error) return error;
	error = AddMenus(message);
	if (error) return error;
	error = AddPanel(message);
	if (error) return error;
	return AddNotifier(message);
}

ASErr VectorSuitePlugin::ShutdownPlugin(SPInterfaceMessage* message)
{
	if (fPanelController) {
		VSDestroyPanelController(fPanelController);
		fPanelController = nullptr;
	}
	if (fPanel) {
		sAIPanel->Destroy(fPanel);
		fPanel = nullptr;
	}
	if (fPanelFlyoutMenu) {
		sAIPanelFlyoutMenu->Destroy(fPanelFlyoutMenu);
		fPanelFlyoutMenu = nullptr;
	}
	if (fResourceManagerHandle) {
		sAIUser->DisposeCursorResourceMgr(fResourceManagerHandle);
		fResourceManagerHandle = nullptr;
	}
	return Plugin::ShutdownPlugin(message);
}

ASErr VectorSuitePlugin::Notify(AINotifierMessage* message)
{
	if (message->notifier == fShutdownApplicationNotifier) {
		if (fResourceManagerHandle) {
			ASErr error = sAIUser->DisposeCursorResourceMgr(fResourceManagerHandle);
			fResourceManagerHandle = nullptr;
			return error;
		}
	}
	else if (message->notifier == fNotifySelectionChanged && fAnnotatorHandle) {
		AIRealRect viewBounds = {0, 0, 0, 0};
		ASErr error = sAIDocumentView->GetDocumentViewBounds(nullptr, &viewBounds);
		if (!error) error = InvalidateRect(viewBounds);
		return error;
	}
	return kNoErr;
}

ASErr VectorSuitePlugin::AddMenus(SPInterfaceMessage* message)
{
	ASErr error = sAIMenu->AddMenuItemZString(
		message->d.self,
		"Vector Suite Panel",
		kOtherPalettesMenuGroup,
		ZREF("Vector Suite"),
		kMenuItemNoOptions,
		&fPanelMenuItem);
	if (error) return error;

	return sAIMenu->AddMenuItemZString(
		message->d.self,
		"About Vector Suite",
		kAboutMenuGroup,
		ZREF("Informazioni su Vector Suite…"),
		kMenuItemNoOptions,
		&fAboutPluginMenu);
}

ASErr VectorSuitePlugin::GoMenuItem(AIMenuMessage* message)
{
	if (message->menuItem == fPanelMenuItem) {
		AIBoolean shown = false;
		ASErr error = sAIPanel->IsShown(fPanel, shown);
		if (error) return error;
		if (shown && !fPanelController) return ShowPanel(true, kVSSuiteCore);
		return ShowPanel(!shown, kVSSuiteCore);
	}
	if (message->menuItem == fAboutPluginMenu) {
		SDKAboutPluginsHelper helper;
		helper.PopAboutBox(
			message,
			"Vector Suite",
			"22 moduli nativi per Adobe Illustrator. Interfaccia monocromatica adattiva.");
		return kNoErr;
	}
	return kUnhandledMsgErr;
}

ASErr VectorSuitePlugin::AddTools(SPInterfaceMessage* message)
{
	ASErr error = kNoErr;
	char firstToolName[256] = {0};

	for (int index = 0; index < kVSToolCount; ++index) {
		const VSToolDefinition& definition = kVSTools[index];
		AIAddToolData data;
		data.title = ai::UnicodeString(definition.title);
		data.tooltip = ai::UnicodeString(definition.title);
		data.normalIconResID = definition.iconResourceID;
		data.darkIconResID = definition.iconResourceID;
		data.iconType = ai::IconType::kSVG;

		if (index == 0) {
			std::strncpy(firstToolName, definition.internalName, sizeof(firstToolName) - 1);
			data.sameGroupAs = kNoTool;
			data.sameToolsetAs = kNoTool;
		}
		else {
			error = sAITool->GetToolNumberFromName(firstToolName, &data.sameGroupAs);
			if (error) return error;
			error = sAITool->GetToolNumberFromName(firstToolName, &data.sameToolsetAs);
			if (error) return error;
		}

		error = sAITool->AddTool(
			message->d.self,
			definition.internalName,
			data,
			kToolWantsToTrackCursorOption,
			&fToolHandle[index]);
		if (error) return error;
	}
	return kNoErr;
}

ASErr VectorSuitePlugin::AddPanel(SPInterfaceMessage* message)
{
	AISize minimum = {280, 360};
	ASErr error = sAIPanel->Create(
		message->d.self,
		ai::UnicodeString("studio.vectorsuite.panel.main"),
		ai::UnicodeString("Vector Suite"),
		1,
		minimum,
		true,
		nullptr,
		this,
		fPanel);
	if (error) return error;

	AISize preferredFloating = {330, 560};
	AISize preferredDocked = {300, 520};
	AISize maximum = {620, 1100};
	error = sAIPanel->SetSizes(
		fPanel,
		minimum,
		preferredFloating,
		preferredDocked,
		maximum);
	if (error) return error;

	error = sAIPanel->SetSVGIconResourceID(
		fPanel,
		kVectorSuitePanelIconResID,
		kVectorSuitePanelIconResID);
	if (error) return error;

	error = sAIPanel->SetVisibilityChangedNotifyProc(fPanel, PanelVisibilityChangedCallback);
	if (error) return error;
	error = sAIPanel->SetSizeChangedNotifyProc(fPanel, PanelSizeChangedCallback);
	if (error) return error;

	// La NSView di piattaforma non esiste in modo affidabile durante lo
	// startup. Il controller viene creato una sola volta, subito dopo che il
	// pannello è stato realmente mostrato dall'utente.
	error = sAIPanel->Show(fPanel, false);
	return error;
}

ASErr VectorSuitePlugin::EnsurePanelController()
{
	if (fPanelController) {
		return kNoErr;
	}

	// La finestra di piattaforma è una NSView su macOS e una HWND su Windows:
	// il controller del pannello riceve un puntatore opaco e ciascuna delle due
	// implementazioni sa che cosa farne.
#if defined(MAC_ENV)
	AIPanelPlatformWindow __autoreleasing platformView = nullptr;
	ASErr error = sAIPanel->GetPlatformWindow(fPanel, platformView);
	if (error) return error;
	if (!platformView) return kCantHappenErr;

	@try {
		fPanelController = VSCreatePanelController(
			(__bridge void*)platformView,
			PanelActivateTool,
			PanelGenerateFractal,
			this);
	}
	@catch (NSException* exception) {
		(void)exception;
		fPanelController = nullptr;
	}
#else
	AIPanelPlatformWindow platformView = nullptr;
	ASErr error = sAIPanel->GetPlatformWindow(fPanel, platformView);
	if (error) return error;
	if (!platformView) return kCantHappenErr;

	fPanelController = VSCreatePanelController(
		reinterpret_cast<void*>(platformView),
		PanelActivateTool,
		PanelGenerateFractal,
		this);
#endif
	return fPanelController ? kNoErr : kCantHappenErr;
}

ASErr VectorSuitePlugin::ShowPanel(AIBoolean show, VSModuleID selectedModule)
{
	if (!fPanel) return kNoErr;
	if (!show) return sAIPanel->Show(fPanel, false);

	// AIPanel segue un ciclo lazy: prima rende disponibile la view Cocoa,
	// quindi può ospitare l'interfaccia. Questo ordine evita il pannello vuoto
	// lasciato da un tentativo di attachment durante StartupPlugin.
	ASErr error = sAIPanel->Show(fPanel, true);
	if (error) return error;
	error = EnsurePanelController();
	if (error) {
		sAIPanel->Show(fPanel, false);
		return error;
	}
	VSSelectPanelModule(fPanelController, selectedModule);
	VSResizePanelController(fPanelController);
	return kNoErr;
}

void VectorSuitePlugin::PanelDidResize()
{
	if (fPanelController) VSResizePanelController(fPanelController);
}

void VectorSuitePlugin::PanelVisibilityChanged(AIBoolean visible)
{
	if (fPanelMenuItem) sAIMenu->CheckItem(fPanelMenuItem, visible);
	if (visible) {
		if (!EnsurePanelController() && fPanelController) {
			VSResizePanelController(fPanelController);
		}
	}
}

int VectorSuitePlugin::ToolIndex(AIToolHandle handle) const
{
	for (int index = 0; index < kVSToolCount; ++index) {
		if (fToolHandle[index] == handle) return index;
	}
	return -1;
}

int VectorSuitePlugin::ToolIndexForModule(VSModuleID module) const
{
	for (int index = 0; index < kVSToolCount; ++index) {
		if (kVSTools[index].module == module) return index;
	}
	return -1;
}

bool VectorSuitePlugin::IsProjectionTool(int toolIndex) const
{
	return toolIndex >= kVSToolProjectionLine && toolIndex <= kVSToolProjectionBox;
}

void VectorSuitePlugin::ActivateModuleFromPanel(int moduleID)
{
	if (moduleID < 0 || moduleID >= kVSModuleCount) return;
	int toolIndex = ToolIndexForModule(static_cast<VSModuleID>(moduleID));
	if (toolIndex >= 0 && fToolHandle[toolIndex]) {
		sAITool->SetSelectedTool(fToolHandle[toolIndex]);
	}
}

ASErr VectorSuitePlugin::SelectTool(AIToolMessage* message)
{
	const int index = ToolIndex(message->tool);
	if (index < 0) return kNoErr;

	const VSModuleID module = kVSTools[index].module;
	if (index == kVSToolSuiteCore ||
		index == kVSToolDirectSettings ||
		index == kVSToolFractalGrove) {
		return ShowPanel(true, module);
	}

	AIBoolean shown = false;
	ASErr error = sAIPanel->IsShown(fPanel, shown);
	if (!error && shown && fPanelController) {
		VSSelectPanelModule(fPanelController, module);
	}
	return error;
}

ASErr VectorSuitePlugin::TrackToolCursor(AIToolMessage* message)
{
	const int index = ToolIndex(message->tool);
	if (index < 0 || !sAIUser) return kNoErr;
	return sAIUser->SetSVGCursor(kVSTools[index].iconResourceID, fResourceManagerHandle);
}

ASErr VectorSuitePlugin::ToolMouseDown(AIToolMessage* message)
{
	const int index = ToolIndex(message->tool);
	if (index < 0 ||
		index == kVSToolSuiteCore ||
		index == kVSToolDirectSettings ||
		index == kVSToolFractalGrove) {
		return kNoErr;
	}
	fStartingPoint = message->cursor;
	fEndPoint = message->cursor;
	fGesturePoints.clear();
	fGesturePoints.push_back(message->cursor);
	return IsProjectionTool(index)
		? sAIAnnotator->SetAnnotatorActive(fAnnotatorHandle, true)
		: BeginModuleGesture(message, index);
}

ASErr VectorSuitePlugin::ToolMouseDrag(AIToolMessage* message)
{
	const int index = ToolIndex(message->tool);
	if (index < 0 ||
		index == kVSToolSuiteCore ||
		index == kVSToolDirectSettings ||
		index == kVSToolFractalGrove) {
		return kNoErr;
	}
	return IsProjectionTool(index)
		? CreateProjectionArt(message)
		: ContinueModuleGesture(message, index);
}

ASErr VectorSuitePlugin::ToolMouseUp(AIToolMessage* message)
{
	const int index = ToolIndex(message->tool);
	if (index < 0 ||
		index == kVSToolSuiteCore ||
		index == kVSToolDirectSettings ||
		index == kVSToolFractalGrove) {
		return kNoErr;
	}
	if (IsProjectionTool(index)) {
		return sAIAnnotator->SetAnnotatorActive(fAnnotatorHandle, false);
	}
	return EndModuleGesture(message, index);
}

ASErr VectorSuitePlugin::AddNotifier(SPInterfaceMessage* message)
{
	ASErr error = sAINotifier->AddNotifier(
		fPluginRef,
		"Vector Suite Shutdown",
		kAIApplicationShutdownNotifier,
		&fShutdownApplicationNotifier);
	if (error) return error;

	return sAINotifier->AddNotifier(
		fPluginRef,
		"Vector Suite Selection",
		kAIArtSelectionChangedNotifier,
		&fNotifySelectionChanged);
}

ASErr VectorSuitePlugin::AddAnnotator(SPInterfaceMessage* message)
{
	ASErr error = sAIAnnotator->AddAnnotator(
		message->d.self,
		"Vector Suite Coordinates",
		&fAnnotatorHandle);
	if (error) return error;
	return sAIAnnotator->SetAnnotatorActive(fAnnotatorHandle, false);
}

ASErr VectorSuitePlugin::DrawAnnotator(AIAnnotatorMessage* message)
{
	ai::UnicodeString pointString;
	ASErr error = GetPointString(fEndPoint, pointString);
	if (error) return error;

	AIPoint point;
	error = sAIDocumentView->ArtworkPointToViewPoint(nullptr, &fEndPoint, &point);
	if (error) return error;
	point.h += 7;
	point.v -= 7;

	AIRect bounds;
	error = sAIAnnotatorDrawer->GetTextBounds(
		message->drawer,
		pointString,
		&point,
		false,
		bounds,
		false);
	if (error) return error;

	const AIRGBColor black = {0, 0, 0};
	const AIRGBColor white = {65535, 65535, 65535};
	sAIAnnotatorDrawer->SetColor(message->drawer, black);
	error = sAIAnnotatorDrawer->DrawRect(message->drawer, bounds, true);
	if (error) return error;
	sAIAnnotatorDrawer->SetColor(message->drawer, white);
	sAIAnnotatorDrawer->SetLineWidth(message->drawer, 0.5);
	error = sAIAnnotatorDrawer->DrawRect(message->drawer, bounds, false);
	if (error) return error;
	error = sAIAnnotatorDrawer->SetFontPreset(message->drawer, kAIAFSmall);
	if (error) return error;
	error = sAIAnnotatorDrawer->DrawTextAligned(
		message->drawer,
		pointString,
		kAICenter,
		kAIMiddle,
		bounds,
		false);
	if (!error) oldAnnotatorRect = bounds;
	return error;
}

ASErr VectorSuitePlugin::InvalAnnotator(AIAnnotatorMessage* message)
{
	ai::NOTUSED(message);
	return sAIAnnotator->InvalAnnotationRect(nullptr, &oldAnnotatorRect);
}

ASErr VectorSuitePlugin::InvalidateRect(const AIRect& invalidRect)
{
	return sAIAnnotator->InvalAnnotationRect(nullptr, &invalidRect);
}

ASErr VectorSuitePlugin::InvalidateRect(const AIRealRect& invalidArtworkRect)
{
	AIRect invalidViewRect;
	ASErr error = ArtworkBoundsToViewBounds(invalidArtworkRect, invalidViewRect);
	return error ? error : InvalidateRect(invalidViewRect);
}

ASErr VectorSuitePlugin::ArtworkBoundsToViewBounds(
	const AIRealRect& artworkBounds,
	AIRect& viewBounds)
{
	return sAIDocumentView->ArtworkRectToViewRect(nullptr, &artworkBounds, &viewBounds);
}

ASErr VectorSuitePlugin::GetPointString(
	const AIRealPoint& point,
	ai::UnicodeString& pointString)
{
	const ASInt32 precision = 2;
	ai::NumberFormat format;
	ai::UnicodeString horizontal;
	ai::UnicodeString vertical;
	horizontal = format.toString(static_cast<float>(point.h), precision, horizontal);
	vertical = format.toString(static_cast<float>(-point.v), precision, vertical);
	pointString.append(ai::UnicodeString("x "))
		.append(horizontal)
		.append(ai::UnicodeString("  y "))
		.append(vertical);
	return kNoErr;
}

ASErr VectorSuitePlugin::PostStartupPlugin()
{
	return sAIUser->CreateCursorResourceMgr(fPluginRef, &fResourceManagerHandle);
}
