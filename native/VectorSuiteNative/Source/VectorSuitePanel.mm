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
#include "VectorSuiteProjection.h"

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

@interface VSNumericField : NSTextField
@property(nonatomic) CGFloat wheelRemainder;
@end
@implementation VSNumericField
- (void)scrollWheel:(NSEvent*)event
{
	NSText* editor = self.currentEditor;
	NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
	if (!self.enabled || !editor || self.window.firstResponder != editor || !NSPointInRect(point, self.bounds)) {
		[super scrollWheel:event]; return;
	}
	if (event.momentumPhase != NSEventPhaseNone) return;
	CGFloat delta = event.scrollingDeltaY;
	if (event.isDirectionInvertedFromDevice) delta = -delta;
	if (event.phase == NSEventPhaseBegan) self.wheelRemainder = 0;
	self.wheelRemainder += event.hasPreciseScrollingDeltas ? delta / 8.0 : delta;
	NSInteger steps = (NSInteger)self.wheelRemainder;
	if (!steps) return;
	self.wheelRemainder -= steps;
	double increment = (event.modifierFlags & NSEventModifierFlagOption) ? 0.1 : 1.0;
	if (event.modifierFlags & NSEventModifierFlagShift) increment *= 10.0;
	NSScanner* scanner = [NSScanner scannerWithString:[editor.string stringByReplacingOccurrencesOfString:@"," withString:@"."]];
	double value = 0;
	if (![scanner scanDouble:&value] || !scanner.isAtEnd || !std::isfinite(value)) return;
	self.stringValue = [NSString stringWithFormat:@"%.6g", value + steps * increment];
	editor.string = self.stringValue;
	if (self.action) [self sendAction:self.action to:self.target];
	editor.string = self.stringValue;
	[[NSNotificationCenter defaultCenter] postNotificationName:NSControlTextDidEndEditingNotification object:self];
}
@end

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
	// Il cursore è una barretta verticale, non un pomello tondo: si legge come
	// una tacca su una scala e non lascia dubbi su dove cade il valore.
	NSRect knob = [super knobRectFlipped:flipped];
	const CGFloat width = 3.0;
	const CGFloat height = 14.0;
	return NSMakeRect(NSMidX(knob) - width * 0.5, NSMidY(knob) - height * 0.5, width, height);
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
	// Un filo di carta attorno alla barretta la stacca dalla traccia piena
	// anche quando il cursore è tutto a destra.
	NSRect halo = NSInsetRect(knobRect, -1.5, -1.5);
	[VSPaper() setFill];
	[[NSBezierPath bezierPathWithRoundedRect:halo xRadius:2.5 yRadius:2.5] fill];

	[VSInk() setFill];
	[[NSBezierPath bezierPathWithRoundedRect:knobRect xRadius:1.5 yRadius:1.5] fill];
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
@property(nonatomic, strong) NSMutableDictionary<NSString*, NSSlider*>* projectionSliders;
@property(nonatomic, strong) NSMutableDictionary<NSString*, NSTextField*>* projectionValues;
@property(nonatomic, strong) VSSegmentedControl* projectionTool;
@property(nonatomic, strong) VSSegmentedControl* projectionPreset;
@property(nonatomic, strong) VSSegmentedControl* projectionPlane;
@property(nonatomic, strong) VSSegmentedControl* projectionTarget;
@property(nonatomic, strong) VSSegmentedControl* projectionMoveAxis;
@property(nonatomic, strong) NSButton* projectionSnap;
@property(nonatomic, strong) NSButton* projectionKeepOriginal;
@property(nonatomic, strong) NSMutableDictionary<NSString*, NSTextField*>* transformFields;
@property(nonatomic, strong) VSSegmentedControl* randomDistribution;
@property(nonatomic, strong) NSButton* randomUniformScale;
@property(nonatomic, strong) VSSegmentedControl* mirrorMode;
@property(nonatomic, strong) VSSegmentedControl* collisionDirection;
@property(nonatomic, strong) NSButton* collisionAlignCenters;
@property(nonatomic, strong) VSSegmentedControl* widthMode;
@property(nonatomic, strong) VSSegmentedControl* widthCap;
@property(nonatomic, strong) VSSegmentedControl* widthJoin;
@property(nonatomic, strong) VSSegmentedControl* liveBlendMode;
@property(nonatomic, strong) NSArray<NSNumber*>* liveBlendValues;
@property(nonatomic, strong) NSButton* liveIsolated;
@property(nonatomic, strong) NSButton* autoSaveEnabled;
@property(nonatomic, strong) NSButton* autoSaveModifiedOnly;
@property(nonatomic, strong) NSButton* autoSaveVersions;
@property(nonatomic, strong) VSSegmentedControl* rasterResampling;
@property(nonatomic, strong) NSButton* pathPreserveCurves;
@property(nonatomic, strong) NSButton* fluidClosePath;
@property(nonatomic, strong) NSButton* textureCrosshatch;
@property(nonatomic, strong) VSSegmentedControl* geometryMode;
@property(nonatomic, strong) NSButton* shapePreserveHandles;
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
		_selectedModule = kVSPrecisionPen;
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

		// Le impostazioni di proiezione vivono in memoria dentro il plug-in:
		// qui vengono ripristinate dalle preferenze all'apertura del pannello.
		VSProjectionSettings projection = VSProjectionDefaults();
		NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
		if ([defaults objectForKey:@"studio.vectorsuite.projection.leftAngle"]) {
			projection.leftAngle = [defaults doubleForKey:@"studio.vectorsuite.projection.leftAngle"];
			projection.rightAngle = [defaults doubleForKey:@"studio.vectorsuite.projection.rightAngle"];
			projection.plane = (int)[defaults integerForKey:@"studio.vectorsuite.projection.plane"];
			projection.snapLine = [defaults boolForKey:@"studio.vectorsuite.projection.snapLine"] ? 1 : 0;
			if ([defaults objectForKey:@"studio.vectorsuite.projection.moveDistance"]) {
				projection.moveDistance = [defaults
					doubleForKey:@"studio.vectorsuite.projection.moveDistance"];
				projection.moveAxis = (int)[defaults
					integerForKey:@"studio.vectorsuite.projection.moveAxis"];
			}
			if ([defaults objectForKey:@"studio.vectorsuite.projection.scaleU"]) {
				projection.scaleU = [defaults doubleForKey:@"studio.vectorsuite.projection.scaleU"];
				projection.scaleV = [defaults doubleForKey:@"studio.vectorsuite.projection.scaleV"];
				projection.rotation = [defaults doubleForKey:@"studio.vectorsuite.projection.rotation"];
				projection.shear = [defaults doubleForKey:@"studio.vectorsuite.projection.shear"];
			}
		}
		VSProjectionSet(&projection);

		VSAutoSaveSettings autoSave = VSAutoSaveDefaults();
		if ([defaults objectForKey:@"studio.vectorsuite.autoSave.enabled"]) {
			autoSave.enabled = [defaults boolForKey:@"studio.vectorsuite.autoSave.enabled"] ? 1 : 0;
			autoSave.intervalMinutes = (int)[defaults integerForKey:@"studio.vectorsuite.autoSave.intervalMinutes"];
			autoSave.modifiedOnly = [defaults boolForKey:@"studio.vectorsuite.autoSave.modifiedOnly"] ? 1 : 0;
			autoSave.createVersionCopy = [defaults boolForKey:@"studio.vectorsuite.autoSave.createVersionCopy"] ? 1 : 0;
		}
		autoSave = VSSanitizeAutoSave(autoSave);
		VSAutoSaveSet(&autoSave);
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

	NSTextField* valueField = [[VSNumericField alloc] initWithFrame:NSZeroRect];
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

#pragma mark Projection Studio

// I parametri elementari del disegno assonometrico: i due angoli degli assi
// orizzontali, la faccia su cui cade la forma, e se la linea si aggancia agli
// assi. Con angoli uguali si ha l'isometrica, con angoli diversi la dimetrica o
// la trimetrica.

- (NSString*)projectionPreferenceKey:(NSString*)key
{
	return [@"studio.vectorsuite.projection." stringByAppendingString:key];
}

- (NSInteger)projectionToolSelection
{
	NSInteger tool = [NSUserDefaults.standardUserDefaults
		integerForKey:[self projectionPreferenceKey:@"tool"]];
	if (tool < 0 || tool >= kVSPanelProjectionToolCount) tool = 0;
	return tool;
}

- (NSInteger)projectionTargetSelection
{
	NSInteger target = [NSUserDefaults.standardUserDefaults
		integerForKey:[self projectionPreferenceKey:@"target"]];
	if (target < 0 || target >= kVSPanelProjectionCommandCount) target = 0;
	return target;
}

- (void)storeProjection:(VSProjectionSettings)settings
{
	VSProjectionSet(&settings);
	NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
	[defaults setDouble:settings.leftAngle forKey:[self projectionPreferenceKey:@"leftAngle"]];
	[defaults setDouble:settings.rightAngle forKey:[self projectionPreferenceKey:@"rightAngle"]];
	[defaults setInteger:settings.plane forKey:[self projectionPreferenceKey:@"plane"]];
	[defaults setBool:settings.snapLine != 0 forKey:[self projectionPreferenceKey:@"snapLine"]];
	[defaults setDouble:settings.moveDistance
				 forKey:[self projectionPreferenceKey:@"moveDistance"]];
	[defaults setInteger:settings.moveAxis
				  forKey:[self projectionPreferenceKey:@"moveAxis"]];
	[defaults setDouble:settings.scaleU
				 forKey:[self projectionPreferenceKey:@"scaleU"]];
	[defaults setDouble:settings.scaleV
				 forKey:[self projectionPreferenceKey:@"scaleV"]];
	[defaults setDouble:settings.rotation
				 forKey:[self projectionPreferenceKey:@"rotation"]];
	[defaults setDouble:settings.shear
				 forKey:[self projectionPreferenceKey:@"shear"]];
}

- (void)syncProjectionFields
{
	const VSProjectionSettings settings = VSProjectionGet();
	self.projectionValues[@"left"].stringValue =
		[NSString stringWithFormat:@"%.1f°", settings.leftAngle];
	self.projectionValues[@"right"].stringValue =
		[NSString stringWithFormat:@"%.1f°", settings.rightAngle];
	self.projectionValues[@"move"].stringValue =
		[NSString stringWithFormat:@"%.1f", settings.moveDistance];
	self.projectionValues[@"scaleU"].stringValue =
		[NSString stringWithFormat:@"%.0f", settings.scaleU];
	self.projectionValues[@"scaleV"].stringValue =
		[NSString stringWithFormat:@"%.0f", settings.scaleV];
	self.projectionValues[@"rotation"].stringValue =
		[NSString stringWithFormat:@"%.1f", settings.rotation];
	self.projectionValues[@"shear"].stringValue =
		[NSString stringWithFormat:@"%.1f", settings.shear];
	self.projectionSliders[@"scaleU"].doubleValue = settings.scaleU;
	self.projectionSliders[@"scaleV"].doubleValue = settings.scaleV;
	self.projectionSliders[@"rotation"].doubleValue = settings.rotation;
	self.projectionSliders[@"shear"].doubleValue = settings.shear;

	// Il preset si deduce dagli angoli invece di essere memorizzato a parte:
	// così non può raccontare una cosa diversa da quella che disegna.
	NSInteger preset = 4;
	if (std::abs(settings.leftAngle - 30.0) < 0.05 &&
		std::abs(settings.rightAngle - 30.0) < 0.05) {
		preset = 0;
	}
	else if (std::abs(settings.leftAngle - 7.0) < 0.05 &&
			 std::abs(settings.rightAngle - 42.0) < 0.05) {
		preset = 1;
	}
	else if (std::abs(settings.leftAngle - 45.0) < 0.05 && std::abs(settings.rightAngle) < 0.05) preset = 2;
	else if (std::abs(settings.leftAngle - 30.0) < 0.05 && std::abs(settings.rightAngle - 60.0) < 0.05) preset = 3;
	self.projectionPreset.selectedSegment = preset;
}

- (NSView*)projectionParameterRow:(NSString*)key label:(NSString*)label value:(double)value
{
	NSStackView* row = [[NSStackView alloc] initWithFrame:NSZeroRect];
	row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	row.alignment = NSLayoutAttributeCenterY;
	row.spacing = 8;
	row.translatesAutoresizingMaskIntoConstraints = NO;

	NSTextField* caption = VSLabel(label, VSFontBody(), VSMuted());
	[caption.widthAnchor constraintEqualToConstant:112].active = YES;
	[row addArrangedSubview:caption];

	NSSlider* slider = [[NSSlider alloc] initWithFrame:NSZeroRect];
	VSSliderCell* cell = [[VSSliderCell alloc] init];
	cell.minValue = 0.0;
	cell.maxValue = 89.0;
	cell.sliderType = NSSliderTypeLinear;
	slider.cell = cell;
	slider.minValue = 0.0;
	slider.maxValue = 89.0;
	slider.doubleValue = value;
	slider.target = self;
	slider.action = @selector(projectionSliderChanged:);
	slider.identifier = key;
	slider.continuous = NO;
	slider.translatesAutoresizingMaskIntoConstraints = NO;
	[slider.widthAnchor constraintGreaterThanOrEqualToConstant:86].active = YES;
	[slider.heightAnchor constraintEqualToConstant:18].active = YES;
	[row addArrangedSubview:slider];

	NSTextField* field = [[VSNumericField alloc] initWithFrame:NSZeroRect];
	field.font = VSFontMono();
	field.identifier = key;
	field.target = self;
	field.action = @selector(projectionSliderChanged:);
	field.translatesAutoresizingMaskIntoConstraints = NO;
	field.alignment = NSTextAlignmentRight;
	[field.widthAnchor constraintEqualToConstant:56].active = YES;
	[row addArrangedSubview:field];

	self.projectionSliders[key] = slider;
	self.projectionValues[key] = field;
	return row;
}

