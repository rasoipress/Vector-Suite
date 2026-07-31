// Pannello nativo di Vector Suite dentro Adobe Illustrator.
//
// Il pannello adotta lo stesso sistema visivo dell'app di gestione: marchio
// isometrico, pastiglie di icona invertite, stessa scala tipografica, stessi
// raggi. Le tinte non sono costanti ma colori dinamici di sistema, così il
// pannello segue il tema chiaro o scuro di macOS e di Illustrator senza
// introdurre un solo colore oltre a inchiostro, carta e i grigi che ne
// derivano: i controlli sono ridisegnati proprio per non ereditare il colore
// di accento del sistema.

#import <Cocoa/Cocoa.h>

#include "VectorSuiteCatalog.h"
#include "VectorSuitePanel.h"

#include <cmath>

#pragma mark - Sistema visivo

namespace {

/// Inchiostro: testo primario e superfici piene.
NSColor* VSInk() { return NSColor.labelColor; }

/// Carta: fondo delle superfici e glifi sopra l'inchiostro.
NSColor* VSPaper() { return NSColor.controlBackgroundColor; }

/// Testo di supporto.
NSColor* VSMuted() { return NSColor.secondaryLabelColor; }

/// Etichette e valori di servizio.
NSColor* VSQuiet() { return NSColor.tertiaryLabelColor; }

/// Filetti e contorni a riposo.
NSColor* VSLine() { return NSColor.separatorColor; }

const CGFloat kVSCardHeight = 96.0;
const CGFloat kVSCardRadius = 10.0;
const CGFloat kVSTileSide = 30.0;
const CGFloat kVSTileRadius = 8.0;
const CGFloat kVSGutter = 8.0;
const CGFloat kVSMargin = 14.0;

NSFont* VSFontTitle()   { return [NSFont systemFontOfSize:13 weight:NSFontWeightSemibold]; }
NSFont* VSFontCard()    { return [NSFont systemFontOfSize:12 weight:NSFontWeightSemibold]; }
NSFont* VSFontBody()    { return [NSFont systemFontOfSize:11 weight:NSFontWeightRegular]; }
NSFont* VSFontSmall()   { return [NSFont systemFontOfSize:10 weight:NSFontWeightRegular]; }
NSFont* VSFontEyebrow() { return [NSFont systemFontOfSize:10 weight:NSFontWeightSemibold]; }
NSFont* VSFontMono()    { return [NSFont monospacedDigitSystemFontOfSize:11
                                                                 weight:NSFontWeightRegular]; }

/// Etichetta con sfondo trasparente e nessun bordo.
NSTextField* VSLabel(NSString* text, NSFont* font, NSColor* color)
{
	NSTextField* label = [NSTextField labelWithString:text];
	label.font = font;
	label.textColor = color;
	label.translatesAutoresizingMaskIntoConstraints = NO;
	return label;
}

/// Sezione in maiuscoletto spaziato, come nell'app di gestione.
NSTextField* VSEyebrow(NSString* text)
{
	NSMutableParagraphStyle* paragraph = [[NSMutableParagraphStyle alloc] init];
	NSDictionary* attributes = @{
		NSFontAttributeName: VSFontEyebrow(),
		NSForegroundColorAttributeName: VSQuiet(),
		NSKernAttributeName: @0.6,
		NSParagraphStyleAttributeName: paragraph
	};
	NSTextField* label = [NSTextField labelWithAttributedString:
		[[NSAttributedString alloc] initWithString:text.uppercaseString
										attributes:attributes]];
	label.translatesAutoresizingMaskIntoConstraints = NO;
	return label;
}

}  // namespace

#pragma mark - Marchio

// Ricalco del marchio disegnato in Resources/VectorSuiteLogo.svg: cubo
// isometrico con le tre facce separate dalle fenditure, e sul vertice centrale
// le due maniglie di direzione con i pomelli vuoti. Le coordinate sono quelle
// del file, sulla sua scatola di 896 unità, così un confronto fra i due è
// immediato: se il disegno cambia, qui vanno riportati gli stessi numeri.
//
//   sagoma      esagono (448,79.21) (767.38,263.61) (767.38,632.4)
//               (448,816.79) (128.62,632.4) (128.62,263.61)
//   fenditure   dal centro (448,448) ai tre vertici, spessore 30
//   maniglie    (279.43,545.32) → (448,448) → (616.57,545.32), spessore 20
//   pomelli     raggio 43.03, pieni nel colore della sagoma e cerchiati nel
//               colore di fondo con lo stesso spessore delle maniglie

static const CGFloat kVSMarkBox = 896.0;

static void VSDrawMark(NSRect box, NSColor* foreground, NSColor* background)
{
	const CGFloat side = MIN(NSWidth(box), NSHeight(box));
	const CGFloat scale = side / kVSMarkBox;
	const CGFloat originX = NSMinX(box) + (NSWidth(box) - side) * 0.5;
	const CGFloat originY = NSMinY(box) + (NSHeight(box) - side) * 0.5;

	auto point = [&](CGFloat x, CGFloat y) {
		return NSMakePoint(originX + x * scale, originY + y * scale);
	};

	NSBezierPath* solid = [NSBezierPath bezierPath];
	[solid moveToPoint:point(448, 79.21)];
	[solid lineToPoint:point(767.38, 263.61)];
	[solid lineToPoint:point(767.38, 632.4)];
	[solid lineToPoint:point(448, 816.79)];
	[solid lineToPoint:point(128.62, 632.4)];
	[solid lineToPoint:point(128.62, 263.61)];
	[solid closePath];
	[foreground setFill];
	[solid fill];

	[background setStroke];

	NSBezierPath* seams = [NSBezierPath bezierPath];
	[seams moveToPoint:point(448, 448)];
	[seams lineToPoint:point(767.38, 263.61)];
	[seams moveToPoint:point(448, 448)];
	[seams lineToPoint:point(128.62, 263.61)];
	[seams moveToPoint:point(448, 448)];
	[seams lineToPoint:point(448, 816.79)];
	seams.lineWidth = 30.0 * scale;
	seams.lineCapStyle = NSLineCapStyleButt;
	[seams stroke];

	const NSPoint knobCentres[2] = { point(279.43, 545.32), point(616.57, 545.32) };

	NSBezierPath* handles = [NSBezierPath bezierPath];
	[handles moveToPoint:knobCentres[0]];
	[handles lineToPoint:point(448, 448)];
	[handles lineToPoint:knobCentres[1]];
	handles.lineWidth = 20.0 * scale;
	handles.lineCapStyle = NSLineCapStyleButt;
	handles.lineJoinStyle = NSLineJoinStyleMiter;
	[handles stroke];

	// I pomelli sono vuoti: pieni nel colore della sagoma e cerchiati nel colore
	// di fondo. È così che Illustrator disegna le maniglie di direzione.
	const CGFloat knob = 43.03 * scale;
	for (NSInteger index = 0; index < 2; ++index) {
		NSRect knobRect = NSMakeRect(knobCentres[index].x - knob,
									 knobCentres[index].y - knob,
									 knob * 2,
									 knob * 2);
		NSBezierPath* disc = [NSBezierPath bezierPathWithOvalInRect:knobRect];
		[foreground setFill];
		[disc fill];
		[background setStroke];
		disc.lineWidth = 20.0 * scale;
		[disc stroke];
	}
}

@interface VSBrandMarkView : NSView
@end

@implementation VSBrandMarkView

- (BOOL)isFlipped
{
	return YES;
}

