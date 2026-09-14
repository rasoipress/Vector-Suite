#if defined(__APPLE__)
#import <Foundation/Foundation.h>
#endif

#include "IllustratorSDK.h"
#include "SDKErrors.h"
#include "AppContext.hpp"
#include "AICommandManager.h"
#include "AIMenuCommandString.h"
#include "actions/AIDocumentAction.h"

#include "VectorSuitePlugin.h"
#include "VectorSuiteProjection.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <system_error>

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
	  fGestureToolIndex(-1),
	  fGestureActive(false),
	  fAnnotatorHandle(nullptr),
	  fShutdownApplicationNotifier(nullptr),
	  fNotifySelectionChanged(nullptr),
	  fResourceManagerHandle(nullptr),
	  fLastFractalGroup(nullptr),
	  fAutoSaveTimer(nullptr),
	  fApplicationShuttingDown(false)
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
	error = AddNotifier(message);
	if (error) return error;
	return AddAutoSaveTimer(message);
}

ASErr VectorSuitePlugin::ShutdownPlugin(SPInterfaceMessage* message)
{
	if (fAutoSaveTimer && sAITimer) {
		sAITimer->SetTimerActive(fAutoSaveTimer, false);
		fAutoSaveTimer = nullptr;
	}
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
	if(message->notifier==fSnapArtChanged||message->notifier==fSnapDocumentChanged||message->notifier==fNotifySelectionChanged)
		fSnapGeometryDirty=true;
	if (message->notifier == fShutdownApplicationNotifier) {
		fApplicationShuttingDown = true;
		if (fAutoSaveTimer && sAITimer) {
			sAITimer->SetTimerActive(fAutoSaveTimer, false);
		}
		if (fResourceManagerHandle) {
			ASErr error = sAIUser->DisposeCursorResourceMgr(fResourceManagerHandle);
			fResourceManagerHandle = nullptr;
			return error;
		}
	}
	else if (message->notifier == fNotifySelectionChanged && fAnnotatorHandle) {
		// La selezione cambia anche mentre l'ultimo documento viene chiuso.
		// In quella finestra GetDocumentViewBounds restituisce "no document";
		// propagare l'errore fa aprire a Illustrator un messaggio modale e può
		// sembrare che l'applicazione sia bloccata in uscita.
		if (fApplicationShuttingDown || !sAIDocument) return kNoErr;
		AIDocumentHandle document = nullptr;
		ASErr error = sAIDocument->GetDocument(&document);
		if (error || !document) return kNoErr;
		AIBoolean exists = false;
		error = sAIDocument->DocumentExists(document, &exists);
		if (error || !exists) return kNoErr;

		AIRealRect viewBounds = {0, 0, 0, 0};
		error = sAIDocumentView->GetDocumentViewBounds(nullptr, &viewBounds);
		if (error) return kNoErr;
		(void)InvalidateRect(viewBounds);
		return kNoErr;
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
		if (shown && !fPanelController) return ShowPanel(true, kVSPrecisionPen);
		return ShowPanel(!shown, kVSPrecisionPen);
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

		// Ogni strumento è un toolset a sé, tutti nello stesso gruppo.
		//
		// Prima erano tutti nello stesso toolset: nella palette occupavano un
		// solo pulsante e per raggiungerli bisognava tenerlo premuto. Con un
		// toolset per strumento ognuno ha il proprio slot, quindi compare
		// singolarmente nell'editor della barra strumenti di Illustrator e si
		// può trascinare dove si vuole. Il gruppo comune serve solo a tenerli
		// vicini, separati dagli strumenti di serie.
		if (index == 0) {
			std::strncpy(firstToolName, definition.internalName, sizeof(firstToolName) - 1);
			data.sameGroupAs = kNoTool;
			data.sameToolsetAs = kNoTool;
		}
		else {
			error = sAITool->GetToolNumberFromName(firstToolName, &data.sameGroupAs);
			if (error) return error;
			data.sameToolsetAs = kNoTool;
		}

		error = sAITool->AddTool(
			message->d.self,
			definition.internalName,
			data,
			kToolWantsToTrackCursorOption | kToolWantsHiddenToolOption,
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
	if (fPanelController) {
		const ASErr configureError = ConfigureAutoSave(VSAutoSaveGet());
		if (configureError) return configureError;
	}
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
	if (moduleID == kVSPanelNativeSnapPreferences) {
		const void* acquiredSuite = nullptr;
		ASErr error = sSPBasic->AcquireSuite(kAICommandManagerSuite,
			kAICommandManagerSuiteVersion, &acquiredSuite);
		const auto* commands = static_cast<const AICommandManagerSuite*>(acquiredSuite);
		if (!error && commands) {
			AICommandID command = 0;
			error = commands->GetCommandIDFromName(kSnapPrefCommandStr, &command);
			if (!error) error = sAIMenu->InvokeMenuAction(command);
			sSPBasic->ReleaseSuite(kAICommandManagerSuite, kAICommandManagerSuiteVersion);
		}
		if (error && sAIUser) sAIUser->MessageAlert(ai::UnicodeString(
			"Apri le preferenze di Illustrator > Guide sensibili per configurare gli snap."));
		return;
	}
	if (moduleID == kVSPanelNativePen) {
		sAITool->SetSelectedToolByName("Adobe Pen Tool");
		return;
	}
	if (moduleID == kVSPanelProjectionGrid) {
		sAIUndo->SetUndoRedoCmdTextUS(ai::UnicodeString("Annulla griglia"), ai::UnicodeString("Ripeti griglia"), ai::UnicodeString("Griglia assonometrica"));
		if (CreateProjectionGuideGrid()) {
			sAIUndo->UndoChanges();
			sAIUser->MessageAlert(ai::UnicodeString("Impossibile creare la griglia. Verifica documento e livello attivo."));
		}
		return;
	}
	bool copyProjection = false;
	int projectionCommand = moduleID;
	const int possibleCopiedCommand =
		projectionCommand - kVSPanelProjectionCopyOffset;
	const bool copiedProject =
		possibleCopiedCommand >= kVSPanelProjectionProjectBase &&
		possibleCopiedCommand < kVSPanelProjectionProjectBase +
			kVSPanelProjectionCommandCount;
	const bool copiedUnproject =
		possibleCopiedCommand >= kVSPanelProjectionUnprojectBase &&
		possibleCopiedCommand < kVSPanelProjectionUnprojectBase +
			kVSPanelProjectionCommandCount;
	const bool copiedMove =
		possibleCopiedCommand >= kVSPanelProjectionMoveBase &&
		possibleCopiedCommand < kVSPanelProjectionMoveBase +
			kVSPanelProjectionAxisCount;
	const bool copiedPlaneTransform =
		possibleCopiedCommand >= kVSPanelProjectionScale &&
		possibleCopiedCommand <= kVSPanelProjectionShear;
	if (copiedProject || copiedUnproject || copiedMove || copiedPlaneTransform) {
		copyProjection = true;
		projectionCommand = possibleCopiedCommand;
	}
	const bool projectsSelection =
		projectionCommand >= kVSPanelProjectionProjectBase &&
		projectionCommand < kVSPanelProjectionProjectBase +
			kVSPanelProjectionCommandCount;
	const bool unprojectsSelection =
		projectionCommand >= kVSPanelProjectionUnprojectBase &&
		projectionCommand < kVSPanelProjectionUnprojectBase +
			kVSPanelProjectionCommandCount;
	if (projectsSelection || unprojectsSelection) {
		const int base = projectsSelection
			? kVSPanelProjectionProjectBase
			: kVSPanelProjectionUnprojectBase;
		sAIUndo->SetUndoRedoCmdTextUS(
			ai::UnicodeString(projectsSelection
				? "Annulla proiezione"
				: "Annulla deproiezione"),
			ai::UnicodeString(projectsSelection
				? "Ripeti proiezione"
				: "Ripeti deproiezione"),
			ai::UnicodeString("Projection Studio"));
		const ASErr error = TransformProjectionSelection(
			projectionCommand - base,
			unprojectsSelection,
			copyProjection);
		if (error && sAIUser) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Vector Suite: la trasformazione non è stata completata."));
		}
		return;
	}
	const bool movesSelection =
		projectionCommand >= kVSPanelProjectionMoveBase &&
		projectionCommand < kVSPanelProjectionMoveBase +
			kVSPanelProjectionAxisCount;
	const bool extrudesSelection =
		projectionCommand >= kVSPanelProjectionExtrudeBase &&
		projectionCommand < kVSPanelProjectionExtrudeBase +
			kVSPanelProjectionAxisCount;
	if (movesSelection || extrudesSelection) {
		const int base = movesSelection
			? kVSPanelProjectionMoveBase
			: kVSPanelProjectionExtrudeBase;
		sAIUndo->SetUndoRedoCmdTextUS(
			ai::UnicodeString(movesSelection
				? "Annulla spostamento assonometrico"
				: "Annulla estrusione"),
			ai::UnicodeString(movesSelection
				? "Ripeti spostamento assonometrico"
				: "Ripeti estrusione"),
			ai::UnicodeString("Projection Studio"));
		const VSProjectionSettings settings = VSProjectionGet();
		const ASErr error = MoveOrExtrudeProjectionSelection(
			projectionCommand - base,
			settings.moveDistance,
			extrudesSelection,
			copyProjection);
		if (error && sAIUser) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Vector Suite: l’operazione assonometrica non è stata completata."));
		}
		return;
	}
	if (projectionCommand == kVSPanelProjectionMeasure) {
		const ASErr error = MeasureProjectionSelection();
		if (error && sAIUser) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Vector Suite: la misurazione non è stata completata."));
		}
		return;
	}
	if (projectionCommand >= kVSPanelProjectionScale &&
		projectionCommand <= kVSPanelProjectionShear) {
		const char* undo = "Annulla trasformazione sul piano";
		const char* redo = "Ripeti trasformazione sul piano";
		if (projectionCommand == kVSPanelProjectionScale) {
			undo = "Annulla scala sul piano";
			redo = "Ripeti scala sul piano";
		}
		else if (projectionCommand == kVSPanelProjectionRotate) {
			undo = "Annulla rotazione sul piano";
			redo = "Ripeti rotazione sul piano";
		}
		else if (projectionCommand == kVSPanelProjectionShear) {
			undo = "Annulla inclinazione sul piano";
			redo = "Ripeti inclinazione sul piano";
		}
		sAIUndo->SetUndoRedoCmdTextUS(
			ai::UnicodeString(undo),
			ai::UnicodeString(redo),
			ai::UnicodeString("Projection Studio"));
		const ASErr error = TransformProjectionPlaneSelection(
			projectionCommand,
			copyProjection);
		if (error && sAIUser) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Vector Suite: la trasformazione sul piano non è stata completata."));
		}
		return;
	}

	if (moduleID >= kVSPanelProjectionToolBase &&
		moduleID < kVSPanelProjectionToolBase + kVSPanelProjectionToolCount) {
		const int toolIndex = kVSToolProjectionLine +
			(moduleID - kVSPanelProjectionToolBase);
		if (fToolHandle[toolIndex]) sAITool->SetSelectedTool(fToolHandle[toolIndex]);
		return;
	}
	if (moduleID >= kVSPanelSmartFindAppearance &&
		moduleID <= kVSPanelSmartFindApplyStyle) {
		if (moduleID == kVSPanelSmartFindApplyStyle) {
			sAIUndo->SetUndoRedoCmdTextUS(
				ai::UnicodeString("Annulla applicazione stile"),
				ai::UnicodeString("Ripeti applicazione stile"),
				ai::UnicodeString("Smart Find"));
		}
		const ASErr error = ExecuteSmartFindCommand(moduleID);
		if (error && sAIUser) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Vector Suite: Smart Find non ha completato il comando."));
		}
		return;
	}
	if (moduleID == kVSPanelCollisionApply ||
		moduleID == kVSPanelMirrorApply ||
		moduleID == kVSPanelRandomizeApply) {
		const char* moduleName = moduleID == kVSPanelCollisionApply
			? "Collision Align"
			: (moduleID == kVSPanelMirrorApply ? "Mirror Studio" : "Randomize");
		sAIUndo->SetUndoRedoCmdTextUS(
			ai::UnicodeString("Annulla trasformazione"),
			ai::UnicodeString("Ripeti trasformazione"),
			ai::UnicodeString(moduleName));
		const ASErr error = ExecuteTransformCommand(moduleID);
		if (error && sAIUser) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Vector Suite: la trasformazione non è stata completata."));
		}
		return;
	}
	if (moduleID == kVSPanelWidthApply ||
		moduleID == kVSPanelLiveStyleApply ||
		moduleID == kVSPanelColorApply) {
		const char* moduleName = moduleID == kVSPanelWidthApply
			? "Width Studio"
			: (moduleID == kVSPanelLiveStyleApply ? "Live Style" : "Color Lab");
		sAIUndo->SetUndoRedoCmdTextUS(
			ai::UnicodeString("Annulla modifica stile"),
			ai::UnicodeString("Ripeti modifica stile"),
			ai::UnicodeString(moduleName));
		const ASErr error = ExecuteStyleCommand(moduleID);
		if (error && sAIUser) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Vector Suite: la modifica dello stile non è stata completata."));
		}
		return;
	}
	if (moduleID == kVSPanelAutoSaveConfigure ||
		moduleID == kVSPanelAutoSaveNow) {
		const ASErr error = ExecuteWorkflowCommand(moduleID);
		if (error && sAIUser) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Vector Suite: Auto Save non ha completato il comando."));
		}
		return;
	}
	if (moduleID >= kVSPanelRasterSelect &&
		moduleID <= kVSPanelRasterSetResolution) {
		if (moduleID != kVSPanelRasterSelect) {
			sAIUndo->SetUndoRedoCmdTextUS(
				ai::UnicodeString("Annulla Raster Lab"),
				ai::UnicodeString("Ripeti Raster Lab"),
				ai::UnicodeString("Raster Lab"));
		}
		const ASErr error = ExecuteRasterCommand(moduleID);
		if (error && sAIUser) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Vector Suite: Raster Lab non ha completato il comando."));
		}
		return;
	}
	if (moduleID >= kVSPanelPathSimplify && moduleID <= kVSPanelPathReverse) {
		sAIUndo->SetUndoRedoCmdTextUS(
			ai::UnicodeString("Annulla Path Studio"),
			ai::UnicodeString("Ripeti Path Studio"),
			ai::UnicodeString("Path Studio"));
		const ASErr error = ExecutePathCommand(moduleID);
		if (error && sAIUser) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Vector Suite: Path Studio non ha completato il comando."));
		}
		return;
	}
	if (moduleID >= kVSPanelPrecisionConfigure &&
		moduleID <= kVSPanelInkConfigure) {
		VSModuleID module = kVSPrecisionPen;
		if (moduleID == kVSPanelFluidConfigure) module = kVSFluidSketch;
		if (moduleID == kVSPanelInkConfigure) module = kVSInkStudio;
		const int toolIndex = ToolIndexForModule(module);
		if (toolIndex >= 0 && fToolHandle[toolIndex]) {
			sAITool->SetSelectedTool(fToolHandle[toolIndex]);
		}
		return;
	}
	if (moduleID == kVSPanelTextureConfigure ||
		moduleID == kVSPanelStippleConfigure) {
		const VSModuleID module = moduleID == kVSPanelTextureConfigure
			? kVSTextureLab
			: kVSStippleLab;
		const int toolIndex = ToolIndexForModule(module);
		if (toolIndex >= 0 && fToolHandle[toolIndex]) {
			sAITool->SetSelectedTool(fToolHandle[toolIndex]);
		}
		return;
	}
	if (moduleID == kVSPanelGeometryConfigure ||
		moduleID == kVSPanelShapeConfigure) {
		const VSModuleID module = moduleID == kVSPanelGeometryConfigure
			? kVSGeometryLab
			: kVSShapeReform;
		const int toolIndex = ToolIndexForModule(module);
		if (toolIndex >= 0 && fToolHandle[toolIndex]) {
			sAITool->SetSelectedTool(fToolHandle[toolIndex]);
		}
		return;
	}

	if (moduleID < 0 || moduleID >= kVSModuleCount) return;
	const VSModuleID module = static_cast<VSModuleID>(moduleID);
	switch (module) {
		case kVSVectorRepair:
		{
			if (module == kVSVectorRepair) {
				sAIUndo->SetUndoRedoCmdTextUS(
					ai::UnicodeString("Annulla Vector Repair"),
					ai::UnicodeString("Ripeti Vector Repair"),
					ai::UnicodeString("Vector Repair"));
			}
			const ASErr error = ExecuteModuleCommand(module);
			if (error && sAIUser) {
				sAIUser->MessageAlert(ai::UnicodeString(
					"Vector Suite: il comando non è stato completato."));
			}
			return;
		}
		default:
			break;
	}
	if (module == kVSSmartFind) return;

	const int toolIndex = ToolIndexForModule(module);
	if (toolIndex >= 0 && fToolHandle[toolIndex]) {
		sAITool->SetSelectedTool(fToolHandle[toolIndex]);
	}
}