- (NSView*)projectionTransformParameterRow:(NSString*)key
								 label:(NSString*)label
							  minimum:(double)minimum
							  maximum:(double)maximum
								 value:(double)value
{
	NSStackView* row = [[NSStackView alloc] initWithFrame:NSZeroRect];
	row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	row.alignment = NSLayoutAttributeCenterY;
	row.spacing = 8;
	row.translatesAutoresizingMaskIntoConstraints = NO;

	NSTextField* caption = VSLabel(label, VSFontBody(), VSMuted());
	[caption.widthAnchor constraintEqualToConstant:112].active = YES;
	[row addArrangedSubview:caption];

	NSSlider* slider = [[NSSlider alloc] initWithFrame:NSZeroRect];
	VSSliderCell* cell = [[VSSliderCell alloc] init];
	cell.minValue = minimum;
	cell.maxValue = maximum;
	cell.sliderType = NSSliderTypeLinear;
	slider.cell = cell;
	slider.minValue = minimum;
	slider.maxValue = maximum;
	slider.doubleValue = MAX(minimum, MIN(maximum, value));
	slider.identifier = key;
	slider.target = self;
	slider.action = @selector(projectionTransformSliderChanged:);
	slider.continuous = NO;
	slider.translatesAutoresizingMaskIntoConstraints = NO;
	[slider.widthAnchor constraintGreaterThanOrEqualToConstant:86].active = YES;
	[slider.heightAnchor constraintEqualToConstant:18].active = YES;
	[row addArrangedSubview:slider];

	NSTextField* field = [[VSNumericField alloc] initWithFrame:NSZeroRect];
	field.identifier = key;
	field.alignment = NSTextAlignmentRight;
	field.font = VSFontMono();
	field.textColor = VSInk();
	field.bezeled = YES;
	field.bezelStyle = NSTextFieldRoundedBezel;
	field.target = self;
	field.action = @selector(projectionTransformValueCommitted:);
	field.translatesAutoresizingMaskIntoConstraints = NO;
	[field.widthAnchor constraintEqualToConstant:56].active = YES;
	[row addArrangedSubview:field];

	self.projectionSliders[key] = slider;
	self.projectionValues[key] = field;
	return row;
}

- (NSView*)projectionMoveDistanceRow:(double)value
{
	NSStackView* row = [[NSStackView alloc] initWithFrame:NSZeroRect];
	row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	row.alignment = NSLayoutAttributeCenterY;
	row.spacing = 8;
	row.translatesAutoresizingMaskIntoConstraints = NO;

	NSTextField* caption = VSLabel(@"Distanza (pt)", VSFontBody(), VSMuted());
	[caption.widthAnchor constraintEqualToConstant:112].active = YES;
	[row addArrangedSubview:caption];

	NSSlider* slider = [[NSSlider alloc] initWithFrame:NSZeroRect];
	VSSliderCell* cell = [[VSSliderCell alloc] init];
	cell.minValue = -2000.0;
	cell.maxValue = 2000.0;
	cell.sliderType = NSSliderTypeLinear;
	slider.cell = cell;
	slider.minValue = -2000.0;
	slider.maxValue = 2000.0;
	slider.doubleValue = MAX(-2000.0, MIN(2000.0, value));
	slider.target = self;
	slider.action = @selector(projectionSliderChanged:);
	slider.identifier = @"move";
	slider.continuous = NO;
	slider.translatesAutoresizingMaskIntoConstraints = NO;
	[slider.widthAnchor constraintGreaterThanOrEqualToConstant:86].active = YES;
	[slider.heightAnchor constraintEqualToConstant:18].active = YES;
	[row addArrangedSubview:slider];

	NSTextField* field = [[VSNumericField alloc] initWithFrame:NSZeroRect];
	field.alignment = NSTextAlignmentRight;
	field.font = VSFontMono();
	field.textColor = VSInk();
	field.bezeled = YES;
	field.bezelStyle = NSTextFieldRoundedBezel;
	field.target = self;
	field.action = @selector(projectionMoveDistanceCommitted:);
	field.translatesAutoresizingMaskIntoConstraints = NO;
	[field.widthAnchor constraintEqualToConstant:56].active = YES;
	[row addArrangedSubview:field];

	self.projectionSliders[@"move"] = slider;
	self.projectionValues[@"move"] = field;
	return row;
}