- (void)drawRect:(NSRect)dirtyRect
{
	NSRect tile = self.bounds;
	NSBezierPath* plate = [NSBezierPath bezierPathWithRoundedRect:tile
														 xRadius:9.0
														 yRadius:9.0];
	[VSInk() setFill];
	[plate fill];

	// Il disegno ha già un suo margine interno: qui basta un rientro del 10%
	// perché il cubo occupi circa i due terzi della pastiglia, come nelle
	// pastiglie dei moduli.
	const CGFloat inset = NSWidth(tile) * 0.10;
	VSDrawMark(NSInsetRect(tile, inset, inset), VSPaper(), VSInk());
}

- (void)viewDidChangeEffectiveAppearance
{
	[super viewDidChangeEffectiveAppearance];
	[self setNeedsDisplay:YES];
}

@end

#pragma mark - Contenitore

@protocol VSAppearanceObserving <NSObject>
- (void)appearanceDidChange;
@end

@interface VSPanelRootView : NSView
@property(nonatomic, weak) id<VSAppearanceObserving> appearanceObserver;
@end

@implementation VSPanelRootView

- (void)drawRect:(NSRect)dirtyRect
{
	[NSColor.windowBackgroundColor setFill];
	NSRectFill(dirtyRect);
}

- (void)viewDidChangeEffectiveAppearance
{
	[super viewDidChangeEffectiveAppearance];
	[self setNeedsDisplay:YES];
	[self.appearanceObserver appearanceDidChange];
}

@end

#pragma mark - Scheda di modulo

@protocol VSModuleCardReordering <NSObject>
- (void)moveModuleID:(NSInteger)moduleID toWindowPoint:(NSPoint)windowPoint;
@end

@interface VSModuleCardButton : NSButton
@property(nonatomic, assign) NSInteger moduleID;
@property(nonatomic, copy) NSString* moduleName;
@property(nonatomic, copy) NSString* moduleSummary;
@property(nonatomic, strong) NSImage* moduleImage;
@property(nonatomic, assign) BOOL selectedCard;
@property(nonatomic, weak) id<VSModuleCardReordering> reorderDelegate;
@end

@implementation VSModuleCardButton

- (BOOL)isFlipped
{
	return YES;
}

- (BOOL)isOpaque
{
	return NO;
}