ASErr VectorSuitePlugin::AddAutoSaveTimer(SPInterfaceMessage* message)
{
	if (!sAITimer) return kNoErr;
	ASErr error = sAITimer->AddTimer(
		message->d.self,
		"Vector Suite Auto Save",
		5 * 60 * kTicksPerSecond,
		&fAutoSaveTimer);
	if (error) return error;
	return sAITimer->SetTimerActive(fAutoSaveTimer, false);
}

ASErr VectorSuitePlugin::ConfigureAutoSave(const VSAutoSaveSettings& rawSettings)
{
	if (!fAutoSaveTimer || !sAITimer) return kNoErr;
	const VSAutoSaveSettings settings = VSSanitizeAutoSave(rawSettings);
	ASErr error = sAITimer->SetTimerPeriod(
		fAutoSaveTimer,
		settings.intervalMinutes * 60 * kTicksPerSecond);
	if (error) return error;
	return sAITimer->SetTimerActive(fAutoSaveTimer, settings.enabled != 0);
}

ASErr VectorSuitePlugin::CreateAutoSaveVersionCopy()
{
	ai::FilePath documentFile;
	ASErr error = sAIDocument->GetDocumentFileSpecification(documentFile);
	if (error || documentFile.IsEmpty()) return error;

	try {
		const std::filesystem::path source = std::filesystem::u8path(
			documentFile.GetFullPath().as_UTF8());
		if (!std::filesystem::is_regular_file(source)) return kNoErr;
		const std::filesystem::path backupDirectory =
			source.parent_path() / "Vector Suite Backups";
		std::filesystem::create_directories(backupDirectory);
		const std::uint64_t timestamp = static_cast<std::uint64_t>(
			std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::system_clock::now().time_since_epoch()).count());
		std::string extension = source.extension().string();
		if (!extension.empty() && extension.front() == '.') extension.erase(0, 1);
		const std::filesystem::path destination = backupDirectory /
			std::filesystem::u8path(VSBackupFileName(
				source.stem().string(),
				extension,
				timestamp));
		std::filesystem::copy_file(
			source,
			destination,
			std::filesystem::copy_options::overwrite_existing);
	}
	catch (const std::filesystem::filesystem_error&) {
		return kCantHappenErr;
	}
	return kNoErr;
}