- (NSView*)projectionSettingsView
{
	self.projectionSliders = [NSMutableDictionary dictionary];
	self.projectionValues = [NSMutableDictionary dictionary];
	const VSProjectionSettings settings = VSProjectionGet();

	NSStackView* stack = [[NSStackView alloc] initWithFrame:NSZeroRect];
	stack.orientation = NSUserInterfaceLayoutOrientationVertical;
	stack.alignment = NSLayoutAttributeLeading;
	stack.spacing = 7;
	stack.edgeInsets = NSEdgeInsetsMake(12, 12, 14, 12);
	stack.translatesAutoresizingMaskIntoConstraints = NO;

	[stack addArrangedSubview:VSEyebrow(@"Proiezione")];

	self.projectionTool = [[VSSegmentedControl alloc]
		initWithLabels:@[@"Linea", @"Rettangolo", @"Ellisse", @"Box"]
				target:self
				action:@selector(projectionToolChanged:)];
	self.projectionTool.selectedSegment = [self projectionToolSelection];
	[stack addArrangedSubview:self.projectionTool];
	[self.projectionTool.widthAnchor constraintEqualToAnchor:stack.widthAnchor
											 constant:-24].active = YES;

	self.projectionPreset = [[VSSegmentedControl alloc]
		initWithLabels:@[@"Iso", @"Di", @"Cavaliera", @"Mono", @"Libera"]
				target:self
				action:@selector(projectionPresetChanged:)];
	[stack addArrangedSubview:self.projectionPreset];
	[self.projectionPreset.widthAnchor constraintEqualToAnchor:stack.widthAnchor
													 constant:-24].active = YES;

	NSView* leftRow = [self projectionParameterRow:@"left"
											 label:@"Angolo sinistro"
											 value:settings.leftAngle];
	[stack addArrangedSubview:leftRow];
	[leftRow.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	NSView* rightRow = [self projectionParameterRow:@"right"
											  label:@"Angolo destro"
											  value:settings.rightAngle];
	[stack addArrangedSubview:rightRow];
	[rightRow.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	[stack addArrangedSubview:VSEyebrow(@"Piano attivo")];

	self.projectionPlane = [[VSSegmentedControl alloc]
		initWithLabels:@[@"Superiore", @"Sinistro", @"Destro"]
				target:self
				action:@selector(projectionPlaneChanged:)];
	self.projectionPlane.selectedSegment = settings.plane;
	[stack addArrangedSubview:self.projectionPlane];
	[self.projectionPlane.widthAnchor constraintEqualToAnchor:stack.widthAnchor
													constant:-24].active = YES;

	self.projectionSnap = [[VSCheckbox alloc] initWithFrame:NSZeroRect];
	[self.projectionSnap setButtonType:NSButtonTypeSwitch];
	self.projectionSnap.bordered = NO;
	self.projectionSnap.title = @"Aggancia la linea agli assi";
	self.projectionSnap.target = self;
	self.projectionSnap.action = @selector(projectionSnapChanged:);
	self.projectionSnap.state = settings.snapLine ? NSControlStateValueOn : NSControlStateValueOff;
	self.projectionSnap.translatesAutoresizingMaskIntoConstraints = NO;
	[self.projectionSnap.heightAnchor constraintEqualToConstant:20].active = YES;
	[stack addArrangedSubview:self.projectionSnap];
	VSPushButton* grid = [[VSPushButton alloc] initWithTitle:@"Crea griglia di guide" prominent:NO target:self action:@selector(createProjectionGrid:)];
	[stack addArrangedSubview:grid];
	VSPushButton* pen = [[VSPushButton alloc] initWithTitle:@"Usa Penna Illustrator" prominent:NO target:self action:@selector(useNativePen:)];
	[stack addArrangedSubview:pen];
	[self.projectionSnap.widthAnchor constraintEqualToAnchor:stack.widthAnchor
											   constant:-24].active = YES;

	[stack addArrangedSubview:VSEyebrow(@"Trasforma selezione")];

	self.projectionTarget = [[VSSegmentedControl alloc]
		initWithLabels:@[@"Alto X", @"Alto Z", @"Sinistra", @"Destra"]
				target:self
				action:@selector(projectionTargetChanged:)];
	self.projectionTarget.selectedSegment = [self projectionTargetSelection];
	[stack addArrangedSubview:self.projectionTarget];
	[self.projectionTarget.widthAnchor constraintEqualToAnchor:stack.widthAnchor
											   constant:-24].active = YES;

	NSStackView* transformActions = [[NSStackView alloc] initWithFrame:NSZeroRect];
	transformActions.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	transformActions.distribution = NSStackViewDistributionFillEqually;
	transformActions.spacing = 7;
	transformActions.translatesAutoresizingMaskIntoConstraints = NO;
	[transformActions addArrangedSubview:
		[[VSPushButton alloc] initWithTitle:@"Proietta"
							 prominent:YES
								target:self
								action:@selector(projectionProjectSelection:)]];
	[transformActions addArrangedSubview:
		[[VSPushButton alloc] initWithTitle:@"Deproietta"
							 prominent:NO
								target:self
								action:@selector(projectionUnprojectSelection:)]];
	[stack addArrangedSubview:transformActions];
	[transformActions.widthAnchor constraintEqualToAnchor:stack.widthAnchor
												constant:-24].active = YES;

	self.projectionKeepOriginal = [[VSCheckbox alloc] initWithFrame:NSZeroRect];
	[self.projectionKeepOriginal setButtonType:NSButtonTypeSwitch];
	self.projectionKeepOriginal.bordered = NO;
	self.projectionKeepOriginal.title = @"Mantieni l’originale";
	self.projectionKeepOriginal.state = [NSUserDefaults.standardUserDefaults
		boolForKey:[self projectionPreferenceKey:@"keepOriginal"]]
		? NSControlStateValueOn
		: NSControlStateValueOff;
	self.projectionKeepOriginal.target = self;
	self.projectionKeepOriginal.action = @selector(projectionKeepOriginalChanged:);
	self.projectionKeepOriginal.translatesAutoresizingMaskIntoConstraints = NO;
	[self.projectionKeepOriginal.heightAnchor constraintEqualToConstant:20].active = YES;
	[stack addArrangedSubview:self.projectionKeepOriginal];
	[self.projectionKeepOriginal.widthAnchor constraintEqualToAnchor:stack.widthAnchor
												  constant:-24].active = YES;

	[stack addArrangedSubview:VSEyebrow(@"Sposta ed estrudi")];

	self.projectionMoveAxis = [[VSSegmentedControl alloc]
		initWithLabels:@[@"Asse X", @"Asse Z", @"Asse Y"]
				target:self
				action:@selector(projectionMoveAxisChanged:)];
	self.projectionMoveAxis.selectedSegment = settings.moveAxis;
	[stack addArrangedSubview:self.projectionMoveAxis];
	[self.projectionMoveAxis.widthAnchor constraintEqualToAnchor:stack.widthAnchor
												 constant:-24].active = YES;

	NSView* moveRow = [self projectionMoveDistanceRow:settings.moveDistance];
	[stack addArrangedSubview:moveRow];
	[moveRow.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	NSStackView* moveActions = [[NSStackView alloc] initWithFrame:NSZeroRect];
	moveActions.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	moveActions.distribution = NSStackViewDistributionFillEqually;
	moveActions.spacing = 7;
	moveActions.translatesAutoresizingMaskIntoConstraints = NO;
	[moveActions addArrangedSubview:
		[[VSPushButton alloc] initWithTitle:@"Sposta"
							 prominent:YES
								target:self
								action:@selector(projectionMoveSelection:)]];
	[moveActions addArrangedSubview:
		[[VSPushButton alloc] initWithTitle:@"Estrudi"
							 prominent:NO
								target:self
								action:@selector(projectionExtrudeSelection:)]];
	[stack addArrangedSubview:moveActions];
	[moveActions.widthAnchor constraintEqualToAnchor:stack.widthAnchor
											constant:-24].active = YES;

	[stack addArrangedSubview:VSEyebrow(@"Trasforma sul piano")];

	NSArray<NSView*>* planeRows = @[
		[self projectionTransformParameterRow:@"scaleU"
									label:@"Scala U (%)"
								  minimum:1.0
								  maximum:1000.0
									 value:settings.scaleU],
		[self projectionTransformParameterRow:@"scaleV"
									label:@"Scala V (%)"
								  minimum:1.0
								  maximum:1000.0
									 value:settings.scaleV],
		[self projectionTransformParameterRow:@"rotation"
									label:@"Rotazione (°)"
								  minimum:-360.0
								  maximum:360.0
									 value:settings.rotation],
		[self projectionTransformParameterRow:@"shear"
									label:@"Inclinazione (°)"
								  minimum:-80.0
								  maximum:80.0
									 value:settings.shear]
	];
	for (NSView* row in planeRows) {
		[stack addArrangedSubview:row];
		[row.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	}

	NSStackView* planeActions = [[NSStackView alloc] initWithFrame:NSZeroRect];
	planeActions.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	planeActions.distribution = NSStackViewDistributionFillEqually;
	planeActions.spacing = 7;
	planeActions.translatesAutoresizingMaskIntoConstraints = NO;
	[planeActions addArrangedSubview:
		[[VSPushButton alloc] initWithTitle:@"Scala"
							 prominent:YES
								target:self
								action:@selector(projectionScaleSelection:)]];
	[planeActions addArrangedSubview:
		[[VSPushButton alloc] initWithTitle:@"Ruota"
							 prominent:NO
								target:self
								action:@selector(projectionRotateSelection:)]];
	[planeActions addArrangedSubview:
		[[VSPushButton alloc] initWithTitle:@"Inclina"
							 prominent:NO
								target:self
								action:@selector(projectionShearSelection:)]];
	[stack addArrangedSubview:planeActions];
	[planeActions.widthAnchor constraintEqualToAnchor:stack.widthAnchor
											 constant:-24].active = YES;

	VSPushButton* measure = [[VSPushButton alloc] initWithTitle:@"Misura selezione"
										 prominent:NO
										target:self
										action:@selector(projectionMeasureSelection:)];
	[stack addArrangedSubview:measure];
	[measure.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	NSTextField* hint = VSLabel(
		@"Gli angoli valgono per tutti e quattro gli strumenti. Con Maiuscolo "
		@"premuto rettangoli ed ellissi restano quadrati sul piano. Mantieni "
		@"l’originale vale anche per spostamento e trasformazioni.",
		VSFontSmall(), VSQuiet());
	hint.lineBreakMode = NSLineBreakByWordWrapping;
	hint.maximumNumberOfLines = 3;
	[stack addArrangedSubview:hint];
	[hint.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	[self syncProjectionFields];
	return stack;
}

- (void)projectionToolChanged:(VSSegmentedControl*)sender
{
	NSInteger tool = sender.selectedSegment;
	if (tool < 0 || tool >= kVSPanelProjectionToolCount) tool = 0;
	[NSUserDefaults.standardUserDefaults
		setInteger:tool
			  forKey:[self projectionPreferenceKey:@"tool"]];
	if (self.activateTool) {
		self.activateTool(self.callbackContext,
			kVSPanelProjectionToolBase + (int)tool);
	}
}

- (void)projectionTargetChanged:(VSSegmentedControl*)sender
{
	NSInteger target = sender.selectedSegment;
	if (target < 0 || target >= kVSPanelProjectionCommandCount) target = 0;
	[NSUserDefaults.standardUserDefaults
		setInteger:target
			  forKey:[self projectionPreferenceKey:@"target"]];
}

- (void)projectionKeepOriginalChanged:(NSButton*)sender
{
	[NSUserDefaults.standardUserDefaults
		setBool:sender.state == NSControlStateValueOn
		   forKey:[self projectionPreferenceKey:@"keepOriginal"]];
}

- (void)runProjectionSelectionCommand:(int)base
{
	if (!self.activateTool) return;
	int command = base + (int)[self projectionTargetSelection];
	if (self.projectionKeepOriginal.state == NSControlStateValueOn) {
		command += kVSPanelProjectionCopyOffset;
	}
	self.activateTool(self.callbackContext, command);
}

- (void)projectionProjectSelection:(id)sender
{
	[self runProjectionSelectionCommand:kVSPanelProjectionProjectBase];
}

- (void)projectionUnprojectSelection:(id)sender
{
	[self runProjectionSelectionCommand:kVSPanelProjectionUnprojectBase];
}

- (void)projectionMoveAxisChanged:(VSSegmentedControl*)sender
{
	VSProjectionSettings settings = VSProjectionGet();
	NSInteger axis = sender.selectedSegment;
	if (axis < 0 || axis >= kVSPanelProjectionAxisCount) axis = 0;
	settings.moveAxis = (int)axis;
	[self storeProjection:settings];
}

- (void)projectionMoveDistanceCommitted:(NSTextField*)sender
{
	VSProjectionSettings settings = VSProjectionGet();
	settings.moveDistance = MAX(-2000.0, MIN(2000.0, sender.doubleValue));
	[self storeProjection:settings];
	self.projectionSliders[@"move"].doubleValue = settings.moveDistance;
	[self syncProjectionFields];
}

- (void)runProjectionAxisCommand:(int)base keepOriginal:(BOOL)keepOriginal
{
	if (!self.activateTool) return;
	NSInteger axis = self.projectionMoveAxis.selectedSegment;
	if (axis < 0 || axis >= kVSPanelProjectionAxisCount) axis = 0;
	int command = base + (int)axis;
	if (keepOriginal &&
		self.projectionKeepOriginal.state == NSControlStateValueOn) {
		command += kVSPanelProjectionCopyOffset;
	}
	self.activateTool(self.callbackContext, command);
}

- (void)projectionMoveSelection:(id)sender
{
	[self runProjectionAxisCommand:kVSPanelProjectionMoveBase keepOriginal:YES];
}

- (void)projectionExtrudeSelection:(id)sender
{
	[self runProjectionAxisCommand:kVSPanelProjectionExtrudeBase keepOriginal:NO];
}

- (void)storeProjectionTransformValue:(double)value key:(NSString*)key
{
	VSProjectionSettings settings = VSProjectionGet();
	if ([key isEqualToString:@"scaleU"]) settings.scaleU = value;
	else if ([key isEqualToString:@"scaleV"]) settings.scaleV = value;
	else if ([key isEqualToString:@"rotation"]) settings.rotation = value;
	else if ([key isEqualToString:@"shear"]) settings.shear = value;
	else return;
	[self storeProjection:settings];
	[self syncProjectionFields];
}

- (void)projectionTransformSliderChanged:(NSSlider*)sender
{
	[self storeProjectionTransformValue:sender.doubleValue key:sender.identifier];
}

- (void)projectionTransformValueCommitted:(NSTextField*)sender
{
	[self storeProjectionTransformValue:sender.doubleValue key:sender.identifier];
}

- (void)runProjectionPlaneTransform:(int)command
{
	if (!self.activateTool) return;
	if (self.projectionKeepOriginal.state == NSControlStateValueOn) {
		command += kVSPanelProjectionCopyOffset;
	}
	self.activateTool(self.callbackContext, command);
}

- (void)projectionScaleSelection:(id)sender
{
	[self runProjectionPlaneTransform:kVSPanelProjectionScale];
}

- (void)projectionRotateSelection:(id)sender
{
	[self runProjectionPlaneTransform:kVSPanelProjectionRotate];
}

- (void)projectionShearSelection:(id)sender
{
	[self runProjectionPlaneTransform:kVSPanelProjectionShear];
}

- (void)projectionMeasureSelection:(id)sender
{
	if (self.activateTool) {
		self.activateTool(self.callbackContext, kVSPanelProjectionMeasure);
	}
}

- (void)projectionSliderChanged:(NSSlider*)sender
{
	VSProjectionSettings settings = VSProjectionGet();
	if ([sender.identifier isEqualToString:@"left"]) {
		settings.leftAngle = sender.doubleValue;
	}
	else if ([sender.identifier isEqualToString:@"right"]) {
		settings.rightAngle = sender.doubleValue;
	}
	else if ([sender.identifier isEqualToString:@"move"]) {
		settings.moveDistance = sender.doubleValue;
	}
	[self storeProjection:settings];
	[self syncProjectionFields];
}

- (void)projectionPresetChanged:(VSSegmentedControl*)sender
{
	VSProjectionSettings settings = VSProjectionGet();
	if (sender.selectedSegment == 0) {
		settings.leftAngle = 30.0;
		settings.rightAngle = 30.0;
	}
	else if (sender.selectedSegment == 1) {
		// Dimetrica secondo la convenzione del disegno tecnico: 7° e 42°.
		settings.leftAngle = 7.0;
		settings.rightAngle = 42.0;
	}
	else if (sender.selectedSegment == 2) {
		settings.leftAngle = 45.0; settings.rightAngle = 0.0;
	}
	else if (sender.selectedSegment == 3) {
		settings.leftAngle = 30.0; settings.rightAngle = 60.0;
	}
	else {
		// «Libera» non impone angoli: restano quelli scelti con i cursori.
		[self syncProjectionFields];
		return;
	}
	[self storeProjection:settings];
	self.projectionSliders[@"left"].doubleValue = settings.leftAngle;
	self.projectionSliders[@"right"].doubleValue = settings.rightAngle;
	[self syncProjectionFields];
}

- (void)projectionPlaneChanged:(VSSegmentedControl*)sender
{
	VSProjectionSettings settings = VSProjectionGet();
	settings.plane = (int)sender.selectedSegment;
	[self storeProjection:settings];
}

- (void)projectionSnapChanged:(NSButton*)sender
{
	VSProjectionSettings settings = VSProjectionGet();
	settings.snapLine = sender.state == NSControlStateValueOn ? 1 : 0;
	[self storeProjection:settings];
}

#pragma mark Trasformazioni

- (void)createProjectionGrid:(id)sender
{
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelProjectionGrid);
}
- (void)useNativePen:(id)sender
{
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelNativePen);
}

- (NSString*)transformPreferenceKey:(NSString*)module key:(NSString*)key
{
	return [NSString stringWithFormat:@"studio.vectorsuite.%@.%@", module, key];
}

- (double)storedTransformValue:(NSString*)module
							key:(NSString*)key
					 defaultValue:(double)defaultValue
{
	NSString* preference = [self transformPreferenceKey:module key:key];
	id stored = [NSUserDefaults.standardUserDefaults objectForKey:preference];
	return stored ? [stored doubleValue] : defaultValue;
}

- (BOOL)storedTransformFlag:(NSString*)module
						  key:(NSString*)key
				 defaultValue:(BOOL)defaultValue
{
	NSString* preference = [self transformPreferenceKey:module key:key];
	id stored = [NSUserDefaults.standardUserDefaults objectForKey:preference];
	return stored ? [stored boolValue] : defaultValue;
}

- (NSView*)transformFieldRow:(NSString*)title
						 key:(NSString*)key
					   value:(double)value
					decimals:(NSInteger)decimals
{
	NSStackView* row = [[NSStackView alloc] initWithFrame:NSZeroRect];
	row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	row.alignment = NSLayoutAttributeCenterY;
	row.spacing = 8;
	row.translatesAutoresizingMaskIntoConstraints = NO;

	NSTextField* label = VSLabel(title, VSFontBody(), VSMuted());
	[label.widthAnchor constraintEqualToConstant:150].active = YES;
	[row addArrangedSubview:label];

	NSTextField* field = [[VSNumericField alloc] initWithFrame:NSZeroRect];
	field.identifier = key;
	field.stringValue = [NSString stringWithFormat:
		decimals == 0 ? @"%.0f" : (decimals == 1 ? @"%.1f" : @"%.2f"),
		value];
	field.alignment = NSTextAlignmentRight;
	field.font = VSFontMono();
	field.textColor = VSInk();
	field.bezeled = YES;
	field.bezelStyle = NSTextFieldRoundedBezel;
	field.translatesAutoresizingMaskIntoConstraints = NO;
	[field.widthAnchor constraintEqualToConstant:76].active = YES;
	[row addArrangedSubview:field];
	self.transformFields[key] = field;
	return row;
}

- (void)addTransformRow:(NSView*)row toStack:(NSStackView*)stack
{
	[stack addArrangedSubview:row];
	[row.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
}

- (NSStackView*)transformSettingsStack
{
	self.transformFields = [NSMutableDictionary dictionary];
	NSStackView* stack = [[NSStackView alloc] initWithFrame:NSZeroRect];
	stack.orientation = NSUserInterfaceLayoutOrientationVertical;
	stack.alignment = NSLayoutAttributeLeading;
	stack.spacing = 7;
	stack.edgeInsets = NSEdgeInsetsMake(12, 12, 14, 12);
	stack.translatesAutoresizingMaskIntoConstraints = NO;
	return stack;
}

- (NSView*)collisionSettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Allineamento a contatto")];
	NSTextField* explanation = VSLabel(
		@"Il primo oggetto selezionato è il riferimento; gli altri vengono "
		 @"disposti in sequenza senza sovrapposizione.",
		VSFontSmall(),
		VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	explanation.maximumNumberOfLines = 3;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	VSCollisionSettings settings = VSCollisionDefaults();
	settings.direction = (int)[self storedTransformValue:@"collision"
		key:@"direction" defaultValue:settings.direction];
	settings.gap = [self storedTransformValue:@"collision"
		key:@"gap" defaultValue:settings.gap];
	settings.alignCenters = [self storedTransformFlag:@"collision"
		key:@"alignCenters" defaultValue:YES] ? 1 : 0;
	settings = VSSanitizeCollision(settings);

	self.collisionDirection = [[VSSegmentedControl alloc]
		initWithLabels:@[@"Destra", @"Sinistra", @"Su", @"Giù"]
			target:nil
			action:nil];
	self.collisionDirection.selectedSegment = settings.direction;
	[stack addArrangedSubview:self.collisionDirection];
	[self.collisionDirection.widthAnchor constraintEqualToAnchor:stack.widthAnchor
		constant:-24].active = YES;

	[self addTransformRow:[self transformFieldRow:@"Distanza" key:@"gap"
		value:settings.gap decimals:2] toStack:stack];

	VSCheckbox* centers = [[VSCheckbox alloc] initWithFrame:NSZeroRect];
	[centers setButtonType:NSButtonTypeSwitch];
	centers.bordered = NO;
	centers.title = @"Allinea anche i centri";
	centers.state = settings.alignCenters ? NSControlStateValueOn : NSControlStateValueOff;
	centers.translatesAutoresizingMaskIntoConstraints = NO;
	self.collisionAlignCenters = centers;
	[stack addArrangedSubview:centers];

	VSPushButton* apply = [[VSPushButton alloc]
		initWithTitle:@"Allinea selezione"
		prominent:YES
		target:self
		action:@selector(applyCollision:)];
	[stack addArrangedSubview:apply];
	[apply.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSCollisionSet(&settings);
	return stack;
}

- (void)applyCollision:(id)sender
{
	VSCollisionSettings settings = VSCollisionDefaults();
	settings.direction = (int)self.collisionDirection.selectedSegment;
	settings.gap = self.transformFields[@"gap"].doubleValue;
	settings.alignCenters = self.collisionAlignCenters.state == NSControlStateValueOn ? 1 : 0;
	settings = VSSanitizeCollision(settings);
	NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
	[defaults setInteger:settings.direction forKey:[self transformPreferenceKey:@"collision" key:@"direction"]];
	[defaults setDouble:settings.gap forKey:[self transformPreferenceKey:@"collision" key:@"gap"]];
	[defaults setBool:settings.alignCenters forKey:[self transformPreferenceKey:@"collision" key:@"alignCenters"]];
	VSCollisionSet(&settings);
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelCollisionApply);
}

- (NSView*)mirrorSettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Simmetria")];
	NSTextField* explanation = VSLabel(
		@"Crea copie rispetto al centro complessivo della selezione. La modalità "
		 @"radiale distribuisce il numero di copie indicato su 360°.",
		VSFontSmall(),
		VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	explanation.maximumNumberOfLines = 3;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	VSMirrorSettings settings = VSMirrorDefaults();
	settings.mode = (int)[self storedTransformValue:@"mirror" key:@"mode"
		defaultValue:settings.mode];
	settings.copies = (int)[self storedTransformValue:@"mirror" key:@"copies"
		defaultValue:settings.copies];
	settings.axisOffset = [self storedTransformValue:@"mirror" key:@"axisOffset"
		defaultValue:settings.axisOffset];
	settings = VSSanitizeMirror(settings);

	self.mirrorMode = [[VSSegmentedControl alloc]
		initWithLabels:@[@"Verticale", @"Orizzontale", @"Entrambi", @"Radiale"]
			target:self
			action:@selector(mirrorSettingsChanged:)];
	self.mirrorMode.selectedSegment = settings.mode;
	[stack addArrangedSubview:self.mirrorMode];
	[self.mirrorMode.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	[self addTransformRow:[self transformFieldRow:@"Copie radiali (2–32)"
		key:@"copies" value:settings.copies decimals:0] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Spostamento asse"
		key:@"axisOffset" value:settings.axisOffset decimals:2] toStack:stack];
	for (NSString* key in @[@"copies", @"axisOffset"]) {
		self.transformFields[key].target = self;
		self.transformFields[key].action = @selector(mirrorSettingsChanged:);
		self.transformFields[key].delegate = self;
	}
	self.transformFields[@"copies"].enabled = settings.mode == kVSMirrorRadial;

	VSPushButton* apply = [[VSPushButton alloc]
		initWithTitle:@"Crea simmetria"
		prominent:YES
		target:self
		action:@selector(applyMirror:)];
	[stack addArrangedSubview:apply];
	[apply.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSMirrorSet(&settings);
	return stack;
}

- (void)mirrorSettingsChanged:(id)sender
{
	VSMirrorSettings settings = VSMirrorDefaults();
	settings.mode = (int)self.mirrorMode.selectedSegment;
	settings.copies = self.transformFields[@"copies"].intValue;
	settings.axisOffset = self.transformFields[@"axisOffset"].doubleValue;
	settings = VSSanitizeMirror(settings);
	NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
	[defaults setInteger:settings.mode forKey:[self transformPreferenceKey:@"mirror" key:@"mode"]];
	[defaults setInteger:settings.copies forKey:[self transformPreferenceKey:@"mirror" key:@"copies"]];
	[defaults setDouble:settings.axisOffset forKey:[self transformPreferenceKey:@"mirror" key:@"axisOffset"]];
	VSMirrorSet(&settings);
	self.transformFields[@"copies"].enabled = settings.mode == kVSMirrorRadial;
}

- (void)applyMirror:(id)sender
{
	[self.view.window makeFirstResponder:nil];
	[self mirrorSettingsChanged:sender];
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelMirrorApply);
}

- (NSView*)randomizeSettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Trasformazione casuale")];
	NSTextField* explanation = VSLabel(
		@"Lo stesso seme produce sempre la stessa variazione. Tutti i valori "
		 @"sono limiti massimi, applicati attorno al centro di ogni oggetto.",
		VSFontSmall(),
		VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	explanation.maximumNumberOfLines = 3;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	VSRandomizeSettings settings = VSRandomizeDefaults();
	settings.seed = (std::uint32_t)[self storedTransformValue:@"randomize" key:@"seed"
		defaultValue:settings.seed];
	settings.positionX = [self storedTransformValue:@"randomize" key:@"positionX"
		defaultValue:settings.positionX];
	settings.positionY = [self storedTransformValue:@"randomize" key:@"positionY"
		defaultValue:settings.positionY];
	settings.rotation = [self storedTransformValue:@"randomize" key:@"rotation"
		defaultValue:settings.rotation];
	settings.scaleMinimum = [self storedTransformValue:@"randomize" key:@"scaleMinimum"
		defaultValue:settings.scaleMinimum];
	settings.scaleMaximum = [self storedTransformValue:@"randomize" key:@"scaleMaximum"
		defaultValue:settings.scaleMaximum];
	settings.uniformScale = [self storedTransformFlag:@"randomize" key:@"uniformScale"
		defaultValue:YES] ? 1 : 0;
	settings.distribution = (int)[self storedTransformValue:@"randomize" key:@"distribution"
		defaultValue:settings.distribution];
	settings = VSSanitizeRandomize(settings);

	self.randomDistribution = [[VSSegmentedControl alloc]
		initWithLabels:@[@"Uniforme", @"Centrata"] target:nil action:nil];
	self.randomDistribution.selectedSegment = settings.distribution;
	[stack addArrangedSubview:self.randomDistribution];
	[self.randomDistribution.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	[self addTransformRow:[self transformFieldRow:@"Seme" key:@"seed"
		value:settings.seed decimals:0] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Posizione X (pt)" key:@"positionX"
		value:settings.positionX decimals:1] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Posizione Y (pt)" key:@"positionY"
		value:settings.positionY decimals:1] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Rotazione (°)" key:@"rotation"
		value:settings.rotation decimals:1] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Scala minima" key:@"scaleMinimum"
		value:settings.scaleMinimum decimals:2] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Scala massima" key:@"scaleMaximum"
		value:settings.scaleMaximum decimals:2] toStack:stack];

	VSCheckbox* uniform = [[VSCheckbox alloc] initWithFrame:NSZeroRect];
	[uniform setButtonType:NSButtonTypeSwitch];
	uniform.bordered = NO;
	uniform.title = @"Scala uniforme";
	uniform.state = settings.uniformScale ? NSControlStateValueOn : NSControlStateValueOff;
	uniform.translatesAutoresizingMaskIntoConstraints = NO;
	self.randomUniformScale = uniform;
	[stack addArrangedSubview:uniform];

	NSStackView* actions = [[NSStackView alloc] initWithFrame:NSZeroRect];
	actions.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	actions.distribution = NSStackViewDistributionFillEqually;
	actions.spacing = 7;
	actions.translatesAutoresizingMaskIntoConstraints = NO;
	[actions addArrangedSubview:[[VSPushButton alloc] initWithTitle:@"Nuovo seme"
		prominent:NO target:self action:@selector(newRandomSeed:)]];
	[actions addArrangedSubview:[[VSPushButton alloc] initWithTitle:@"Applica"
		prominent:YES target:self action:@selector(applyRandomize:)]];
	[stack addArrangedSubview:actions];
	[actions.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSRandomizeSet(&settings);
	return stack;
}

- (void)newRandomSeed:(id)sender
{
	std::uint32_t seed = arc4random_uniform(999999u) + 1u;
	self.transformFields[@"seed"].integerValue = seed;
}

- (void)applyRandomize:(id)sender
{
	VSRandomizeSettings settings = VSRandomizeDefaults();
	settings.seed = (std::uint32_t)MAX(1, self.transformFields[@"seed"].integerValue);
	settings.positionX = self.transformFields[@"positionX"].doubleValue;
	settings.positionY = self.transformFields[@"positionY"].doubleValue;
	settings.rotation = self.transformFields[@"rotation"].doubleValue;
	settings.scaleMinimum = self.transformFields[@"scaleMinimum"].doubleValue;
	settings.scaleMaximum = self.transformFields[@"scaleMaximum"].doubleValue;
	settings.uniformScale = self.randomUniformScale.state == NSControlStateValueOn ? 1 : 0;
	settings.distribution = (int)self.randomDistribution.selectedSegment;
	settings = VSSanitizeRandomize(settings);

	NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
	[defaults setInteger:settings.seed forKey:[self transformPreferenceKey:@"randomize" key:@"seed"]];
	[defaults setDouble:settings.positionX forKey:[self transformPreferenceKey:@"randomize" key:@"positionX"]];
	[defaults setDouble:settings.positionY forKey:[self transformPreferenceKey:@"randomize" key:@"positionY"]];
	[defaults setDouble:settings.rotation forKey:[self transformPreferenceKey:@"randomize" key:@"rotation"]];
	[defaults setDouble:settings.scaleMinimum forKey:[self transformPreferenceKey:@"randomize" key:@"scaleMinimum"]];
	[defaults setDouble:settings.scaleMaximum forKey:[self transformPreferenceKey:@"randomize" key:@"scaleMaximum"]];
	[defaults setBool:settings.uniformScale forKey:[self transformPreferenceKey:@"randomize" key:@"uniformScale"]];
	[defaults setInteger:settings.distribution forKey:[self transformPreferenceKey:@"randomize" key:@"distribution"]];
	VSRandomizeSet(&settings);
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelRandomizeApply);
}

#pragma mark Traccia e aspetto

- (NSView*)widthSettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Traccia")];
	NSTextField* explanation = VSLabel(
		@"Imposta uno spessore esatto oppure moltiplica quello esistente. "
		 @"L’operazione attraversa gruppi e tracciati composti.",
		VSFontSmall(),
		VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	explanation.maximumNumberOfLines = 3;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	VSWidthSettings settings = VSWidthDefaults();
	settings.mode = (int)[self storedTransformValue:@"width" key:@"mode"
		defaultValue:settings.mode];
	settings.value = [self storedTransformValue:@"width" key:@"value"
		defaultValue:settings.value];
	settings.cap = (int)[self storedTransformValue:@"width" key:@"cap"
		defaultValue:settings.cap];
	settings.join = (int)[self storedTransformValue:@"width" key:@"join"
		defaultValue:settings.join];
	settings = VSSanitizeWidth(settings);

	self.widthMode = [[VSSegmentedControl alloc]
		initWithLabels:@[@"Spessore esatto", @"Moltiplicatore"] target:nil action:nil];
	self.widthMode.selectedSegment = settings.mode;
	[stack addArrangedSubview:self.widthMode];
	[self.widthMode.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	[self addTransformRow:[self transformFieldRow:@"Valore" key:@"widthValue"
		value:settings.value decimals:2] toStack:stack];

	[stack addArrangedSubview:VSEyebrow(@"Terminali")];
	self.widthCap = [[VSSegmentedControl alloc]
		initWithLabels:@[@"Netto", @"Tondo", @"Esteso"] target:nil action:nil];
	self.widthCap.selectedSegment = settings.cap;
	[stack addArrangedSubview:self.widthCap];
	[self.widthCap.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	self.widthJoin = [[VSSegmentedControl alloc]
		initWithLabels:@[@"Mitra", @"Tondo", @"Smusso"] target:nil action:nil];
	self.widthJoin.selectedSegment = settings.join;
	[stack addArrangedSubview:self.widthJoin];
	[self.widthJoin.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	VSPushButton* apply = [[VSPushButton alloc]
		initWithTitle:@"Applica alla selezione" prominent:YES target:self
		action:@selector(applyWidth:)];
	[stack addArrangedSubview:apply];
	[apply.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSWidthSet(&settings);
	return stack;
}

- (void)applyWidth:(id)sender
{
	VSWidthSettings settings = VSWidthDefaults();
	settings.mode = (int)self.widthMode.selectedSegment;
	settings.value = self.transformFields[@"widthValue"].doubleValue;
	settings.cap = (int)self.widthCap.selectedSegment;
	settings.join = (int)self.widthJoin.selectedSegment;
	settings = VSSanitizeWidth(settings);
	NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
	[defaults setInteger:settings.mode forKey:[self transformPreferenceKey:@"width" key:@"mode"]];
	[defaults setDouble:settings.value forKey:[self transformPreferenceKey:@"width" key:@"value"]];
	[defaults setInteger:settings.cap forKey:[self transformPreferenceKey:@"width" key:@"cap"]];
	[defaults setInteger:settings.join forKey:[self transformPreferenceKey:@"width" key:@"join"]];
	VSWidthSet(&settings);
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelWidthApply);
}

- (NSView*)liveStyleSettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Composizione")];
	NSTextField* explanation = VSLabel(
		@"Applica opacità e metodo di fusione in modo non distruttivo "
		 @"all’oggetto o al gruppo selezionato.",
		VSFontSmall(),
		VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	explanation.maximumNumberOfLines = 3;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	VSLiveStyleSettings settings = VSLiveStyleDefaults();
	settings.opacity = [self storedTransformValue:@"liveStyle" key:@"opacity"
		defaultValue:settings.opacity];
	settings.blendMode = (int)[self storedTransformValue:@"liveStyle" key:@"blendMode"
		defaultValue:settings.blendMode];
	settings.isolated = [self storedTransformFlag:@"liveStyle" key:@"isolated"
		defaultValue:NO] ? 1 : 0;
	settings = VSSanitizeLiveStyle(settings);

	[self addTransformRow:[self transformFieldRow:@"Opacità (0–100%)"
		key:@"opacity" value:settings.opacity * 100.0 decimals:1] toStack:stack];
	self.liveBlendValues = @[@0, @1, @2, @3];
	self.liveBlendMode = [[VSSegmentedControl alloc]
		initWithLabels:@[@"Normale", @"Moltiplica", @"Scolora", @"Sovrapponi"]
			target:nil action:nil];
	NSUInteger blendIndex = [self.liveBlendValues indexOfObject:@(settings.blendMode)];
	self.liveBlendMode.selectedSegment = blendIndex == NSNotFound ? 0 : blendIndex;
	[stack addArrangedSubview:self.liveBlendMode];
	[self.liveBlendMode.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	VSCheckbox* isolated = [[VSCheckbox alloc] initWithFrame:NSZeroRect];
	[isolated setButtonType:NSButtonTypeSwitch];
	isolated.bordered = NO;
	isolated.title = @"Isola la fusione nel gruppo";
	isolated.state = settings.isolated ? NSControlStateValueOn : NSControlStateValueOff;
	isolated.translatesAutoresizingMaskIntoConstraints = NO;
	self.liveIsolated = isolated;
	[stack addArrangedSubview:isolated];

	VSPushButton* apply = [[VSPushButton alloc]
		initWithTitle:@"Applica stile" prominent:YES target:self
		action:@selector(applyLiveStyle:)];
	[stack addArrangedSubview:apply];
	[apply.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSLiveStyleSet(&settings);
	return stack;
}

- (void)applyLiveStyle:(id)sender
{
	VSLiveStyleSettings settings = VSLiveStyleDefaults();
	settings.opacity = self.transformFields[@"opacity"].doubleValue / 100.0;
	NSInteger blendIndex = self.liveBlendMode.selectedSegment;
	if (blendIndex < 0 || blendIndex >= (NSInteger)self.liveBlendValues.count) blendIndex = 0;
	settings.blendMode = self.liveBlendValues[blendIndex].intValue;
	settings.isolated = self.liveIsolated.state == NSControlStateValueOn ? 1 : 0;
	settings = VSSanitizeLiveStyle(settings);
	NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
	[defaults setDouble:settings.opacity forKey:[self transformPreferenceKey:@"liveStyle" key:@"opacity"]];
	[defaults setInteger:settings.blendMode forKey:[self transformPreferenceKey:@"liveStyle" key:@"blendMode"]];
	[defaults setBool:settings.isolated forKey:[self transformPreferenceKey:@"liveStyle" key:@"isolated"]];
	VSLiveStyleSet(&settings);
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelLiveStyleApply);
}

- (NSView*)colorSettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Regolazione colore")];
	NSTextField* explanation = VSLabel(
		@"Regola riempimenti e tracce RGB, CMYK e scala di grigio. "
		 @"Colori campione, pattern e gradienti restano invariati.",
		VSFontSmall(),
		VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	explanation.maximumNumberOfLines = 3;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	VSColorSettings settings = VSColorDefaults();
	settings.brightness = [self storedTransformValue:@"color" key:@"brightness"
		defaultValue:settings.brightness];
	settings.contrast = [self storedTransformValue:@"color" key:@"contrast"
		defaultValue:settings.contrast];
	settings.saturation = [self storedTransformValue:@"color" key:@"saturation"
		defaultValue:settings.saturation];
	settings.hueDegrees = [self storedTransformValue:@"color" key:@"hueDegrees"
		defaultValue:settings.hueDegrees];
	settings = VSSanitizeColor(settings);

	[self addTransformRow:[self transformFieldRow:@"Luminosità (−100…100)"
		key:@"brightness" value:settings.brightness * 100 decimals:1] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Contrasto (−100…100)"
		key:@"contrast" value:settings.contrast * 100 decimals:1] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Saturazione (−100…100)"
		key:@"saturation" value:settings.saturation * 100 decimals:1] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Tonalità (−180…180°)"
		key:@"hueDegrees" value:settings.hueDegrees decimals:1] toStack:stack];

	NSStackView* actions = [[NSStackView alloc] initWithFrame:NSZeroRect];
	actions.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	actions.distribution = NSStackViewDistributionFillEqually;
	actions.spacing = 7;
	actions.translatesAutoresizingMaskIntoConstraints = NO;
	[actions addArrangedSubview:[[VSPushButton alloc] initWithTitle:@"Azzera"
		prominent:NO target:self action:@selector(resetColor:)]];
	[actions addArrangedSubview:[[VSPushButton alloc] initWithTitle:@"Applica"
		prominent:YES target:self action:@selector(applyColor:)]];
	[stack addArrangedSubview:actions];
	[actions.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSColorSet(&settings);
	return stack;
}

- (void)resetColor:(id)sender
{
	for (NSString* key in @[@"brightness", @"contrast", @"saturation", @"hueDegrees"]) {
		self.transformFields[key].doubleValue = 0;
	}
}

- (void)applyColor:(id)sender
{
	VSColorSettings settings = VSColorDefaults();
	settings.brightness = self.transformFields[@"brightness"].doubleValue / 100.0;
	settings.contrast = self.transformFields[@"contrast"].doubleValue / 100.0;
	settings.saturation = self.transformFields[@"saturation"].doubleValue / 100.0;
	settings.hueDegrees = self.transformFields[@"hueDegrees"].doubleValue;
	settings = VSSanitizeColor(settings);
	NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
	[defaults setDouble:settings.brightness forKey:[self transformPreferenceKey:@"color" key:@"brightness"]];
	[defaults setDouble:settings.contrast forKey:[self transformPreferenceKey:@"color" key:@"contrast"]];
	[defaults setDouble:settings.saturation forKey:[self transformPreferenceKey:@"color" key:@"saturation"]];
	[defaults setDouble:settings.hueDegrees forKey:[self transformPreferenceKey:@"color" key:@"hueDegrees"]];
	VSColorSet(&settings);
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelColorApply);
}

#pragma mark Strumenti di disegno

- (NSView*)precisionSettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Penna parametrica")];
	NSTextField* explanation = VSLabel(
		@"Trascina sulla tavola. L’angolo viene vincolato al passo indicato; "
		 @"la lunghezza zero usa la distanza del cursore.",
		VSFontSmall(), VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	explanation.maximumNumberOfLines = 3;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	VSPrecisionSettings settings = VSPrecisionDefaults();
	settings.angleStep = [self storedTransformValue:@"precision" key:@"angleStep"
		defaultValue:settings.angleStep];
	settings.fixedLength = [self storedTransformValue:@"precision" key:@"fixedLength"
		defaultValue:settings.fixedLength];
	settings.curveAmount = [self storedTransformValue:@"precision" key:@"curveAmount"
		defaultValue:settings.curveAmount];
	settings.strokeWidth = [self storedTransformValue:@"precision" key:@"strokeWidth"
		defaultValue:settings.strokeWidth];
	settings = VSSanitizePrecision(settings);
	[self addTransformRow:[self transformFieldRow:@"Passo angolare (°)" key:@"precisionAngle"
		value:settings.angleStep decimals:1] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Lunghezza fissa (pt)" key:@"precisionLength"
		value:settings.fixedLength decimals:1] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Curvatura (−100…100)" key:@"precisionCurve"
		value:settings.curveAmount * 100 decimals:1] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Spessore (pt)" key:@"precisionStroke"
		value:settings.strokeWidth decimals:2] toStack:stack];
	VSPushButton* activate = [[VSPushButton alloc]
		initWithTitle:@"Attiva Precision Pen" prominent:YES target:self
		action:@selector(applyPrecision:)];
	[stack addArrangedSubview:activate];
	[activate.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSPrecisionSet(&settings);
	return stack;
}