- (void)setSelectedCard:(BOOL)selectedCard
{
	_selectedCard = selectedCard;
	[self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect
{
	NSRect cardRect = NSInsetRect(self.bounds, 0.75, 0.75);
	NSBezierPath* card = [NSBezierPath bezierPathWithRoundedRect:cardRect
														xRadius:kVSCardRadius
														yRadius:kVSCardRadius];
	[VSPaper() setFill];
	[card fill];
	[(self.selectedCard ? VSInk() : VSLine()) setStroke];
	card.lineWidth = self.selectedCard ? 1.5 : 1.0;
	[card stroke];

	// Pastiglia dell'icona: inchiostro pieno, glifo in carta. È la stessa
	// inversione che l'app di gestione applica alle sue schede.
	NSRect tile = NSMakeRect(12, 12, kVSTileSide, kVSTileSide);
	NSBezierPath* plate = [NSBezierPath bezierPathWithRoundedRect:tile
														 xRadius:kVSTileRadius
														 yRadius:kVSTileRadius];
	[VSInk() setFill];
	[plate fill];
	if (self.moduleImage) {
		NSRect glyph = NSInsetRect(tile, 5.0, 5.0);
		[self.moduleImage drawInRect:glyph
							fromRect:NSZeroRect
						   operation:NSCompositingOperationSourceOver
							fraction:1.0
					  respectFlipped:YES
							   hints:nil];
	}

	// Presa di trascinamento, allineata al centro verticale della pastiglia.
	[VSQuiet() setFill];
	for (NSInteger dot = 0; dot < 3; ++dot) {
		NSRect dotRect = NSMakeRect(NSWidth(self.bounds) - 15,
									NSMidY(tile) - 5 + dot * 4,
									2.0,
									2.0);
		[[NSBezierPath bezierPathWithOvalInRect:dotRect] fill];
	}

	NSMutableParagraphStyle* tight = [[NSMutableParagraphStyle alloc] init];
	tight.lineBreakMode = NSLineBreakByTruncatingTail;
	[self.moduleName drawInRect:NSMakeRect(12, 50, NSWidth(self.bounds) - 24, 16)
				 withAttributes:@{
					 NSFontAttributeName: VSFontCard(),
					 NSForegroundColorAttributeName: VSInk(),
					 NSParagraphStyleAttributeName: tight
				 }];

	NSMutableParagraphStyle* wrapped = [[NSMutableParagraphStyle alloc] init];
	wrapped.lineBreakMode = NSLineBreakByTruncatingTail;
	wrapped.lineSpacing = 1.0;
	[self.moduleSummary drawInRect:NSMakeRect(12, 68, NSWidth(self.bounds) - 24, 26)
					withAttributes:@{
						NSFontAttributeName: VSFontSmall(),
						NSForegroundColorAttributeName: VSMuted(),
						NSParagraphStyleAttributeName: wrapped
					}];
}

- (void)mouseDown:(NSEvent*)event
{
	if ((event.modifierFlags & NSEventModifierFlagControl) == 0) {
		[super mouseDown:event];
		return;
	}

	NSPoint initialPoint = event.locationInWindow;
	NSPoint finalPoint = initialPoint;
	BOOL moved = NO;
	self.alphaValue = 0.56;
	[NSCursor.closedHandCursor push];

	while (YES) {
		NSEvent* nextEvent = [self.window
			nextEventMatchingMask:(NSEventMaskLeftMouseDragged | NSEventMaskLeftMouseUp)];
		if (nextEvent.type == NSEventTypeLeftMouseDragged) {
			finalPoint = nextEvent.locationInWindow;
			if (std::hypot(
				finalPoint.x - initialPoint.x,
				finalPoint.y - initialPoint.y) > 4.0) {
				moved = YES;
			}
		}
		else {
			finalPoint = nextEvent.locationInWindow;
			break;
		}
	}

	[NSCursor pop];
	self.alphaValue = 1.0;
	if (moved) {
		[self.reorderDelegate moveModuleID:self.moduleID toWindowPoint:finalPoint];
	}
}

@end

#pragma mark - Controlli monocromatici

// I controlli standard di AppKit si tingono del colore di accento scelto
// dall'utente. Qui vengono ridisegnati: comportamento invariato, tinte ridotte
// a inchiostro, carta e filetto.

@interface VSSliderCell : NSSliderCell
@end

@implementation VSSliderCell

- (NSRect)barRectFlipped:(BOOL)flipped
{
	NSRect bar = [super barRectFlipped:flipped];
	const CGFloat height = 3.0;
	return NSMakeRect(NSMinX(bar), NSMidY(bar) - height * 0.5, NSWidth(bar), height);
}

- (NSRect)knobRectFlipped:(BOOL)flipped
{
	NSRect knob = [super knobRectFlipped:flipped];
	const CGFloat side = 12.0;
	return NSMakeRect(NSMidX(knob) - side * 0.5, NSMidY(knob) - side * 0.5, side, side);
}

- (void)drawBarInside:(NSRect)rect flipped:(BOOL)flipped
{
	NSBezierPath* track = [NSBezierPath bezierPathWithRoundedRect:rect
														  xRadius:NSHeight(rect) * 0.5
														  yRadius:NSHeight(rect) * 0.5];
	[VSLine() setFill];
	[track fill];

	const double span = self.maxValue - self.minValue;
	if (span <= 0.0) return;
	const double ratio = MAX(0.0, MIN(1.0, (self.doubleValue - self.minValue) / span));
	NSRect filled = rect;
	filled.size.width = NSWidth(rect) * ratio;
	if (NSWidth(filled) <= 0.0) return;

	NSBezierPath* progress = [NSBezierPath bezierPathWithRoundedRect:filled
															xRadius:NSHeight(rect) * 0.5
															yRadius:NSHeight(rect) * 0.5];
	[VSInk() setFill];
	[progress fill];
}

- (void)drawKnob:(NSRect)knobRect
{
	NSBezierPath* knob = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(knobRect, 1.0, 1.0)];
	[VSPaper() setFill];
	[knob fill];
	[VSInk() setStroke];
	knob.lineWidth = 1.5;
	[knob stroke];
}

@end

@interface VSCheckbox : NSButton
@end

@implementation VSCheckbox

- (BOOL)isFlipped
{
	return YES;
}

- (void)setState:(NSControlStateValue)state
{
	[super setState:state];
	[self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect
{
	const CGFloat side = 13.0;
	NSRect box = NSMakeRect(0.5, NSMidY(self.bounds) - side * 0.5, side, side);
	NSBezierPath* frame = [NSBezierPath bezierPathWithRoundedRect:box
														  xRadius:3.5
														  yRadius:3.5];
	if (self.state == NSControlStateValueOn) {
		[VSInk() setFill];
		[frame fill];

		// Spunta a 45°, la stessa dell'app di gestione.
		NSBezierPath* tick = [NSBezierPath bezierPath];
		[tick moveToPoint:NSMakePoint(NSMinX(box) + 3.2, NSMidY(box))];
		[tick lineToPoint:NSMakePoint(NSMinX(box) + 5.4, NSMidY(box) + 2.2)];
		[tick lineToPoint:NSMakePoint(NSMinX(box) + 9.8, NSMidY(box) - 2.2)];
		tick.lineWidth = 1.6;
		tick.lineCapStyle = NSLineCapStyleRound;
		tick.lineJoinStyle = NSLineJoinStyleRound;
		[VSPaper() setStroke];
		[tick stroke];
	}
	else {
		[VSPaper() setFill];
		[frame fill];
		[VSLine() setStroke];
		frame.lineWidth = 1.0;
		[frame stroke];
	}

	NSMutableParagraphStyle* paragraph = [[NSMutableParagraphStyle alloc] init];
	paragraph.lineBreakMode = NSLineBreakByTruncatingTail;
	NSDictionary* attributes = @{
		NSFontAttributeName: VSFontBody(),
		NSForegroundColorAttributeName: self.enabled ? VSInk() : VSQuiet(),
		NSParagraphStyleAttributeName: paragraph
	};
	const CGFloat textHeight = [self.title sizeWithAttributes:attributes].height;
	[self.title drawInRect:NSMakeRect(NSMaxX(box) + 7,
									  NSMidY(self.bounds) - textHeight * 0.5,
									  NSWidth(self.bounds) - NSMaxX(box) - 7,
									  textHeight)
			withAttributes:attributes];
}

@end

/// Selettore a segmenti ridisegnato da zero: nessun colore di accento, e il
/// comportamento resta quello atteso perché la mappatura dei segmenti è una
/// semplice divisione in parti uguali.
@interface VSSegmentedControl : NSControl
@property(nonatomic, copy) NSArray<NSString*>* labels;
@property(nonatomic, assign) NSInteger selectedSegment;
@end

@implementation VSSegmentedControl

- (BOOL)isFlipped
{
	return YES;
}

- (instancetype)initWithLabels:(NSArray<NSString*>*)labels
						target:(id)target
						action:(SEL)action
{
	self = [super initWithFrame:NSZeroRect];
	if (self) {
		_labels = [labels copy];
		_selectedSegment = 0;
		self.target = target;
		self.action = action;
		self.translatesAutoresizingMaskIntoConstraints = NO;
		[self.heightAnchor constraintEqualToConstant:24].active = YES;
	}
	return self;
}

- (void)setSelectedSegment:(NSInteger)selectedSegment
{
	_selectedSegment = MAX(0, MIN((NSInteger)self.labels.count - 1, selectedSegment));
	[self setNeedsDisplay:YES];
}

- (NSRect)rectForSegment:(NSInteger)index
{
	const CGFloat width = NSWidth(self.bounds) / MAX(1u, (unsigned)self.labels.count);
	return NSMakeRect(index * width, 0, width, NSHeight(self.bounds));
}

- (void)drawRect:(NSRect)dirtyRect
{
	NSRect track = NSInsetRect(self.bounds, 0.5, 0.5);
	NSBezierPath* frame = [NSBezierPath bezierPathWithRoundedRect:track
														  xRadius:6.0
														  yRadius:6.0];
	[VSPaper() setFill];
	[frame fill];
	[VSLine() setStroke];
	frame.lineWidth = 1.0;
	[frame stroke];

	for (NSInteger index = 0; index < (NSInteger)self.labels.count; ++index) {
		NSRect segment = [self rectForSegment:index];
		const BOOL selected = index == self.selectedSegment;

		if (selected) {
			NSBezierPath* pill = [NSBezierPath
				bezierPathWithRoundedRect:NSInsetRect(segment, 2.0, 2.0)
								  xRadius:4.5
								  yRadius:4.5];
			[VSInk() setFill];
			[pill fill];
		}
		else if (index > 0) {
			// Separatore verticale solo fra due segmenti entrambi a riposo.
			const BOOL previousSelected = (index - 1) == self.selectedSegment;
			if (!previousSelected) {
				[VSLine() setFill];
				NSRectFill(NSMakeRect(NSMinX(segment), 5, 1, NSHeight(self.bounds) - 10));
			}
		}

		NSMutableParagraphStyle* centred = [[NSMutableParagraphStyle alloc] init];
		centred.alignment = NSTextAlignmentCenter;
		centred.lineBreakMode = NSLineBreakByTruncatingTail;
		NSDictionary* attributes = @{
			NSFontAttributeName: [NSFont systemFontOfSize:11
												   weight:selected ? NSFontWeightSemibold
																   : NSFontWeightRegular],
			NSForegroundColorAttributeName: selected ? VSPaper() : VSInk(),
			NSParagraphStyleAttributeName: centred
		};
		NSString* label = self.labels[index];
		const CGFloat textHeight = [label sizeWithAttributes:attributes].height;
		[label drawInRect:NSMakeRect(NSMinX(segment),
									 NSMidY(self.bounds) - textHeight * 0.5,
									 NSWidth(segment),
									 textHeight)
		   withAttributes:attributes];
	}
}

- (void)mouseDown:(NSEvent*)event
{
	NSPoint local = [self convertPoint:event.locationInWindow fromView:nil];
	const CGFloat width = NSWidth(self.bounds) / MAX(1u, (unsigned)self.labels.count);
	NSInteger index = (NSInteger)std::floor(local.x / MAX(1.0, width));
	index = MAX(0, MIN((NSInteger)self.labels.count - 1, index));
	if (index == self.selectedSegment) return;
	self.selectedSegment = index;
	[self sendAction:self.action to:self.target];
}

@end

/// Pulsante piatto in due varianti: pieno per l'azione principale, contornato
/// per le azioni di servizio.
@interface VSPushButton : NSButton
@property(nonatomic, assign) BOOL prominent;
@end

@implementation VSPushButton

- (BOOL)isFlipped
{
	return YES;
}

- (instancetype)initWithTitle:(NSString*)title
					prominent:(BOOL)prominent
					   target:(id)target
					   action:(SEL)action
{
	self = [super initWithFrame:NSZeroRect];
	if (self) {
		_prominent = prominent;
		self.title = title;
		self.target = target;
		self.action = action;
		self.bordered = NO;
		self.translatesAutoresizingMaskIntoConstraints = NO;
		[self.heightAnchor constraintEqualToConstant:prominent ? 30 : 24].active = YES;
	}
	return self;
}

- (void)drawRect:(NSRect)dirtyRect
{
	NSRect shape = NSInsetRect(self.bounds, 0.5, 0.5);
	NSBezierPath* frame = [NSBezierPath bezierPathWithRoundedRect:shape
														  xRadius:7.0
														  yRadius:7.0];
	if (self.prominent) {
		[VSInk() setFill];
		[frame fill];
	}
	else {
		[VSPaper() setFill];
		[frame fill];
		[VSLine() setStroke];
		frame.lineWidth = 1.0;
		[frame stroke];
	}

	NSMutableParagraphStyle* centred = [[NSMutableParagraphStyle alloc] init];
	centred.alignment = NSTextAlignmentCenter;
	centred.lineBreakMode = NSLineBreakByTruncatingTail;
	NSDictionary* attributes = @{
		NSFontAttributeName: [NSFont systemFontOfSize:self.prominent ? 12 : 11
											   weight:NSFontWeightMedium],
		NSForegroundColorAttributeName: self.prominent ? VSPaper() : VSInk(),
		NSParagraphStyleAttributeName: centred
	};
	const CGFloat textHeight = [self.title sizeWithAttributes:attributes].height;
	[self.title drawInRect:NSMakeRect(0,
									  NSMidY(self.bounds) - textHeight * 0.5,
									  NSWidth(self.bounds),
									  textHeight)
			withAttributes:attributes];

	if (self.isHighlighted) {
		[[NSColor colorWithWhite:0.5 alpha:0.22] setFill];
		[frame fill];
	}
}

@end

#pragma mark - Controller

@interface VSPanelController : NSViewController <NSSearchFieldDelegate, VSModuleCardReordering, VSAppearanceObserving>
@property(nonatomic, assign) VSPanelActivateToolProc activateTool;
@property(nonatomic, assign) VSPanelGenerateFractalProc generateFractal;
@property(nonatomic, assign) void* callbackContext;
@property(nonatomic, strong) NSView* hostView;
@property(nonatomic, strong) NSSearchField* searchField;
@property(nonatomic, strong) NSPopUpButton* categoryPopup;
@property(nonatomic, strong) NSTextField* countLabel;
@property(nonatomic, strong) NSScrollView* scrollView;
@property(nonatomic, strong) NSStackView* moduleStack;
@property(nonatomic, strong) NSMutableDictionary<NSNumber*, NSButton*>* moduleButtons;
@property(nonatomic, strong) NSMutableArray<NSNumber*>* moduleOrder;
@property(nonatomic, strong) NSMutableDictionary<NSString*, NSImage*>* glyphCache;
@property(nonatomic, strong) NSMutableDictionary<NSString*, NSSlider*>* fractalSliders;
@property(nonatomic, strong) NSMutableDictionary<NSString*, NSTextField*>* fractalValues;
@property(nonatomic, strong) VSSegmentedControl* waveControl;
@property(nonatomic, strong) NSButton* automaticToggle;
@property(nonatomic, strong) NSButton* advancedToggle;
@property(nonatomic, strong) NSButton* groupToggle;
@property(nonatomic, strong) NSButton* replaceToggle;
@property(nonatomic, strong) NSTextField* fractalStatus;
@property(nonatomic, assign) NSInteger selectedModule;
@end

@implementation VSPanelController

- (instancetype)initWithHostView:(NSView*)hostView
					activateTool:(VSPanelActivateToolProc)activateTool
				 generateFractal:(VSPanelGenerateFractalProc)generateFractal
						 context:(void*)context
{
	self = [super initWithNibName:nil bundle:nil];
	if (self) {
		_hostView = hostView;
		_activateTool = activateTool;
		_generateFractal = generateFractal;
		_callbackContext = context;
		_selectedModule = kVSSuiteCore;
		_moduleButtons = [NSMutableDictionary dictionary];
		_glyphCache = [NSMutableDictionary dictionary];

		NSArray<NSNumber*>* storedOrder = [NSUserDefaults.standardUserDefaults
			arrayForKey:@"studio.vectorsuite.panel.moduleOrder"];
		NSMutableOrderedSet<NSNumber*>* validatedOrder = [NSMutableOrderedSet orderedSet];
		for (NSNumber* moduleID in storedOrder ?: @[]) {
			if (moduleID.integerValue >= 0 && moduleID.integerValue < kVSModuleCount) {
				[validatedOrder addObject:moduleID];
			}
		}
		for (NSInteger moduleID = 0; moduleID < kVSModuleCount; ++moduleID) {
			[validatedOrder addObject:@(moduleID)];
		}
		_moduleOrder = [validatedOrder.array mutableCopy];
	}
	return self;
}

- (void)loadView
{
	VSPanelRootView* root = [[VSPanelRootView alloc] initWithFrame:NSMakeRect(0, 0, 340, 580)];
	root.appearanceObserver = self;
	self.view = root;

	VSBrandMarkView* brand = [[VSBrandMarkView alloc] initWithFrame:NSZeroRect];
	brand.translatesAutoresizingMaskIntoConstraints = NO;
	[root addSubview:brand];

	NSTextField* title = VSLabel(@"Vector Suite", VSFontTitle(), VSInk());
	[root addSubview:title];

	NSTextField* subtitle = VSLabel(@"Adobe Illustrator 2026", VSFontSmall(), VSMuted());
	[root addSubview:subtitle];

	self.searchField = [[NSSearchField alloc] initWithFrame:NSZeroRect];
	self.searchField.placeholderString = @"Cerca un modulo";
	self.searchField.font = VSFontBody();
	self.searchField.delegate = self;
	self.searchField.translatesAutoresizingMaskIntoConstraints = NO;
	[root addSubview:self.searchField];

	self.categoryPopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
	[self.categoryPopup addItemsWithTitles:@[
		@"Tutti", @"Disegno", @"Geometria", @"Aspetto", @"Workflow", @"Sistema"
	]];
	self.categoryPopup.font = VSFontBody();
	self.categoryPopup.target = self;
	self.categoryPopup.action = @selector(categoryChanged:);
	self.categoryPopup.translatesAutoresizingMaskIntoConstraints = NO;
	[root addSubview:self.categoryPopup];

	NSTextField* libraryLabel = VSEyebrow(@"Libreria");
	[root addSubview:libraryLabel];

	self.countLabel = VSLabel(@"", VSFontSmall(), VSQuiet());
	self.countLabel.alignment = NSTextAlignmentRight;
	[root addSubview:self.countLabel];

	self.moduleStack = [[NSStackView alloc] initWithFrame:NSZeroRect];
	self.moduleStack.orientation = NSUserInterfaceLayoutOrientationVertical;
	self.moduleStack.alignment = NSLayoutAttributeLeading;
	self.moduleStack.distribution = NSStackViewDistributionFill;
	self.moduleStack.spacing = kVSGutter;
	self.moduleStack.edgeInsets = NSEdgeInsetsMake(kVSGutter, kVSGutter, kVSGutter, kVSGutter);
	self.moduleStack.translatesAutoresizingMaskIntoConstraints = NO;

	self.scrollView = [[NSScrollView alloc] initWithFrame:NSZeroRect];
	self.scrollView.hasVerticalScroller = YES;
	self.scrollView.drawsBackground = NO;
	self.scrollView.borderType = NSNoBorder;
	self.scrollView.translatesAutoresizingMaskIntoConstraints = NO;
	self.scrollView.documentView = self.moduleStack;
	[root addSubview:self.scrollView];

	NSTextField* footer = VSLabel(@"Control + trascina per riordinare",
								  VSFontSmall(), VSQuiet());
	footer.alignment = NSTextAlignmentCenter;
	[root addSubview:footer];

	[NSLayoutConstraint activateConstraints:@[
		[brand.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:kVSMargin],
		[brand.topAnchor constraintEqualToAnchor:root.topAnchor constant:kVSMargin],
		[brand.widthAnchor constraintEqualToConstant:32],
		[brand.heightAnchor constraintEqualToConstant:32],

		[title.leadingAnchor constraintEqualToAnchor:brand.trailingAnchor constant:10],
		[title.topAnchor constraintEqualToAnchor:brand.topAnchor constant:0],
		[subtitle.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
		[subtitle.topAnchor constraintEqualToAnchor:title.bottomAnchor constant:1],

		[self.searchField.leadingAnchor constraintEqualToAnchor:root.leadingAnchor
													  constant:kVSMargin],
		[self.searchField.trailingAnchor constraintEqualToAnchor:root.trailingAnchor
													   constant:-kVSMargin],
		[self.searchField.topAnchor constraintEqualToAnchor:brand.bottomAnchor constant:16],

		[self.categoryPopup.leadingAnchor constraintEqualToAnchor:self.searchField.leadingAnchor],
		[self.categoryPopup.trailingAnchor constraintEqualToAnchor:self.searchField.trailingAnchor],
		[self.categoryPopup.topAnchor constraintEqualToAnchor:self.searchField.bottomAnchor
													constant:8],

		[libraryLabel.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:kVSMargin],
		[libraryLabel.topAnchor constraintEqualToAnchor:self.categoryPopup.bottomAnchor
											   constant:14],
		[self.countLabel.trailingAnchor constraintEqualToAnchor:root.trailingAnchor
													  constant:-kVSMargin],
		[self.countLabel.firstBaselineAnchor constraintEqualToAnchor:libraryLabel.firstBaselineAnchor],

		[self.scrollView.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:6],
		[self.scrollView.trailingAnchor constraintEqualToAnchor:root.trailingAnchor constant:-6],
		[self.scrollView.topAnchor constraintEqualToAnchor:libraryLabel.bottomAnchor
												 constant:8],
		[self.scrollView.bottomAnchor constraintEqualToAnchor:footer.topAnchor constant:-8],

		[self.moduleStack.widthAnchor constraintEqualToAnchor:self.scrollView.contentView.widthAnchor],

		[footer.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:12],
		[footer.trailingAnchor constraintEqualToAnchor:root.trailingAnchor constant:-12],
		[footer.bottomAnchor constraintEqualToAnchor:root.bottomAnchor constant:-10]
	]];

	[self rebuildModules];
}

#pragma mark Icone

/**
 * Le icone del bundle sono SVG su tela 44×36: l'area di disegno è il quadrato
 * 32×32 centrato, con 6 unità di margine orizzontale e 2 verticali. Qui la
 * tela viene riscalata in modo che sia proprio quel quadrato a coincidere con
 * la pastiglia, altrimenti il glifo risulterebbe piccolo e schiacciato.
 */
- (NSImage*)glyphForModule:(const VSModuleDefinition&)module
					  side:(CGFloat)side
					 color:(NSColor*)color
{
	NSString* key = [NSString stringWithFormat:@"%s|%.0f|%@",
		module.key, side, [color colorUsingColorSpace:NSColorSpace.deviceRGBColorSpace]];
	NSImage* cached = self.glyphCache[key];
	if (cached) return cached;

	NSString* file = [NSString stringWithFormat:@"VSIcon-%s", module.key];
	NSString* path = [[NSBundle bundleForClass:self.class] pathForResource:file
																	ofType:@"svg"
															   inDirectory:@"svg"];
	NSImage* source = path ? [[NSImage alloc] initWithContentsOfFile:path] : nil;
	if (!source) return nil;

	const CGFloat scale = side / 32.0;
	NSRect placement = NSMakeRect(-6.0 * scale, -2.0 * scale, 44.0 * scale, 36.0 * scale);
	NSColor* resolved = [color colorUsingColorSpace:NSColorSpace.deviceRGBColorSpace] ?: color;

	NSImage* glyph = [NSImage imageWithSize:NSMakeSize(side, side)
									flipped:NO
							 drawingHandler:^BOOL(NSRect bounds) {
		[source drawInRect:placement
				  fromRect:NSZeroRect
				 operation:NSCompositingOperationSourceOver
				  fraction:1.0];
		[resolved setFill];
		NSRectFillUsingOperation(bounds, NSCompositingOperationSourceAtop);
		return YES;
	}];

	self.glyphCache[key] = glyph;
	return glyph;
}

#pragma mark Filtri

- (BOOL)module:(const VSModuleDefinition&)module
	matchesCategory:(NSString*)category
			  query:(NSString*)query
{
	if (![category isEqualToString:@"Tutti"] &&
		![category isEqualToString:[NSString stringWithUTF8String:module.category]]) {
		return NO;
	}
	if (query.length == 0) return YES;
	NSString* haystack = [NSString stringWithFormat:@"%s %s %s",
		module.name, module.summary, module.category];
	return [haystack rangeOfString:query options:NSCaseInsensitiveSearch].location != NSNotFound;
}

#pragma mark Fractal Grove

- (NSString*)fractalPreferenceKey:(NSString*)key
{
	return [@"studio.vectorsuite.fractal." stringByAppendingString:key];
}

- (NSArray<NSDictionary*>*)fractalParameterSpecifications
{
	return @[
		@{@"section": @"Struttura"},
		@{@"key": @"seed", @"label": @"Seme", @"min": @1, @"max": @9999, @"default": @999, @"decimals": @0},
		@{@"key": @"initialLength", @"label": @"Lunghezza iniziale", @"min": @10, @"max": @600, @"default": @150, @"decimals": @0},
		@{@"key": @"lengthFalloff", @"label": @"Decadimento lunghezza", @"min": @0.02, @"max": @0.6, @"default": @0.25, @"decimals": @2},
		@{@"key": @"initialAngle", @"label": @"Angolo iniziale", @"min": @1, @"max": @90, @"default": @25, @"decimals": @0},
		@{@"key": @"angleFalloff", @"label": @"Decadimento angolo", @"min": @0.7, @"max": @1.4, @"default": @1, @"decimals": @2},
		@{@"key": @"minimumLength", @"label": @"Lunghezza minima", @"min": @0.5, @"max": @40, @"default": @2, @"decimals": @1},
		@{@"key": @"maximumIterations", @"label": @"Iterazioni max", @"min": @1, @"max": @13, @"advancedMax": @20, @"default": @11, @"decimals": @0},
		@{@"key": @"maximumPaths", @"label": @"Tracciati max", @"min": @10, @"max": @5000, @"advancedMax": @30000, @"default": @3000, @"decimals": @0},
		@{@"section": @"Stile"},
		@{@"key": @"initialWidth", @"label": @"Spessore iniziale", @"min": @0.1, @"max": @200, @"default": @40, @"decimals": @1},
		@{@"key": @"widthFalloff", @"label": @"Decadimento spessore", @"min": @0.3, @"max": @1, @"default": @0.72, @"decimals": @2},
		@{@"key": @"lengthRandomness", @"label": @"Casualità lunghezza", @"min": @0, @"max": @0.8, @"default": @0.15, @"decimals": @2},
		@{@"key": @"angleRandomness", @"label": @"Casualità angolo", @"min": @0, @"max": @0.8, @"default": @0.15, @"decimals": @2}
	];
}

- (NSDictionary*)fractalSpecificationForKey:(NSString*)key
{
	for (NSDictionary* specification in [self fractalParameterSpecifications]) {
		if ([specification[@"key"] isEqualToString:key]) return specification;
	}
	return nil;
}

- (double)fractalValueForSpecification:(NSDictionary*)specification
{
	NSString* preference = [self fractalPreferenceKey:specification[@"key"]];
	id stored = [NSUserDefaults.standardUserDefaults objectForKey:preference];
	return stored ? [stored doubleValue] : [specification[@"default"] doubleValue];
}

- (BOOL)fractalFlag:(NSString*)key defaultValue:(BOOL)defaultValue
{
	id stored = [NSUserDefaults.standardUserDefaults
		objectForKey:[self fractalPreferenceKey:key]];
	return stored ? [stored boolValue] : defaultValue;
}

- (NSString*)formattedFractalValue:(double)value specification:(NSDictionary*)specification
{
	NSInteger decimals = [specification[@"decimals"] integerValue];
	return [NSString stringWithFormat:decimals == 0 ? @"%.0f" : (decimals == 1 ? @"%.1f" : @"%.2f"), value];
}

- (NSView*)fractalParameterRow:(NSDictionary*)specification
{
	NSString* key = specification[@"key"];
	BOOL advanced = [self fractalFlag:@"advanced" defaultValue:NO];
	double maximum = specification[@"advancedMax"] && advanced
		? [specification[@"advancedMax"] doubleValue]
		: [specification[@"max"] doubleValue];
	double value = [self fractalValueForSpecification:specification];
	value = MAX([specification[@"min"] doubleValue], MIN(maximum, value));

	NSStackView* row = [[NSStackView alloc] initWithFrame:NSZeroRect];
	row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	row.alignment = NSLayoutAttributeCenterY;
	row.spacing = 8;
	row.translatesAutoresizingMaskIntoConstraints = NO;

	NSTextField* label = VSLabel(specification[@"label"], VSFontBody(), VSMuted());
	((NSTextFieldCell*)label.cell).lineBreakMode = NSLineBreakByTruncatingTail;
	[label.widthAnchor constraintEqualToConstant:112].active = YES;
	[row addArrangedSubview:label];

	NSSlider* slider = [[NSSlider alloc] initWithFrame:NSZeroRect];
	VSSliderCell* sliderCell = [[VSSliderCell alloc] init];
	sliderCell.minValue = [specification[@"min"] doubleValue];
	sliderCell.maxValue = maximum;
	sliderCell.sliderType = NSSliderTypeLinear;
	slider.cell = sliderCell;
	slider.minValue = [specification[@"min"] doubleValue];
	slider.maxValue = maximum;
	slider.doubleValue = value;
	slider.target = self;
	slider.action = @selector(fractalSliderChanged:);
	slider.identifier = key;
	slider.continuous = NO;
	slider.translatesAutoresizingMaskIntoConstraints = NO;
	[slider.widthAnchor constraintGreaterThanOrEqualToConstant:86].active = YES;
	[slider.heightAnchor constraintEqualToConstant:18].active = YES;
	[row addArrangedSubview:slider];

	NSTextField* valueField = [[NSTextField alloc] initWithFrame:NSZeroRect];
	valueField.identifier = key;
	valueField.stringValue = [self formattedFractalValue:value specification:specification];
	valueField.alignment = NSTextAlignmentRight;
	valueField.font = VSFontMono();
	valueField.textColor = VSInk();
	valueField.bezeled = YES;
	valueField.bezelStyle = NSTextFieldRoundedBezel;
	valueField.target = self;
	valueField.action = @selector(fractalValueCommitted:);
	valueField.translatesAutoresizingMaskIntoConstraints = NO;
	[valueField.widthAnchor constraintEqualToConstant:56].active = YES;
	[row addArrangedSubview:valueField];

	self.fractalSliders[key] = slider;
	self.fractalValues[key] = valueField;
	return row;
}

- (NSButton*)fractalToggle:(NSString*)title
					   key:(NSString*)key
			  defaultValue:(BOOL)defaultValue
{
	VSCheckbox* button = [[VSCheckbox alloc] initWithFrame:NSZeroRect];
	[button setButtonType:NSButtonTypeSwitch];
	button.bordered = NO;
	button.title = title;
	button.identifier = key;
	button.target = self;
	button.action = @selector(fractalToggleChanged:);
	button.state = [self fractalFlag:key defaultValue:defaultValue]
		? NSControlStateValueOn
		: NSControlStateValueOff;
	button.translatesAutoresizingMaskIntoConstraints = NO;
	[button.heightAnchor constraintEqualToConstant:20].active = YES;
	return button;
}

- (NSView*)fractalSettingsView
{
	self.fractalSliders = [NSMutableDictionary dictionary];
	self.fractalValues = [NSMutableDictionary dictionary];

	NSStackView* stack = [[NSStackView alloc] initWithFrame:NSZeroRect];
	stack.orientation = NSUserInterfaceLayoutOrientationVertical;
	stack.alignment = NSLayoutAttributeLeading;
	stack.spacing = 7;
	stack.edgeInsets = NSEdgeInsetsMake(12, 12, 14, 12);
	stack.translatesAutoresizingMaskIntoConstraints = NO;

	for (NSDictionary* specification in [self fractalParameterSpecifications]) {
		if (specification[@"section"]) {
			[stack addArrangedSubview:VSEyebrow(specification[@"section"])];
		}
		else {
			NSView* row = [self fractalParameterRow:specification];
			[stack addArrangedSubview:row];
			[row.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
		}
	}

	[stack addArrangedSubview:VSEyebrow(@"Ondulazione")];

	self.waveControl = [[VSSegmentedControl alloc]
		initWithLabels:@[@"Nessuna", @"Bassa", @"Media", @"Alta"]
				target:self
				action:@selector(fractalWaveChanged:)];
	id storedWave = [NSUserDefaults.standardUserDefaults
		objectForKey:[self fractalPreferenceKey:@"wave"]];
	self.waveControl.selectedSegment = storedWave ? [storedWave integerValue] : 1;
	[stack addArrangedSubview:self.waveControl];
	[self.waveControl.widthAnchor constraintEqualToAnchor:stack.widthAnchor
												constant:-24].active = YES;

	[stack addArrangedSubview:VSEyebrow(@"Generazione")];

	NSStackView* togglesOne = [[NSStackView alloc] initWithFrame:NSZeroRect];
	togglesOne.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	togglesOne.distribution = NSStackViewDistributionFillEqually;
	togglesOne.translatesAutoresizingMaskIntoConstraints = NO;
	self.automaticToggle = [self fractalToggle:@"Auto" key:@"automatic" defaultValue:NO];
	self.advancedToggle = [self fractalToggle:@"Avanzato" key:@"advanced" defaultValue:NO];
	[togglesOne addArrangedSubview:self.automaticToggle];
	[togglesOne addArrangedSubview:self.advancedToggle];
	[stack addArrangedSubview:togglesOne];
	[togglesOne.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	NSStackView* togglesTwo = [[NSStackView alloc] initWithFrame:NSZeroRect];
	togglesTwo.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	togglesTwo.distribution = NSStackViewDistributionFillEqually;
	togglesTwo.translatesAutoresizingMaskIntoConstraints = NO;
	self.groupToggle = [self fractalToggle:@"Raggruppa" key:@"group" defaultValue:YES];
	self.replaceToggle = [self fractalToggle:@"Sostituisci" key:@"replace" defaultValue:YES];
	self.replaceToggle.enabled = self.groupToggle.state == NSControlStateValueOn;
	[togglesTwo addArrangedSubview:self.groupToggle];
	[togglesTwo addArrangedSubview:self.replaceToggle];
	[stack addArrangedSubview:togglesTwo];
	[togglesTwo.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	NSStackView* utility = [[NSStackView alloc] initWithFrame:NSZeroRect];
	utility.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	utility.distribution = NSStackViewDistributionFillEqually;
	utility.spacing = 7;
	utility.translatesAutoresizingMaskIntoConstraints = NO;
	[utility addArrangedSubview:[[VSPushButton alloc] initWithTitle:@"Nuovo seme"
														 prominent:NO
															target:self
															action:@selector(fractalNewSeed:)]];
	[utility addArrangedSubview:[[VSPushButton alloc] initWithTitle:@"Ripristina"
														 prominent:NO
															target:self
															action:@selector(fractalReset:)]];
	[stack addArrangedSubview:utility];
	[utility.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	VSPushButton* generate = [[VSPushButton alloc] initWithTitle:@"Genera albero"
													  prominent:YES
														 target:self
														 action:@selector(fractalGenerate:)];
	generate.keyEquivalent = @"\r";
	[stack addArrangedSubview:generate];
	[generate.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	self.fractalStatus = VSLabel(@"Pronto", VSFontMono(), VSQuiet());
	self.fractalStatus.alignment = NSTextAlignmentCenter;
	[stack addArrangedSubview:self.fractalStatus];
	[self.fractalStatus.widthAnchor constraintEqualToAnchor:stack.widthAnchor
												  constant:-24].active = YES;
	return stack;
}

- (VSFractalSettings)currentFractalSettings
{
	VSFractalSettings settings = {};
	settings.seed = self.fractalSliders[@"seed"].doubleValue;
	settings.initialLength = self.fractalSliders[@"initialLength"].doubleValue;
	settings.lengthFalloff = self.fractalSliders[@"lengthFalloff"].doubleValue;
	settings.initialAngle = self.fractalSliders[@"initialAngle"].doubleValue;
	settings.angleFalloff = self.fractalSliders[@"angleFalloff"].doubleValue;
	settings.minimumLength = self.fractalSliders[@"minimumLength"].doubleValue;
	settings.maximumIterations = (int)llround(self.fractalSliders[@"maximumIterations"].doubleValue);
	settings.maximumPaths = (int)llround(self.fractalSliders[@"maximumPaths"].doubleValue);
	settings.initialWidth = self.fractalSliders[@"initialWidth"].doubleValue;
	settings.widthFalloff = self.fractalSliders[@"widthFalloff"].doubleValue;
	settings.lengthRandomness = self.fractalSliders[@"lengthRandomness"].doubleValue;
	settings.angleRandomness = self.fractalSliders[@"angleRandomness"].doubleValue;
	settings.wave = (int)self.waveControl.selectedSegment;
	settings.automatic = self.automaticToggle.state == NSControlStateValueOn;
	settings.advanced = self.advancedToggle.state == NSControlStateValueOn;
	settings.group = self.groupToggle.state == NSControlStateValueOn;
	settings.replace = self.replaceToggle.state == NSControlStateValueOn;
	return settings;
}

- (void)generateFractalNow
{
	if (!self.generateFractal) return;
	self.fractalStatus.stringValue = @"Generazione…";
	[self.view.window displayIfNeeded];
	VSFractalSettings settings = [self currentFractalSettings];
	int count = self.generateFractal(self.callbackContext, &settings);
	self.fractalStatus.stringValue = count >= 0
		? [NSString stringWithFormat:@"%d tracciati · seme %.0f", count, settings.seed]
		: @"Generazione non riuscita";
}

- (void)fractalSliderChanged:(NSSlider*)sender
{
	NSDictionary* specification = [self fractalSpecificationForKey:sender.identifier];
	if (!specification) return;
	[NSUserDefaults.standardUserDefaults setDouble:sender.doubleValue
											forKey:[self fractalPreferenceKey:sender.identifier]];
	self.fractalValues[sender.identifier].stringValue =
		[self formattedFractalValue:sender.doubleValue specification:specification];
	if (self.automaticToggle.state == NSControlStateValueOn) [self generateFractalNow];
}

- (void)fractalValueCommitted:(NSTextField*)sender
{
	NSDictionary* specification = [self fractalSpecificationForKey:sender.identifier];
	NSSlider* slider = self.fractalSliders[sender.identifier];
	if (!specification || !slider) return;
	double value = sender.doubleValue;
	value = MAX(slider.minValue, MIN(slider.maxValue, value));
	slider.doubleValue = value;
	[self fractalSliderChanged:slider];
}

- (void)fractalWaveChanged:(VSSegmentedControl*)sender
{
	[NSUserDefaults.standardUserDefaults setInteger:sender.selectedSegment
											 forKey:[self fractalPreferenceKey:@"wave"]];
	if (self.automaticToggle.state == NSControlStateValueOn) [self generateFractalNow];
}

- (void)fractalToggleChanged:(NSButton*)sender
{
	[NSUserDefaults.standardUserDefaults setBool:sender.state == NSControlStateValueOn
										  forKey:[self fractalPreferenceKey:sender.identifier]];
	if ([sender.identifier isEqualToString:@"group"]) {
		self.replaceToggle.enabled = sender.state == NSControlStateValueOn;
		[self.replaceToggle setNeedsDisplay:YES];
	}
	if ([sender.identifier isEqualToString:@"advanced"]) {
		BOOL advanced = sender.state == NSControlStateValueOn;
		for (NSString* key in @[@"maximumIterations", @"maximumPaths"]) {
			NSDictionary* specification = [self fractalSpecificationForKey:key];
			NSSlider* slider = self.fractalSliders[key];
			slider.maxValue = advanced
				? [specification[@"advancedMax"] doubleValue]
				: [specification[@"max"] doubleValue];
			if (slider.doubleValue > slider.maxValue) {
				slider.doubleValue = slider.maxValue;
				[self fractalSliderChanged:slider];
			}
			[slider setNeedsDisplay:YES];
		}
	}
}

- (void)fractalNewSeed:(id)sender
{
	NSSlider* slider = self.fractalSliders[@"seed"];
	slider.doubleValue = (double)arc4random_uniform(9999) + 1;
	[self fractalSliderChanged:slider];
	if (self.automaticToggle.state != NSControlStateValueOn) [self generateFractalNow];
}

- (void)fractalReset:(id)sender
{
	for (NSDictionary* specification in [self fractalParameterSpecifications]) {
		NSString* key = specification[@"key"];
		if (!key) continue;
		[NSUserDefaults.standardUserDefaults setDouble:[specification[@"default"] doubleValue]
												forKey:[self fractalPreferenceKey:key]];
	}
	[NSUserDefaults.standardUserDefaults setInteger:1
											 forKey:[self fractalPreferenceKey:@"wave"]];
	[self rebuildModules];
	self.fractalStatus.stringValue = @"Valori predefiniti ripristinati";
}

- (void)fractalGenerate:(id)sender
{
	[self generateFractalNow];
}

#pragma mark Griglia dei moduli

- (VSModuleCardButton*)moduleCardForModule:(const VSModuleDefinition&)module
{
	VSModuleCardButton* button = [[VSModuleCardButton alloc] initWithFrame:NSZeroRect];
	button.moduleID = module.id;
	button.moduleName = [NSString stringWithUTF8String:module.name];
	button.moduleSummary = [NSString stringWithUTF8String:module.summary];
	button.moduleImage = [self glyphForModule:module
										 side:kVSTileSide - 10.0
										color:VSPaper()];
	button.selectedCard = module.id == self.selectedModule;
	button.reorderDelegate = self;
	button.tag = module.id;
	button.target = self;
	button.action = @selector(modulePressed:);
	button.toolTip = [NSString stringWithFormat:@"%s · Control + trascina per spostare",
					  module.summary];
	button.translatesAutoresizingMaskIntoConstraints = NO;
	[button.heightAnchor constraintEqualToConstant:kVSCardHeight].active = YES;
	return button;
}

- (void)rebuildModules
{
	for (NSView* view in self.moduleStack.arrangedSubviews.copy) {
		[self.moduleStack removeArrangedSubview:view];
		[view removeFromSuperview];
	}
	[self.moduleButtons removeAllObjects];

	NSString* category = self.categoryPopup.titleOfSelectedItem ?: @"Tutti";
	NSString* query = [self.searchField.stringValue
		stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];

	NSMutableArray<NSNumber*>* visibleModules = [NSMutableArray array];
	for (NSNumber* moduleNumber in self.moduleOrder) {
		const VSModuleDefinition& module = kVSModules[moduleNumber.integerValue];
		if (![self module:module matchesCategory:category query:query]) continue;
		[visibleModules addObject:moduleNumber];
	}

	self.countLabel.stringValue = visibleModules.count == 1
		? @"1 modulo"
		: [NSString stringWithFormat:@"%lu moduli", (unsigned long)visibleModules.count];

	for (NSInteger index = 0; index < (NSInteger)visibleModules.count; index += 2) {
		NSStackView* row = [[NSStackView alloc] initWithFrame:NSZeroRect];
		row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
		row.alignment = NSLayoutAttributeCenterY;
		row.distribution = NSStackViewDistributionFillEqually;
		row.spacing = kVSGutter;
		row.translatesAutoresizingMaskIntoConstraints = NO;

		for (NSInteger column = 0; column < 2; ++column) {
			NSInteger visibleIndex = index + column;
			if (visibleIndex < (NSInteger)visibleModules.count) {
				NSInteger moduleID = visibleModules[visibleIndex].integerValue;
				const VSModuleDefinition& module = kVSModules[moduleID];
				VSModuleCardButton* button = [self moduleCardForModule:module];
				[row addArrangedSubview:button];
				self.moduleButtons[@(moduleID)] = button;
			}
			else {
				NSView* placeholder = [[NSView alloc] initWithFrame:NSZeroRect];
				placeholder.translatesAutoresizingMaskIntoConstraints = NO;
				[placeholder.heightAnchor constraintEqualToConstant:kVSCardHeight].active = YES;
				[row addArrangedSubview:placeholder];
			}
		}

		[self.moduleStack addArrangedSubview:row];
		[row.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
									   constant:-2 * kVSGutter].active = YES;
		[row.heightAnchor constraintEqualToConstant:kVSCardHeight].active = YES;
	}

	if (self.selectedModule == kVSFractalGrove &&
		[visibleModules containsObject:@(kVSFractalGrove)]) {
		NSView* settings = [self fractalSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
											 constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		CGFloat settingsHeight = MAX(1.0, settings.fittingSize.height);
		[settings.heightAnchor constraintEqualToConstant:settingsHeight].active = YES;
	}
}

- (void)moveModuleID:(NSInteger)moduleID toWindowPoint:(NSPoint)windowPoint
{
	__block NSInteger targetModuleID = NSNotFound;
	[self.moduleButtons enumerateKeysAndObjectsUsingBlock:
		^(NSNumber* key, NSButton* button, BOOL* stop) {
			NSRect windowRect = [button convertRect:button.bounds toView:nil];
			if (NSPointInRect(windowPoint, windowRect)) {
				targetModuleID = key.integerValue;
				*stop = YES;
			}
		}];
	if (targetModuleID == NSNotFound || targetModuleID == moduleID) return;

	NSUInteger sourceIndex = [self.moduleOrder indexOfObject:@(moduleID)];
	NSUInteger targetIndex = [self.moduleOrder indexOfObject:@(targetModuleID)];
	if (sourceIndex == NSNotFound || targetIndex == NSNotFound) return;

	[self.moduleOrder exchangeObjectAtIndex:sourceIndex withObjectAtIndex:targetIndex];
	[NSUserDefaults.standardUserDefaults setObject:self.moduleOrder
											forKey:@"studio.vectorsuite.panel.moduleOrder"];
	[self rebuildModules];
}

- (void)modulePressed:(NSButton*)sender
{
	NSInteger moduleID = sender.tag;
	self.selectedModule = moduleID;
	[self rebuildModules];
	if (self.activateTool) {
		self.activateTool(self.callbackContext, (int)moduleID);
	}
}

- (void)categoryChanged:(id)sender
{
	[self rebuildModules];
}

- (void)controlTextDidChange:(NSNotification*)notification
{
	[self rebuildModules];
}

- (void)selectModule:(NSInteger)moduleID
{
	if (moduleID < 0 || moduleID >= kVSModuleCount) return;
	self.selectedModule = moduleID;
	[self rebuildModules];
	NSButton* selectedButton = self.moduleButtons[@(moduleID)];
	if (selectedButton) {
		[self.moduleStack layoutSubtreeIfNeeded];
		[selectedButton scrollRectToVisible:selectedButton.bounds];
	}
}

/// Un cambio di tema cambia il colore dei glifi: la cache va svuotata e le
/// schede ricostruite, altrimenti resterebbero le icone del tema precedente.
- (void)appearanceDidChange
{
	[self.glyphCache removeAllObjects];
	[self rebuildModules];
}

@end

#pragma mark - Ponte C

void* VSCreatePanelController(
	void* parentView,
	VSPanelActivateToolProc activateTool,
	VSPanelGenerateFractalProc generateFractal,
	void* context)
{
	NSView* hostView = (__bridge NSView*)parentView;
	if (!hostView) return nullptr;

	VSPanelController* controller = [[VSPanelController alloc]
		initWithHostView:hostView
		activateTool:activateTool
		generateFractal:generateFractal
		context:context];
	(void)controller.view;

	// Il contenitore restituito da AIPanel è gestito direttamente da
	// Illustrator e non partecipa in modo affidabile a un albero Auto Layout.
	// Il sample ufficiale EmptyPanel usa frame + autoresizing; seguire lo
	// stesso contratto evita una view a frame nullo nei pannelli flottanti.
	NSRect initialFrame = controller.view.frame;
	[hostView setFrame:initialFrame];
	NSRect hostBounds = hostView.bounds;
	controller.view.translatesAutoresizingMaskIntoConstraints = YES;
	controller.view.frame = NSMakeRect(
		0,
		0,
		NSWidth(hostBounds),
		NSHeight(hostBounds));
	controller.view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
	[hostView addSubview:controller.view];
	[hostView setNeedsLayout:YES];
	[hostView layoutSubtreeIfNeeded];
	[controller.view setNeedsDisplay:YES];
	return (__bridge_retained void*)controller;
}

void VSDestroyPanelController(void* opaqueController)
{
	if (!opaqueController) return;
	VSPanelController* controller = (__bridge_transfer VSPanelController*)opaqueController;
	[controller.view removeFromSuperview];
}

void VSResizePanelController(void* opaqueController)
{
	if (!opaqueController) return;
	VSPanelController* controller = (__bridge VSPanelController*)opaqueController;
	NSRect hostBounds = controller.hostView.bounds;
	controller.view.frame = NSMakeRect(
		0,
		0,
		NSWidth(hostBounds),
		NSHeight(hostBounds));
	[controller.view setNeedsLayout:YES];
	[controller.view layoutSubtreeIfNeeded];
	[controller.view setNeedsDisplay:YES];
}

void VSSelectPanelModule(void* opaqueController, int moduleID)
{
	if (!opaqueController) return;
	VSPanelController* controller = (__bridge VSPanelController*)opaqueController;
	[controller selectModule:moduleID];
}