ASErr VectorSuitePlugin::PerformAutoSave(AIBoolean interactive)
{
	if (fApplicationShuttingDown || !sAIDocument || !sAIActionManager) return kNoErr;
	AIDocumentHandle document = nullptr;
	ASErr error = sAIDocument->GetDocument(&document);
	if (error || !document) return kNoErr;
	AIBoolean exists = false;
	error = sAIDocument->DocumentExists(document, &exists);
	if (error || !exists) return kNoErr;

	const VSAutoSaveSettings settings = VSAutoSaveGet();
	if (!interactive && settings.modifiedOnly) {
		AIBoolean modified = false;
		error = sAIDocument->GetDocumentModified(&modified);
		if (error || !modified) return error;
	}

	ai::FilePath documentFile;
	error = sAIDocument->GetDocumentFileSpecification(documentFile);
	if (error || documentFile.IsEmpty()) {
		if (!interactive) return kNoErr;
		return sAIActionManager->PlayActionEvent(
			kAISaveDocumentAction,
			kDialogOn,
			nullptr);
	}

	error = sAIActionManager->PlayActionEvent(
		kAISaveDocumentAction,
		kDialogOff,
		nullptr);
	if (!error && settings.createVersionCopy) {
		error = CreateAutoSaveVersionCopy();
	}
	return error;
}

