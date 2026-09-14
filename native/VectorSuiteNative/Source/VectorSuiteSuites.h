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

#ifndef VECTOR_SUITE_SUITES_H
#define VECTOR_SUITE_SUITES_H

#include "AIRasterize.h"
#include "AIPlaced.h"

#include "IllustratorSDK.h"
#include "Suites.hpp"
#include "AIAnnotator.h"
#include "AIAnnotatorDrawer.h"
#include "AICursorSnap.h"
#include "AIActionManager.h"
#include "AIArtboard.h"
#include "AIAssertion.h"
#include "AIDictionary.h"
#include "AIMask.h"
#include "AIMatchingArt.h"
#include "AIMdMemory.h"
#include "AIPanel.h"
#include "AIStringFormatUtils.h"
#include "AITransformArt.h"

extern  "C" SPBasicSuite*          sSPBasic;
extern	"C"	AIMenuSuite*			sAIMenu;
extern	"C"	AIToolSuite*			sAITool;
extern	"C"	AIUnicodeStringSuite*	sAIUnicodeString;
extern	"C"	AIPluginSuite*			sAIPlugin;
extern	"C"	SPBlocksSuite*			sSPBlocks;
extern	"C"	AIUndoSuite*			sAIUndo;
extern	"C"	AIArtSuite*				sAIArt;
extern	"C"	AIRealMathSuite*		sAIRealMath;
extern	"C"	AIPathSuite*			sAIPath;
extern	"C"	AIPathStyleSuite*		sAIPathStyle;
extern  "C" AIAnnotatorSuite*		sAIAnnotator;
extern  "C" AIAnnotatorDrawerSuite* sAIAnnotatorDrawer;
extern  "C" AICursorSnapSuite*     sAICursorSnap;
extern  "C" AIDocumentViewSuite*	sAIDocumentView;
extern	"C" AIStringFormatUtilsSuite*	sAIStringFormatUtils;
extern  "C" AIPanelSuite*          sAIPanel;
extern  "C" AIPanelFlyoutMenuSuite* sAIPanelFlyoutMenu;
extern  "C" AIMatchingArtSuite*    sAIMatchingArt;
extern  "C" AIMdMemorySuite*       sAIMdMemory;
extern  "C" AITransformArtSuite*   sAITransformArt;
extern  "C" AIBlendStyleSuite*     sAIBlendStyle;
extern  "C" AIActionManagerSuite*  sAIActionManager;
extern  "C" AIArtboardSuite*       sAIArtboard;
extern  "C" AIAssertionSuite*      sAIAssertion;
extern  "C" AIDictionarySuite*     sAIDictionary;
extern  "C" AITimerSuite*          sAITimer;
extern  "C" AIDocumentSuite*       sAIDocument;
extern  "C" AIRasterSuite*         sAIRaster;
extern  "C" AIPlacedSuite*         sAIPlaced;

#endif