- (void)applyPrecision:(id)sender
{
	VSPrecisionSettings settings = VSPrecisionDefaults();
	settings.angleStep = self.transformFields[@"precisionAngle"].doubleValue;
	settings.fixedLength = self.transformFields[@"precisionLength"].doubleValue;
	settings.curveAmount = self.transformFields[@"precisionCurve"].doubleValue / 100.0;
	settings.strokeWidth = self.transformFields[@"precisionStroke"].doubleValue;
	settings = VSSanitizePrecision(settings);
	NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
	[defaults setDouble:settings.angleStep forKey:[self transformPreferenceKey:@"precision" key:@"angleStep"]];
	[defaults setDouble:settings.fixedLength forKey:[self transformPreferenceKey:@"precision" key:@"fixedLength"]];
	[defaults setDouble:settings.curveAmount forKey:[self transformPreferenceKey:@"precision" key:@"curveAmount"]];
	[defaults setDouble:settings.strokeWidth forKey:[self transformPreferenceKey:@"precision" key:@"strokeWidth"]];
	VSPrecisionSet(&settings);
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelPrecisionConfigure);
}

- (NSView*)fluidSettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Disegno fluido")];
	NSTextField* explanation = VSLabel(
		@"Trascina per creare un tratto Bezier smussato. Campionamento e "
		 @"smoothing vengono applicati durante l’anteprima.",
		VSFontSmall(), VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	explanation.maximumNumberOfLines = 3;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	VSFluidSettings settings = VSFluidDefaults();
	settings.smoothing = [self storedTransformValue:@"fluid" key:@"smoothing"
		defaultValue:settings.smoothing];
	settings.strokeWidth = [self storedTransformValue:@"fluid" key:@"strokeWidth"
		defaultValue:settings.strokeWidth];
	settings.sampleDistance = [self storedTransformValue:@"fluid" key:@"sampleDistance"
		defaultValue:settings.sampleDistance];
	settings.closePath = [self storedTransformFlag:@"fluid" key:@"closePath"
		defaultValue:NO] ? 1 : 0;
	settings = VSSanitizeFluid(settings);
	[self addTransformRow:[self transformFieldRow:@"Smoothing (0–100)" key:@"fluidSmoothing"
		value:settings.smoothing * 100 decimals:1] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Spessore (pt)" key:@"fluidStroke"
		value:settings.strokeWidth decimals:2] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Campionamento (pt)" key:@"fluidSample"
		value:settings.sampleDistance decimals:2] toStack:stack];
	VSCheckbox* close = [[VSCheckbox alloc] initWithFrame:NSZeroRect];
	[close setButtonType:NSButtonTypeSwitch];
	close.bordered = NO;
	close.title = @"Chiudi il tracciato";
	close.state = settings.closePath ? NSControlStateValueOn : NSControlStateValueOff;
	close.translatesAutoresizingMaskIntoConstraints = NO;
	self.fluidClosePath = close;
	[stack addArrangedSubview:close];
	VSPushButton* activate = [[VSPushButton alloc]
		initWithTitle:@"Attiva Fluid Sketch" prominent:YES target:self
		action:@selector(applyFluid:)];
	[stack addArrangedSubview:activate];
	[activate.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSFluidSet(&settings);
	return stack;
}