ASErr VectorSuitePlugin::ExecuteWorkflowCommand(int command)
{
	switch (command) {
		case kVSPanelAutoSaveConfigure:
			return ConfigureAutoSave(VSAutoSaveGet());
		case kVSPanelAutoSaveNow:
			return PerformAutoSave(true);
		default:
			return kBadParameterErr;
	}
}

ASErr VectorSuitePlugin::GoTimer(AITimerMessage* message)
{
	if (!message || message->timer != fAutoSaveTimer) return kNoErr;
	if (fApplicationShuttingDown) return kNoErr;
	const VSAutoSaveSettings settings = VSAutoSaveGet();
	if (!settings.enabled) return kNoErr;
	return PerformAutoSave(false);
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
	if (index < 0) return kNoErr;

	// Track() non serve soltanto a correggere la coordinata: è anche il punto
	// d'ingresso con cui Illustrator mostra Smart Guides, snap a punti, griglia
	// e bordi tavola per un tool di terze parti. Chiamarlo durante il semplice
	// movimento del cursore ripristina quindi le annotazioni prima del drag.
	AIRealPoint ignored = message->cursor;
	SnapCursor(message, ignored);
	return sAIUser
		? sAIUser->SetSVGCursor(kVSTools[index].iconResourceID, fResourceManagerHandle)
		: kNoErr;
}

