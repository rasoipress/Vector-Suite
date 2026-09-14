//========================================================================================
//  
//  $File$
//
//  $Revision$
//
//  Copyright 1987 Adobe Systems Incorporated. All rights reserved.
//  
//  NOTICE:  Adobe permits you to use, modify, and distribute this file in accordance 
//  with the terms of the Adobe license agreement accompanying it.  If you have received
//  this file from a source other than Adobe, then your use, modification, or 
//  distribution of it requires the prior written permission of Adobe.
//  
//========================================================================================

#include "IllustratorSDK.h"
#include "VectorSuiteSuites.h"

extern "C" {
	AIMenuSuite*			sAIMenu = NULL;
	AIToolSuite*			sAITool = NULL;
	AIUnicodeStringSuite*	sAIUnicodeString = NULL;
	AIPluginSuite*			sAIPlugin = NULL;
	SPBlocksSuite*			sSPBlocks = NULL;
	AIUndoSuite*			sAIUndo = NULL;
	AIArtSuite*				sAIArt = NULL;
	AIRealMathSuite*		sAIRealMath = NULL;
	AIPathSuite*			sAIPath = NULL;
	AIPathStyleSuite*		sAIPathStyle = NULL;
	AIAnnotatorSuite*		sAIAnnotator = NULL;
	AIAnnotatorDrawerSuite* sAIAnnotatorDrawer = NULL;
	AICursorSnapSuite*		sAICursorSnap = NULL;
	AIDocumentViewSuite*	sAIDocumentView = NULL;
	AIStringFormatUtilsSuite*	sAIStringFormatUtils = NULL;
	AIPanelSuite*			sAIPanel = NULL;
	AIPanelFlyoutMenuSuite*	sAIPanelFlyoutMenu = NULL;
	AIMatchingArtSuite*		sAIMatchingArt = NULL;
	AIMdMemorySuite*		sAIMdMemory = NULL;
	AITransformArtSuite*	sAITransformArt = NULL;
	AIBlendStyleSuite*		sAIBlendStyle = NULL;
	AIActionManagerSuite*	sAIActionManager = NULL;
	AIArtboardSuite*		sAIArtboard = NULL;
	AIAssertionSuite*		sAIAssertion = NULL;
	AIDictionarySuite*		sAIDictionary = NULL;
	AITimerSuite*			sAITimer = NULL;
	AIDocumentSuite*		sAIDocument = NULL;
	AIRasterSuite*			sAIRaster = NULL;
	AIPlacedSuite*			sAIPlaced = NULL;
}

ImportSuite gImportSuites[] = {
	kAIMenuSuite, kAIMenuSuiteVersion, &sAIMenu,
	kAIToolSuite, kAIToolVersion, &sAITool,
	kAIUnicodeStringSuite, kAIUnicodeStringVersion, &sAIUnicodeString,
	kAIPluginSuite, kAIPluginSuiteVersion, &sAIPlugin,
	kSPBlocksSuite, kSPBlocksSuiteVersion, &sSPBlocks,
	kAIUndoSuite, kAIUndoSuiteVersion, &sAIUndo,
	kAIArtSuite, kAIArtVersion, &sAIArt,
	kAIRealMathSuite, kAIRealMathVersion, &sAIRealMath,	
	kAIPathSuite, kAIPathSuiteVersion, &sAIPath,
	kAIPathStyleSuite, kAIPathStyleSuiteVersion, &sAIPathStyle,
	kAIAnnotatorSuite, kAIAnnotatorSuiteVersion, &sAIAnnotator,
	kAIAnnotatorDrawerSuite, kAIAnnotatorDrawerSuiteVersion, &sAIAnnotatorDrawer,
	kAICursorSnapSuite, kAICursorSnapSuiteVersion, &sAICursorSnap,
	kAIDocumentViewSuite, kAIDocumentViewSuiteVersion, &sAIDocumentView,
	kAIStringFormatUtilsSuite, kAIStringFormatUtilsSuiteVersion, &sAIStringFormatUtils,
	kAIPanelSuite, kAIPanelSuiteVersion, &sAIPanel,
	kAIPanelFlyoutMenuSuite, kAIPanelFlyoutMenuSuiteVersion, &sAIPanelFlyoutMenu,
	kAIMatchingArtSuite, kAIMatchingArtSuiteVersion, &sAIMatchingArt,
	kAIMdMemorySuite, kAIMdMemorySuiteVersion, &sAIMdMemory,
	kAITransformArtSuite, kAITransformArtSuiteVersion, &sAITransformArt,
	kAIBlendStyleSuite, kAIBlendStyleSuiteVersion, &sAIBlendStyle,
	kAIActionManagerSuite, kAIActionManagerSuiteVersion, &sAIActionManager,
	kAIArtboardSuite, kAIArtboardSuiteVersion, &sAIArtboard,
	kAIAssertionSuite, kAIAssertionSuiteVersion, &sAIAssertion,
	kAIDictionarySuite, kAIDictionarySuiteVersion, &sAIDictionary,
	kAITimerSuite, kAITimerSuiteVersion, &sAITimer,
	kAIDocumentSuite, kAIDocumentSuiteVersion, &sAIDocument,
	// Illustrator 30.6 pubblica ancora la versione 10 di AIRaster. Le funzioni
	// usate da Raster Lab (Get/SetRasterInfo) appartengono al prefisso ABI
	// stabile della suite. AIRasterize viene invece pubblicata dopo lo startup
	// e viene acquisita al momento del comando.
	kAIRasterSuite, AIAPI_VERSION(10), &sAIRaster,
	kAIPlacedSuite, kAIPlacedSuiteVersion, &sAIPlaced,
	nullptr, 0, nullptr
};

// End VectorSuiteSuites.cpp