- (void)applyFluid:(id)sender
{
	VSFluidSettings settings = VSFluidDefaults();
	settings.smoothing = self.transformFields[@"fluidSmoothing"].doubleValue / 100.0;
	settings.strokeWidth = self.transformFields[@"fluidStroke"].doubleValue;
	settings.sampleDistance = self.transformFields[@"fluidSample"].doubleValue;
	settings.closePath = self.fluidClosePath.state == NSControlStateValueOn ? 1 : 0;
	settings = VSSanitizeFluid(settings);
	NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
	[defaults setDouble:settings.smoothing forKey:[self transformPreferenceKey:@"fluid" key:@"smoothing"]];
	[defaults setDouble:settings.strokeWidth forKey:[self transformPreferenceKey:@"fluid" key:@"strokeWidth"]];
	[defaults setDouble:settings.sampleDistance forKey:[self transformPreferenceKey:@"fluid" key:@"sampleDistance"]];
	[defaults setBool:settings.closePath forKey:[self transformPreferenceKey:@"fluid" key:@"closePath"]];
	VSFluidSet(&settings);
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelFluidConfigure);
}

- (NSView*)inkSettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Pennino calligrafico")];
	NSTextField* explanation = VSLabel(
		@"Crea una sagoma vettoriale chiusa e piena. L’aspetto controlla "
		 @"l’ellisse del pennino, l’angolo ne stabilisce l’inclinazione.",
		VSFontSmall(), VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	explanation.maximumNumberOfLines = 3;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	VSInkSettings settings = VSInkDefaults();
	settings.smoothing = [self storedTransformValue:@"ink" key:@"smoothing"
		defaultValue:settings.smoothing];
	settings.nibWidth = [self storedTransformValue:@"ink" key:@"nibWidth"
		defaultValue:settings.nibWidth];
	settings.nibAspect = [self storedTransformValue:@"ink" key:@"nibAspect"
		defaultValue:settings.nibAspect];
	settings.nibAngle = [self storedTransformValue:@"ink" key:@"nibAngle"
		defaultValue:settings.nibAngle];
	settings = VSSanitizeInk(settings);
	[self addTransformRow:[self transformFieldRow:@"Smoothing (0–100)" key:@"inkSmoothing"
		value:settings.smoothing * 100 decimals:1] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Larghezza pennino (pt)" key:@"inkWidth"
		value:settings.nibWidth decimals:2] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Aspetto (2–100%)" key:@"inkAspect"
		value:settings.nibAspect * 100 decimals:1] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Angolo pennino (°)" key:@"inkAngle"
		value:settings.nibAngle decimals:1] toStack:stack];
	VSPushButton* activate = [[VSPushButton alloc]
		initWithTitle:@"Attiva Ink Studio" prominent:YES target:self
		action:@selector(applyInk:)];
	[stack addArrangedSubview:activate];
	[activate.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSInkSet(&settings);
	return stack;
}