ASErr VectorSuitePlugin::SnapCursor(
	AIToolMessage* message,
	AIRealPoint& snappedPoint)
{
	fHasCustomSnap = false;
	if (!message || !message->event || !sAICursorSnap || !sAIDocumentView) {
		return kNoErr;
	}

	AIDocumentViewHandle view = nullptr;
	if (sAIDocumentView->GetNthDocumentView(0, &view) != kNoErr || !view) {
		return kNoErr;
	}

	AIRealPoint candidate = message->cursor;
	const VSSnap::Settings settings = VSSnapGet();
	if (settings.enabled) {
		AIDocumentHandle document=nullptr;
		if(sAIDocument)sAIDocument->GetDocument(&document);
		if (!fGestureActive&&(fSnapGeometryDirty||document!=fSnapDocument||settings.modes!=fSnapGeometryModes)) {
			RefreshSnapGeometry();fSnapGeometryDirty=false;
			fSnapDocument=document;fSnapGeometryModes=settings.modes;
		}
		AIReal zoom=1;
		if (sAIDocumentView->GetDocumentViewZoom(view,&zoom)) zoom=1;
		const VSSnap::Point origin={fStartingPoint.h,fStartingPoint.v};
		const bool shift=(message->event->modifiers & aiEventModifiers_shiftKey)!=0;
		const auto hit=VSSnap::find(fSnapGeometry,{candidate.h,candidate.v},
			fGestureActive?&origin:nullptr,settings,zoom,shift);
		if (hit.found) {
			ai::AutoBuffer<AICursorConstraint> constraints(1);
			constraints[0]=AICursorConstraint(kPointConstraint,0,{hit.point.x,hit.point.y},0,
				ai::UnicodeString(VSSnap::label(hit.mode)),nullptr);
			if (!sAICursorSnap->SetCustom(constraints)) {
				sAICursorSnap->Track(view,message->cursor,message->event,"T v",&candidate);
			}
			snappedPoint={hit.point.x,hit.point.y};
			fHasCustomSnap=true;
			return kNoErr;
		}
	}
	sAICursorSnap->ClearCustom();
	const ASErr error = sAICursorSnap->Track(
		view,
		message->cursor,
		message->event,
		"ATFPLMG v i o",
		&candidate);
	if (!error) snappedPoint = candidate;

	// Lo snap è un aiuto, non deve mai rendere inutilizzabile lo strumento se
	// una vista sta cambiando o Illustrator non ha ancora inizializzato le
	// Smart Guides del documento.
	return kNoErr;
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
	if (sAICursorSnap) sAICursorSnap->Reset();
	fGestureActive = false;
	fSnapGeometryDirty = true;
	fStartingPoint = message->cursor;
	SnapCursor(message, fStartingPoint);
	fEndPoint = fStartingPoint;
	fGesturePoints.clear();
	fGesturePoints.push_back(fStartingPoint);
	fGestureToolIndex = index;
	fGestureActive = true;
	ASErr error = IsProjectionTool(index)
		? sAIAnnotator->SetAnnotatorActive(fAnnotatorHandle, true)
		: BeginModuleGesture(message, index);
	if (error) {
		fGestureActive = false;
		fGestureToolIndex = -1;
	}
	return error;
}

ASErr VectorSuitePlugin::ToolMouseDrag(AIToolMessage* message)
{
	const int index = ToolIndex(message->tool);
	if (!fGestureActive || index != fGestureToolIndex) return kNoErr;
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
	const ASErr invalidationError = InvalidateRect(oldAnnotatorRect);
	fGestureActive = false;
	fGestureToolIndex = -1;
	ASErr error = IsProjectionTool(index)
		? sAIAnnotator->SetAnnotatorActive(fAnnotatorHandle, false)
		: EndModuleGesture(message, index);
	if (!error) error = invalidationError;
	if (sAICursorSnap) {
		const ASErr resetError = sAICursorSnap->Reset();
		if (!error) error = resetError;
	}
	return error;
}

ASErr VectorSuitePlugin::DeselectTool(AIToolMessage* message)
{
	fGestureActive=false;
	fGestureToolIndex=-1;
	fSnapGeometry={};
	fSnapGeometryDirty=true;
	fHasCustomSnap=false;
	if (sAICursorSnap) sAICursorSnap->ClearCustom();
	return Plugin::DeselectTool(message);
}

ASErr VectorSuitePlugin::AddNotifier(SPInterfaceMessage* message)
{
	ASErr error = sAINotifier->AddNotifier(
		fPluginRef,
		"Vector Suite Shutdown",
		kAIApplicationShutdownNotifier,
		&fShutdownApplicationNotifier);
	if (error) return error;

	error = sAINotifier->AddNotifier(
		fPluginRef,
		"Vector Suite Selection",
		kAIArtSelectionChangedNotifier,
		&fNotifySelectionChanged);
	if(error)return error;
	error=sAINotifier->AddNotifier(fPluginRef,"Vector Suite Snap Art",kAIArtObjectsChangedNotifier,&fSnapArtChanged);
	if(error)return error;
	return sAINotifier->AddNotifier(fPluginRef,"Vector Suite Snap Document",kAIDocumentChangedNotifier,&fSnapDocumentChanged);
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
	AIRect moduleBounds = {0, 0, 0, 0};
	ASErr error = DrawModuleAnnotation(message, moduleBounds);
	if (error) return error;

	ai::UnicodeString pointString;
	error = GetPointString(fEndPoint, pointString);
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
	if (!error) {
		const bool hasModuleBounds =
			moduleBounds.left != moduleBounds.right || moduleBounds.top != moduleBounds.bottom;
		if (hasModuleBounds) {
			bounds.left = std::min(bounds.left, moduleBounds.left);
			bounds.top = std::min(bounds.top, moduleBounds.top);
			bounds.right = std::max(bounds.right, moduleBounds.right);
			bounds.bottom = std::max(bounds.bottom, moduleBounds.bottom);
		}
		oldAnnotatorRect = bounds;
	}
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