- (void)applyInk:(id)sender
{
	VSInkSettings settings = VSInkDefaults();
	settings.smoothing = self.transformFields[@"inkSmoothing"].doubleValue / 100.0;
	settings.nibWidth = self.transformFields[@"inkWidth"].doubleValue;
	settings.nibAspect = self.transformFields[@"inkAspect"].doubleValue / 100.0;
	settings.nibAngle = self.transformFields[@"inkAngle"].doubleValue;
	settings = VSSanitizeInk(settings);
	NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
	[defaults setDouble:settings.smoothing forKey:[self transformPreferenceKey:@"ink" key:@"smoothing"]];
	[defaults setDouble:settings.nibWidth forKey:[self transformPreferenceKey:@"ink" key:@"nibWidth"]];
	[defaults setDouble:settings.nibAspect forKey:[self transformPreferenceKey:@"ink" key:@"nibAspect"]];
	[defaults setDouble:settings.nibAngle forKey:[self transformPreferenceKey:@"ink" key:@"nibAngle"]];
	VSInkSet(&settings);
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelInkConfigure);
}

- (NSView*)textureSettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Texture lineare")];
	NSTextField* explanation = VSLabel(
		@"Trascina un rettangolo sulla tavola per generare linee vettoriali "
		 @"ritagliate ai suoi bordi.", VSFontSmall(), VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	explanation.maximumNumberOfLines = 2;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSTextureSettings settings = VSTextureDefaults();
	settings.spacing = [self storedTransformValue:@"texture" key:@"spacing" defaultValue:settings.spacing];
	settings.angle = [self storedTransformValue:@"texture" key:@"angle" defaultValue:settings.angle];
	settings.strokeWidth = [self storedTransformValue:@"texture" key:@"strokeWidth" defaultValue:settings.strokeWidth];
	settings.crosshatch = [self storedTransformFlag:@"texture" key:@"crosshatch" defaultValue:NO] ? 1 : 0;
	settings = VSSanitizeTexture(settings);
	[self addTransformRow:[self transformFieldRow:@"Spaziatura (pt)" key:@"textureSpacing"
		value:settings.spacing decimals:2] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Angolo (°)" key:@"textureAngle"
		value:settings.angle decimals:1] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Spessore (pt)" key:@"textureStroke"
		value:settings.strokeWidth decimals:2] toStack:stack];
	VSCheckbox* cross = [[VSCheckbox alloc] initWithFrame:NSZeroRect];
	[cross setButtonType:NSButtonTypeSwitch];
	cross.bordered = NO;
	cross.title = @"Tratteggio incrociato";
	cross.state = settings.crosshatch ? NSControlStateValueOn : NSControlStateValueOff;
	cross.translatesAutoresizingMaskIntoConstraints = NO;
	self.textureCrosshatch = cross;
	[stack addArrangedSubview:cross];
	VSPushButton* activate = [[VSPushButton alloc] initWithTitle:@"Attiva Texture Lab"
		prominent:YES target:self action:@selector(applyTexture:)];
	[stack addArrangedSubview:activate];
	[activate.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSTextureSet(&settings);
	return stack;
}

- (void)applyTexture:(id)sender
{
	VSTextureSettings settings = VSTextureDefaults();
	settings.spacing = self.transformFields[@"textureSpacing"].doubleValue;
	settings.angle = self.transformFields[@"textureAngle"].doubleValue;
	settings.strokeWidth = self.transformFields[@"textureStroke"].doubleValue;
	settings.crosshatch = self.textureCrosshatch.state == NSControlStateValueOn ? 1 : 0;
	settings = VSSanitizeTexture(settings);
	NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
	[defaults setDouble:settings.spacing forKey:[self transformPreferenceKey:@"texture" key:@"spacing"]];
	[defaults setDouble:settings.angle forKey:[self transformPreferenceKey:@"texture" key:@"angle"]];
	[defaults setDouble:settings.strokeWidth forKey:[self transformPreferenceKey:@"texture" key:@"strokeWidth"]];
	[defaults setBool:settings.crosshatch forKey:[self transformPreferenceKey:@"texture" key:@"crosshatch"]];
	VSTextureSet(&settings);
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelTextureConfigure);
}

- (NSView*)stippleSettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Puntinatura vettoriale")];
	NSTextField* explanation = VSLabel(
		@"Trascina un rettangolo per creare punti pieni sfalsati e deterministici.",
		VSFontSmall(), VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSStippleSettings settings = VSStippleDefaults();
	settings.spacing = [self storedTransformValue:@"stipple" key:@"spacing" defaultValue:settings.spacing];
	settings.radius = [self storedTransformValue:@"stipple" key:@"radius" defaultValue:settings.radius];
	settings.variation = [self storedTransformValue:@"stipple" key:@"variation" defaultValue:settings.variation];
	settings.maximumDots = (int)[self storedTransformValue:@"stipple" key:@"maximumDots" defaultValue:settings.maximumDots];
	settings = VSSanitizeStipple(settings);
	[self addTransformRow:[self transformFieldRow:@"Spaziatura (pt)" key:@"stippleSpacing"
		value:settings.spacing decimals:2] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Raggio punto (pt)" key:@"stippleRadius"
		value:settings.radius decimals:2] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Variazione (0–100)" key:@"stippleVariation"
		value:settings.variation * 100 decimals:1] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Numero massimo" key:@"stippleMaximum"
		value:settings.maximumDots decimals:0] toStack:stack];
	VSPushButton* activate = [[VSPushButton alloc] initWithTitle:@"Attiva Stipple Lab"
		prominent:YES target:self action:@selector(applyStipple:)];
	[stack addArrangedSubview:activate];
	[activate.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSStippleSet(&settings);
	return stack;
}

- (void)applyStipple:(id)sender
{
	VSStippleSettings settings = VSStippleDefaults();
	settings.spacing = self.transformFields[@"stippleSpacing"].doubleValue;
	settings.radius = self.transformFields[@"stippleRadius"].doubleValue;
	settings.variation = self.transformFields[@"stippleVariation"].doubleValue / 100.0;
	settings.maximumDots = self.transformFields[@"stippleMaximum"].intValue;
	settings = VSSanitizeStipple(settings);
	NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
	[defaults setDouble:settings.spacing forKey:[self transformPreferenceKey:@"stipple" key:@"spacing"]];
	[defaults setDouble:settings.radius forKey:[self transformPreferenceKey:@"stipple" key:@"radius"]];
	[defaults setDouble:settings.variation forKey:[self transformPreferenceKey:@"stipple" key:@"variation"]];
	[defaults setInteger:settings.maximumDots forKey:[self transformPreferenceKey:@"stipple" key:@"maximumDots"]];
	VSStippleSet(&settings);
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelStippleConfigure);
}

- (NSView*)geometrySettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Costruzione geometrica")];
	NSTextField* explanation = VSLabel(
		@"Trascina dal centro al punto di costruzione. Scegli cerchio, "
		 @"tangente, costruzione combinata o arco semicircolare.",
		VSFontSmall(), VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	explanation.maximumNumberOfLines = 3;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSGeometrySettings settings = VSGeometryDefaults();
	settings.mode = (int)[self storedTransformValue:@"geometry" key:@"mode" defaultValue:settings.mode];
	settings.strokeWidth = [self storedTransformValue:@"geometry" key:@"strokeWidth" defaultValue:settings.strokeWidth];
	settings = VSSanitizeGeometry(settings);
	self.geometryMode = [[VSSegmentedControl alloc]
		initWithLabels:@[@"Cerchio + tangente", @"Cerchio", @"Tangente", @"Arco"]
			target:nil action:nil];
	self.geometryMode.selectedSegment = settings.mode;
	[stack addArrangedSubview:self.geometryMode];
	[self.geometryMode.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	[self addTransformRow:[self transformFieldRow:@"Spessore (pt)" key:@"geometryStroke"
		value:settings.strokeWidth decimals:2] toStack:stack];
	VSPushButton* activate = [[VSPushButton alloc] initWithTitle:@"Attiva Geometry Lab"
		prominent:YES target:self action:@selector(applyGeometry:)];
	[stack addArrangedSubview:activate];
	[activate.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSGeometrySet(&settings);
	return stack;
}

- (void)applyGeometry:(id)sender
{
	VSGeometrySettings settings = VSGeometryDefaults();
	settings.mode = (int)self.geometryMode.selectedSegment;
	settings.strokeWidth = self.transformFields[@"geometryStroke"].doubleValue;
	settings = VSSanitizeGeometry(settings);
	NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
	[defaults setInteger:settings.mode forKey:[self transformPreferenceKey:@"geometry" key:@"mode"]];
	[defaults setDouble:settings.strokeWidth forKey:[self transformPreferenceKey:@"geometry" key:@"strokeWidth"]];
	VSGeometrySet(&settings);
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelGeometryConfigure);
}

- (NSView*)shapeSettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Riforma locale")];
	NSTextField* explanation = VSLabel(
		@"Trascina vicino agli ancoraggi selezionati. La deformazione decresce "
		 @"in modo morbido entro il raggio indicato.", VSFontSmall(), VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	explanation.maximumNumberOfLines = 3;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSShapeSettings settings = VSShapeDefaults();
	settings.radius = [self storedTransformValue:@"shape" key:@"radius" defaultValue:settings.radius];
	settings.strength = [self storedTransformValue:@"shape" key:@"strength" defaultValue:settings.strength];
	settings.preserveHandles = [self storedTransformFlag:@"shape" key:@"preserveHandles" defaultValue:YES] ? 1 : 0;
	settings = VSSanitizeShape(settings);
	[self addTransformRow:[self transformFieldRow:@"Raggio (pt)" key:@"shapeRadius"
		value:settings.radius decimals:1] toStack:stack];
	[self addTransformRow:[self transformFieldRow:@"Forza (0–200%)" key:@"shapeStrength"
		value:settings.strength * 100 decimals:1] toStack:stack];
	VSCheckbox* preserve = [[VSCheckbox alloc] initWithFrame:NSZeroRect];
	[preserve setButtonType:NSButtonTypeSwitch];
	preserve.bordered = NO;
	preserve.title = @"Conserva le maniglie";
	preserve.state = settings.preserveHandles ? NSControlStateValueOn : NSControlStateValueOff;
	preserve.translatesAutoresizingMaskIntoConstraints = NO;
	self.shapePreserveHandles = preserve;
	[stack addArrangedSubview:preserve];
	VSPushButton* activate = [[VSPushButton alloc] initWithTitle:@"Attiva Shape Reform"
		prominent:YES target:self action:@selector(applyShape:)];
	[stack addArrangedSubview:activate];
	[activate.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSShapeSet(&settings);
	return stack;
}

- (void)applyShape:(id)sender
{
	VSShapeSettings settings = VSShapeDefaults();
	settings.radius = self.transformFields[@"shapeRadius"].doubleValue;
	settings.strength = self.transformFields[@"shapeStrength"].doubleValue / 100.0;
	settings.preserveHandles = self.shapePreserveHandles.state == NSControlStateValueOn ? 1 : 0;
	settings = VSSanitizeShape(settings);
	NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
	[defaults setDouble:settings.radius forKey:[self transformPreferenceKey:@"shape" key:@"radius"]];
	[defaults setDouble:settings.strength forKey:[self transformPreferenceKey:@"shape" key:@"strength"]];
	[defaults setBool:settings.preserveHandles forKey:[self transformPreferenceKey:@"shape" key:@"preserveHandles"]];
	VSShapeSet(&settings);
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelShapeConfigure);
}

#pragma mark Path Studio

- (NSView*)pathSettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Tracciati")];
	NSTextField* explanation = VSLabel(
		@"Semplifica, converte gli ancoraggi o inverte la direzione. "
		 @"Le operazioni attraversano gruppi e tracciati composti.",
		VSFontSmall(),
		VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	explanation.maximumNumberOfLines = 3;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	VSPathSettings settings = VSPathDefaults();
	settings.tolerance = [self storedTransformValue:@"path" key:@"tolerance"
		defaultValue:settings.tolerance];
	settings.preserveCurves = [self storedTransformFlag:@"path" key:@"preserveCurves"
		defaultValue:YES] ? 1 : 0;
	settings = VSSanitizePath(settings);
	[self addTransformRow:[self transformFieldRow:@"Tolleranza (pt)"
		key:@"pathTolerance" value:settings.tolerance decimals:2] toStack:stack];

	VSCheckbox* preserve = [[VSCheckbox alloc] initWithFrame:NSZeroRect];
	[preserve setButtonType:NSButtonTypeSwitch];
	preserve.bordered = NO;
	preserve.title = @"Proteggi ancoraggi curvi";
	preserve.state = settings.preserveCurves ? NSControlStateValueOn : NSControlStateValueOff;
	preserve.translatesAutoresizingMaskIntoConstraints = NO;
	self.pathPreserveCurves = preserve;
	[stack addArrangedSubview:preserve];

	NSStackView* firstActions = [[NSStackView alloc] initWithFrame:NSZeroRect];
	firstActions.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	firstActions.distribution = NSStackViewDistributionFillEqually;
	firstActions.spacing = 7;
	firstActions.translatesAutoresizingMaskIntoConstraints = NO;
	[firstActions addArrangedSubview:[[VSPushButton alloc] initWithTitle:@"Semplifica"
		prominent:YES target:self action:@selector(pathSimplify:)]];
	[firstActions addArrangedSubview:[[VSPushButton alloc] initWithTitle:@"Inverti direzione"
		prominent:NO target:self action:@selector(pathReverse:)]];
	[stack addArrangedSubview:firstActions];
	[firstActions.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	NSStackView* secondActions = [[NSStackView alloc] initWithFrame:NSZeroRect];
	secondActions.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	secondActions.distribution = NSStackViewDistributionFillEqually;
	secondActions.spacing = 7;
	secondActions.translatesAutoresizingMaskIntoConstraints = NO;
	[secondActions addArrangedSubview:[[VSPushButton alloc] initWithTitle:@"Ancoraggi netti"
		prominent:NO target:self action:@selector(pathCorner:)]];
	[secondActions addArrangedSubview:[[VSPushButton alloc] initWithTitle:@"Ancoraggi morbidi"
		prominent:NO target:self action:@selector(pathSmooth:)]];
	[stack addArrangedSubview:secondActions];
	[secondActions.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSPathSet(&settings);
	return stack;
}

- (void)runPathCommand:(int)command
{
	VSPathSettings settings = VSPathDefaults();
	settings.tolerance = self.transformFields[@"pathTolerance"].doubleValue;
	settings.preserveCurves = self.pathPreserveCurves.state == NSControlStateValueOn ? 1 : 0;
	settings = VSSanitizePath(settings);
	NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
	[defaults setDouble:settings.tolerance forKey:[self transformPreferenceKey:@"path" key:@"tolerance"]];
	[defaults setBool:settings.preserveCurves forKey:[self transformPreferenceKey:@"path" key:@"preserveCurves"]];
	VSPathSet(&settings);
	if (self.activateTool) self.activateTool(self.callbackContext, command);
}

- (void)pathSimplify:(id)sender { [self runPathCommand:kVSPanelPathSimplify]; }
- (void)pathCorner:(id)sender { [self runPathCommand:kVSPanelPathCorner]; }
- (void)pathSmooth:(id)sender { [self runPathCommand:kVSPanelPathSmooth]; }
- (void)pathReverse:(id)sender { [self runPathCommand:kVSPanelPathReverse]; }

#pragma mark Raster Lab

- (NSView*)rasterSettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Immagini")];
	NSTextField* explanation = VSLabel(
		@"Lavora sulle immagini selezionate. Il ricampionamento crea una copia "
		 @"e conserva l’originale; Incorpora converte i collegamenti in contenuto nativo.",
		VSFontSmall(),
		VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	explanation.maximumNumberOfLines = 4;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	VSRasterSettings settings = VSRasterDefaults();
	settings.resolution = [self storedTransformValue:@"raster" key:@"resolution"
		defaultValue:settings.resolution];
	settings.resampling = (int)[self storedTransformValue:@"raster" key:@"resampling"
		defaultValue:settings.resampling];
	settings = VSSanitizeRaster(settings);
	[self addTransformRow:[self transformFieldRow:@"Risoluzione (ppi)"
		key:@"rasterResolution" value:settings.resolution decimals:0] toStack:stack];
	self.rasterResampling = [[VSSegmentedControl alloc]
		initWithLabels:@[@"Rapido", @"Media", @"Bicubica"] target:nil action:nil];
	self.rasterResampling.selectedSegment = settings.resampling;
	[stack addArrangedSubview:self.rasterResampling];
	[self.rasterResampling.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	NSStackView* firstActions = [[NSStackView alloc] initWithFrame:NSZeroRect];
	firstActions.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	firstActions.distribution = NSStackViewDistributionFillEqually;
	firstActions.spacing = 7;
	firstActions.translatesAutoresizingMaskIntoConstraints = NO;
	[firstActions addArrangedSubview:[[VSPushButton alloc] initWithTitle:@"Seleziona immagini"
		prominent:NO target:self action:@selector(rasterSelect:)]];
	[firstActions addArrangedSubview:[[VSPushButton alloc] initWithTitle:@"Incorpora"
		prominent:NO target:self action:@selector(rasterEmbed:)]];
	[stack addArrangedSubview:firstActions];
	[firstActions.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	NSStackView* secondActions = [[NSStackView alloc] initWithFrame:NSZeroRect];
	secondActions.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	secondActions.distribution = NSStackViewDistributionFillEqually;
	secondActions.spacing = 7;
	secondActions.translatesAutoresizingMaskIntoConstraints = NO;
	[secondActions addArrangedSubview:[[VSPushButton alloc] initWithTitle:@"Imposta metadato"
		prominent:NO target:self action:@selector(rasterSetResolution:)]];
	[secondActions addArrangedSubview:[[VSPushButton alloc] initWithTitle:@"Crea copia"
		prominent:YES target:self action:@selector(rasterResample:)]];
	[stack addArrangedSubview:secondActions];
	[secondActions.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSRasterSet(&settings);
	return stack;
}

- (VSRasterSettings)currentRasterSettings
{
	VSRasterSettings settings = VSRasterDefaults();
	settings.resolution = self.transformFields[@"rasterResolution"].doubleValue;
	settings.resampling = (int)self.rasterResampling.selectedSegment;
	settings = VSSanitizeRaster(settings);
	NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
	[defaults setDouble:settings.resolution forKey:[self transformPreferenceKey:@"raster" key:@"resolution"]];
	[defaults setInteger:settings.resampling forKey:[self transformPreferenceKey:@"raster" key:@"resampling"]];
	VSRasterSet(&settings);
	return settings;
}

- (void)runRasterCommand:(int)command
{
	(void)[self currentRasterSettings];
	if (self.activateTool) self.activateTool(self.callbackContext, command);
}

- (void)rasterSelect:(id)sender { [self runRasterCommand:kVSPanelRasterSelect]; }
- (void)rasterEmbed:(id)sender { [self runRasterCommand:kVSPanelRasterEmbed]; }
- (void)rasterResample:(id)sender { [self runRasterCommand:kVSPanelRasterResample]; }
- (void)rasterSetResolution:(id)sender { [self runRasterCommand:kVSPanelRasterSetResolution]; }

#pragma mark Auto Save

- (NSView*)autoSaveSettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Salvataggio automatico")];
	NSTextField* explanation = VSLabel(
		@"Salva il documento aperto all’intervallo scelto. Per i documenti "
		 @"senza nome il salvataggio automatico attende il primo Salva ora.",
		VSFontSmall(),
		VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	explanation.maximumNumberOfLines = 3;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	VSAutoSaveSettings settings = VSAutoSaveGet();
	settings = VSSanitizeAutoSave(settings);
	VSCheckbox* enabled = [[VSCheckbox alloc] initWithFrame:NSZeroRect];
	[enabled setButtonType:NSButtonTypeSwitch];
	enabled.bordered = NO;
	enabled.title = @"Attiva Auto Save";
	enabled.state = settings.enabled ? NSControlStateValueOn : NSControlStateValueOff;
	enabled.translatesAutoresizingMaskIntoConstraints = NO;
	self.autoSaveEnabled = enabled;
	[stack addArrangedSubview:enabled];

	[self addTransformRow:[self transformFieldRow:@"Intervallo (1–120 min)"
		key:@"intervalMinutes" value:settings.intervalMinutes decimals:0] toStack:stack];

	VSCheckbox* modified = [[VSCheckbox alloc] initWithFrame:NSZeroRect];
	[modified setButtonType:NSButtonTypeSwitch];
	modified.bordered = NO;
	modified.title = @"Solo se il documento è modificato";
	modified.state = settings.modifiedOnly ? NSControlStateValueOn : NSControlStateValueOff;
	modified.translatesAutoresizingMaskIntoConstraints = NO;
	self.autoSaveModifiedOnly = modified;
	[stack addArrangedSubview:modified];

	VSCheckbox* versions = [[VSCheckbox alloc] initWithFrame:NSZeroRect];
	[versions setButtonType:NSButtonTypeSwitch];
	versions.bordered = NO;
	versions.title = @"Crea una copia in “Vector Suite Backups”";
	versions.state = settings.createVersionCopy ? NSControlStateValueOn : NSControlStateValueOff;
	versions.translatesAutoresizingMaskIntoConstraints = NO;
	self.autoSaveVersions = versions;
	[stack addArrangedSubview:versions];

	NSStackView* actions = [[NSStackView alloc] initWithFrame:NSZeroRect];
	actions.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	actions.distribution = NSStackViewDistributionFillEqually;
	actions.spacing = 7;
	actions.translatesAutoresizingMaskIntoConstraints = NO;
	[actions addArrangedSubview:[[VSPushButton alloc] initWithTitle:@"Salva ora"
		prominent:NO target:self action:@selector(autoSaveNow:)]];
	[actions addArrangedSubview:[[VSPushButton alloc] initWithTitle:@"Applica"
		prominent:YES target:self action:@selector(applyAutoSave:)]];
	[stack addArrangedSubview:actions];
	[actions.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	return stack;
}

- (VSAutoSaveSettings)currentAutoSaveSettings
{
	VSAutoSaveSettings settings = VSAutoSaveDefaults();
	settings.enabled = self.autoSaveEnabled.state == NSControlStateValueOn ? 1 : 0;
	settings.intervalMinutes = self.transformFields[@"intervalMinutes"].intValue;
	settings.modifiedOnly = self.autoSaveModifiedOnly.state == NSControlStateValueOn ? 1 : 0;
	settings.createVersionCopy = self.autoSaveVersions.state == NSControlStateValueOn ? 1 : 0;
	return VSSanitizeAutoSave(settings);
}

- (void)applyAutoSave:(id)sender
{
	VSAutoSaveSettings settings = [self currentAutoSaveSettings];
	NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
	[defaults setBool:settings.enabled forKey:@"studio.vectorsuite.autoSave.enabled"];
	[defaults setInteger:settings.intervalMinutes forKey:@"studio.vectorsuite.autoSave.intervalMinutes"];
	[defaults setBool:settings.modifiedOnly forKey:@"studio.vectorsuite.autoSave.modifiedOnly"];
	[defaults setBool:settings.createVersionCopy forKey:@"studio.vectorsuite.autoSave.createVersionCopy"];
	VSAutoSaveSet(&settings);
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelAutoSaveConfigure);
}

- (void)autoSaveNow:(id)sender
{
	VSAutoSaveSettings settings = [self currentAutoSaveSettings];
	VSAutoSaveSet(&settings);
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelAutoSaveNow);
}

#pragma mark Smart Find

- (NSView*)smartFindSettingsView
{
	NSStackView* stack = [[NSStackView alloc] initWithFrame:NSZeroRect];
	stack.orientation = NSUserInterfaceLayoutOrientationVertical;
	stack.alignment = NSLayoutAttributeLeading;
	stack.spacing = 7;
	stack.edgeInsets = NSEdgeInsetsMake(12, 12, 14, 12);
	stack.translatesAutoresizingMaskIntoConstraints = NO;

	[stack addArrangedSubview:VSEyebrow(@"Trova nel documento")];
	NSTextField* explanation = VSLabel(
		@"Usa il primo tracciato selezionato come riferimento. Gli oggetti "
		@"bloccati o nascosti non vengono modificati.",
		VSFontSmall(),
		VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	explanation.maximumNumberOfLines = 3;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	NSStackView* primary = [[NSStackView alloc] initWithFrame:NSZeroRect];
	primary.orientation = NSUserInterfaceLayoutOrientationHorizontal;
	primary.distribution = NSStackViewDistributionFillEqually;
	primary.spacing = 7;
	primary.translatesAutoresizingMaskIntoConstraints = NO;
	[primary addArrangedSubview:
		[[VSPushButton alloc] initWithTitle:@"Aspetto"
							 prominent:YES
								target:self
								action:@selector(smartFindAppearance:)]];
	[primary addArrangedSubview:
		[[VSPushButton alloc] initWithTitle:@"Geometria"
							 prominent:NO
								target:self
								action:@selector(smartFindGeometry:)]];
	[stack addArrangedSubview:primary];
	[primary.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	VSPushButton* exact = [[VSPushButton alloc] initWithTitle:@"Aspetto + geometria"
										prominent:NO
									   target:self
									   action:@selector(smartFindExact:)];
	[stack addArrangedSubview:exact];
	[exact.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	[stack addArrangedSubview:VSEyebrow(@"Sostituisci")];
	NSTextField* replaceHint = VSLabel(
		@"Applica lo stile corrente di Illustrator ai tracciati selezionati.",
		VSFontSmall(),
		VSQuiet());
	replaceHint.lineBreakMode = NSLineBreakByWordWrapping;
	[stack addArrangedSubview:replaceHint];
	[replaceHint.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;

	VSPushButton* applyStyle = [[VSPushButton alloc]
		initWithTitle:@"Applica stile corrente"
			 prominent:NO
				target:self
				action:@selector(smartFindApplyStyle:)];
	[stack addArrangedSubview:applyStyle];
	[applyStyle.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	return stack;
}

- (void)runSmartFindCommand:(int)command
{
	if (self.activateTool) self.activateTool(self.callbackContext, command);
}

- (void)smartFindAppearance:(id)sender
{
	[self runSmartFindCommand:kVSPanelSmartFindAppearance];
}

- (void)smartFindGeometry:(id)sender
{
	[self runSmartFindCommand:kVSPanelSmartFindGeometry];
}

- (void)smartFindExact:(id)sender
{
	[self runSmartFindCommand:kVSPanelSmartFindExact];
}

- (void)smartFindApplyStyle:(id)sender
{
	[self runSmartFindCommand:kVSPanelSmartFindApplyStyle];
}

#pragma mark Griglia dei moduli

- (NSView*)suiteCoreSettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Stato della suite")];
	for (NSString* line in @[
		@"22 moduli registrati nel pannello nativo",
		@"Interfaccia monocromatica adattiva chiaro/scuro",
		@"Comandi con undo e preferenze locali senza dati personali",
		@"Bundle compatibile Apple Silicon e Intel"
	]) {
		NSTextField* label = VSLabel(line, VSFontBody(), VSInk());
		label.lineBreakMode = NSLineBreakByWordWrapping;
		[stack addArrangedSubview:label];
		[label.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	}
	return stack;
}

- (NSView*)directSettingsView
{
	NSStackView* stack = [self transformSettingsStack];
	[stack addArrangedSubview:VSEyebrow(@"Snap Vector Suite")];
	const VSSnap::Settings settings=VSSnapGet();
	NSArray<NSString*>* names=@[@"Attiva snap aggiuntivi", @"Estremi e ancoraggi", @"Punti medi", @"Intersezioni (segmenti retti)", @"Punto più vicino", @"Perpendicolare", @"Centro geometrico", @"Estremi X/Y delle curve", @"Tangente"];
	const unsigned modes[]={0,VSSnap::Endpoint,VSSnap::Midpoint,VSSnap::Intersection,VSSnap::Nearest,VSSnap::Perpendicular,VSSnap::Center,VSSnap::Quadrant,VSSnap::Tangent};
	for (NSUInteger i=0;i<names.count;++i) {
		VSCheckbox* check=[[VSCheckbox alloc] initWithFrame:NSZeroRect];
		[check setButtonType:NSButtonTypeSwitch];
		check.bordered=NO;check.title=names[i];check.tag=modes[i];
		check.state=(i==0?settings.enabled:(settings.modes&modes[i])!=0)?NSControlStateValueOn:NSControlStateValueOff;
		check.target=self;check.action=@selector(snapSettingChanged:);
		check.translatesAutoresizingMaskIntoConstraints=NO;
		[stack addArrangedSubview:check];
	}
	NSTextField* snapHint=VSLabel(@"Negli strumenti Vector Suite. Tangente e perpendicolare usano il punto iniziale. Shift mantiene le direzioni a 45°.",VSFontSmall(),VSQuiet());
	snapHint.lineBreakMode=NSLineBreakByWordWrapping;
	[stack addArrangedSubview:snapHint];
	[snapHint.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active=YES;
	[stack addArrangedSubview:VSLabel(@"Tolleranza (1–32 px)",VSFontSmall(),VSQuiet())];
	VSNumericField* tolerance=[[VSNumericField alloc] initWithFrame:NSZeroRect];
	tolerance.doubleValue=settings.pixels;tolerance.tag=100;
	tolerance.target=self;tolerance.action=@selector(snapSettingChanged:);
	tolerance.translatesAutoresizingMaskIntoConstraints=NO;
	[stack addArrangedSubview:tolerance];
	[tolerance.widthAnchor constraintEqualToConstant:80].active=YES;
	[stack addArrangedSubview:VSEyebrow(@"Snap di Illustrator")];
	NSTextField* snapping = VSLabel(
		@"Configura le Guide sensibili nelle preferenze globali di Illustrator. "
		 @"Per la tangente usa Penna o Linea e abilita le Guide geometriche.",
		VSFontSmall(), VSQuiet());
	snapping.lineBreakMode = NSLineBreakByWordWrapping;
	[stack addArrangedSubview:snapping];
	[snapping.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSPushButton* snapPreferences = [[VSPushButton alloc]
		initWithTitle:@"Configura snap di Illustrator…" prominent:NO target:self
		action:@selector(openNativeSnapPreferences:)];
	[stack addArrangedSubview:snapPreferences];
	[snapPreferences.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	[stack addArrangedSubview:VSEyebrow(@"Pannello")];
	NSTextField* explanation = VSLabel(
		@"Il tema segue automaticamente Illustrator e macOS. Control + trascina "
		 @"una scheda per modificare l’ordine dei moduli.", VSFontSmall(), VSQuiet());
	explanation.lineBreakMode = NSLineBreakByWordWrapping;
	explanation.maximumNumberOfLines = 3;
	[stack addArrangedSubview:explanation];
	[explanation.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	VSPushButton* reset = [[VSPushButton alloc]
		initWithTitle:@"Ripristina ordine moduli" prominent:NO target:self
		action:@selector(resetModuleOrder:)];
	[stack addArrangedSubview:reset];
	[reset.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24].active = YES;
	return stack;
}

- (void)resetModuleOrder:(id)sender
{
	[self.moduleOrder removeAllObjects];
	for (NSInteger moduleID = 0; moduleID < kVSModuleCount; ++moduleID) {
		[self.moduleOrder addObject:@(moduleID)];
	}
	[NSUserDefaults.standardUserDefaults setObject:self.moduleOrder
		forKey:@"studio.vectorsuite.panel.moduleOrder"];
	[self rebuildModules];
}

- (void)openNativeSnapPreferences:(id)sender
{
	if (self.activateTool) self.activateTool(self.callbackContext, kVSPanelNativeSnapPreferences);
}

- (void)snapSettingChanged:(NSControl*)sender
{
	VSSnap::Settings settings=VSSnapGet();
	if (sender.tag==100) settings.pixels=sender.doubleValue;
	else if(sender.tag==0) settings.enabled=[(NSButton*)sender state]==NSControlStateValueOn;
	else if ([(NSButton*)sender state]==NSControlStateValueOn) settings.modes|=(unsigned)sender.tag;
	else settings.modes&=~(unsigned)sender.tag;
	VSSnapSet(&settings);settings=VSSnapGet();
	if(sender.tag==100)sender.doubleValue=settings.pixels;
	NSUserDefaults* defaults=NSUserDefaults.standardUserDefaults;
	[defaults setBool:settings.enabled forKey:@"studio.vectorsuite.snap.enabled"];
	[defaults setInteger:settings.modes forKey:@"studio.vectorsuite.snap.modes"];
	[defaults setDouble:settings.pixels forKey:@"studio.vectorsuite.snap.pixels"];
}

- (VSModuleCardButton*)moduleCardForModule:(const VSModuleDefinition&)module
{
	VSModuleCardButton* button = [[VSModuleCardButton alloc] initWithFrame:NSZeroRect];
	button.moduleID = module.id;
	button.moduleName = [NSString stringWithUTF8String:module.name];
	button.moduleSummary = [NSString stringWithUTF8String:module.summary];
	button.accessibilityLabel = button.moduleName;
	button.accessibilityHelp = button.moduleSummary;
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

	if (self.selectedModule == kVSProjectionStudio &&
		[visibleModules containsObject:@(kVSProjectionStudio)]) {
		NSView* settings = [self projectionSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
											 constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSSuiteCore &&
		[visibleModules containsObject:@(kVSSuiteCore)]) {
		NSView* settings = [self suiteCoreSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSDirectSettings &&
		[visibleModules containsObject:@(kVSDirectSettings)]) {
		NSView* settings = [self directSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSSmartFind &&
		[visibleModules containsObject:@(kVSSmartFind)]) {
		NSView* settings = [self smartFindSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
											 constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSCollisionAlign &&
		[visibleModules containsObject:@(kVSCollisionAlign)]) {
		NSView* settings = [self collisionSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSMirrorStudio &&
		[visibleModules containsObject:@(kVSMirrorStudio)]) {
		NSView* settings = [self mirrorSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSRandomize &&
		[visibleModules containsObject:@(kVSRandomize)]) {
		NSView* settings = [self randomizeSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSWidthStudio &&
		[visibleModules containsObject:@(kVSWidthStudio)]) {
		NSView* settings = [self widthSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSLiveStyle &&
		[visibleModules containsObject:@(kVSLiveStyle)]) {
		NSView* settings = [self liveStyleSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSColorLab &&
		[visibleModules containsObject:@(kVSColorLab)]) {
		NSView* settings = [self colorSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSAutoSave &&
		[visibleModules containsObject:@(kVSAutoSave)]) {
		NSView* settings = [self autoSaveSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSRasterLab &&
		[visibleModules containsObject:@(kVSRasterLab)]) {
		NSView* settings = [self rasterSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSPathStudio &&
		[visibleModules containsObject:@(kVSPathStudio)]) {
		NSView* settings = [self pathSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSPrecisionPen &&
		[visibleModules containsObject:@(kVSPrecisionPen)]) {
		NSView* settings = [self precisionSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSFluidSketch &&
		[visibleModules containsObject:@(kVSFluidSketch)]) {
		NSView* settings = [self fluidSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSInkStudio &&
		[visibleModules containsObject:@(kVSInkStudio)]) {
		NSView* settings = [self inkSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSTextureLab &&
		[visibleModules containsObject:@(kVSTextureLab)]) {
		NSView* settings = [self textureSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSStippleLab &&
		[visibleModules containsObject:@(kVSStippleLab)]) {
		NSView* settings = [self stippleSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSGeometryLab &&
		[visibleModules containsObject:@(kVSGeometryLab)]) {
		NSView* settings = [self geometrySettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
	}

	if (self.selectedModule == kVSShapeReform &&
		[visibleModules containsObject:@(kVSShapeReform)]) {
		NSView* settings = [self shapeSettingsView];
		[self.moduleStack addArrangedSubview:settings];
		[settings.widthAnchor constraintEqualToAnchor:self.moduleStack.widthAnchor
			constant:-2 * kVSGutter].active = YES;
		[settings layoutSubtreeIfNeeded];
		[settings.heightAnchor constraintEqualToConstant:
			MAX(1.0, settings.fittingSize.height)].active = YES;
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

	// Le impostazioni venivano aggiunte dopo tutte le righe della libreria:
	// per questo, premendo una scheda in alto, sembravano aprirsi in fondo alla
	// pagina. C'è una sola vista impostazioni attiva; la spostiamo subito dopo
	// la riga che contiene la scheda selezionata.
	NSUInteger selectedVisibleIndex = [visibleModules indexOfObject:@(self.selectedModule)];
	const NSUInteger moduleRowCount = (visibleModules.count + 1) / 2;
	if (selectedVisibleIndex != NSNotFound &&
		self.moduleStack.arrangedSubviews.count > moduleRowCount) {
		NSView* settings = self.moduleStack.arrangedSubviews.lastObject;
		const NSUInteger insertionIndex = MIN(
			selectedVisibleIndex / 2 + 1,
			moduleRowCount);
		[self.moduleStack removeArrangedSubview:settings];
		[self.moduleStack insertArrangedSubview:settings atIndex:insertionIndex];
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
	NSButton* selectedButton = self.moduleButtons[@(moduleID)];
	if (selectedButton) {
		[self.moduleStack layoutSubtreeIfNeeded];
		[selectedButton scrollRectToVisible:selectedButton.bounds];
	}
	if (self.activateTool) {
		int callbackID = (int)moduleID;
		if (moduleID == kVSProjectionStudio) {
			callbackID = kVSPanelProjectionToolBase +
				(int)[self projectionToolSelection];
		}
		self.activateTool(self.callbackContext, callbackID);
	}
}

- (void)categoryChanged:(id)sender
{
	[self rebuildModules];
}

- (void)controlTextDidChange:(NSNotification*)notification
{
	if (notification.object == self.searchField) [self rebuildModules];
}

- (void)controlTextDidEndEditing:(NSNotification*)notification
{
	if (notification.object == self.transformFields[@"copies"] ||
		notification.object == self.transformFields[@"axisOffset"]) {
		[self mirrorSettingsChanged:notification.object];
	}
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
	NSUserDefaults* snapDefaults=NSUserDefaults.standardUserDefaults;
	VSSnap::Settings snapSettings;
	snapSettings.enabled=[snapDefaults boolForKey:@"studio.vectorsuite.snap.enabled"];
	if([snapDefaults objectForKey:@"studio.vectorsuite.snap.modes"])
		snapSettings.modes=(unsigned)[snapDefaults integerForKey:@"studio.vectorsuite.snap.modes"];
	if([snapDefaults objectForKey:@"studio.vectorsuite.snap.pixels"])
		snapSettings.pixels=[snapDefaults doubleForKey:@"studio.vectorsuite.snap.pixels"];
	VSSnapSet(&snapSettings);

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
