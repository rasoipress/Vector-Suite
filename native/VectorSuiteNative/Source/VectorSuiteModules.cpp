#include "IllustratorSDK.h"
#include "SDKErrors.h"
#include "actions/AIDocumentAction.h"

#include "VectorSuitePathRepair.h"
#include "VectorSuitePlugin.h"
#include "VectorSuiteSmartFind.h"
#include "VectorSuiteSelection.h"
#include "VectorSuiteDrawingMath.h"
#include "VectorSuiteStyleMath.h"
#include "VectorSuiteTransformMath.h"
#include "VectorSuiteWorkflow.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>

namespace {

constexpr AIReal kMinimumDrag = 0.01;
constexpr AIReal kCircleBezier = 0.5522847498307936;
constexpr AIReal kDegreesToRadians = 3.14159265358979323846 / 180.0;

VSRandomizeSettings gRandomizeSettings = VSRandomizeDefaults();
VSMirrorSettings gMirrorSettings = VSMirrorDefaults();
VSCollisionSettings gCollisionSettings = VSCollisionDefaults();
VSWidthSettings gWidthSettings = VSWidthDefaults();
VSLiveStyleSettings gLiveStyleSettings = VSLiveStyleDefaults();
VSColorSettings gColorSettings = VSColorDefaults();
VSAutoSaveSettings gAutoSaveSettings = VSAutoSaveDefaults();
VSRasterSettings gRasterSettings = VSRasterDefaults();
VSPathSettings gPathSettings = VSPathDefaults();
VSPrecisionSettings gPrecisionSettings = VSPrecisionDefaults();
VSFluidSettings gFluidSettings = VSFluidDefaults();
VSInkSettings gInkSettings = VSInkDefaults();
VSTextureSettings gTextureSettings = VSTextureDefaults();
VSStippleSettings gStippleSettings = VSStippleDefaults();
VSGeometrySettings gGeometrySettings = VSGeometryDefaults();
VSShapeSettings gShapeSettings = VSShapeDefaults();
VSSnap::Settings gSnapSettings;

AIReal Clamp(AIReal value, AIReal minimum, AIReal maximum)
{
	return std::max(minimum, std::min(maximum, value));
}

AIReal Distance(const AIRealPoint& a, const AIRealPoint& b)
{
	const AIReal dx = b.h - a.h;
	const AIReal dy = b.v - a.v;
	return std::sqrt(dx * dx + dy * dy);
}

AIRealPoint Add(const AIRealPoint& a, const AIRealPoint& b)
{
	return {a.h + b.h, a.v + b.v};
}

AIRealPoint Subtract(const AIRealPoint& a, const AIRealPoint& b)
{
	return {a.h - b.h, a.v - b.v};
}

AIRealPoint Scale(const AIRealPoint& point, AIReal scale)
{
	return {point.h * scale, point.v * scale};
}

class FractalRandom
{
public:
	explicit FractalRandom(double seed)
		: fState(static_cast<std::uint32_t>(std::max(1.0, std::floor(seed))))
	{
		for (int index = 0; index < 8; ++index) Next();
	}

	double Next()
	{
		fState = fState * 1664525u + 1013904223u;
		return static_cast<double>(fState) / 4294967296.0;
	}

	double Bipolar() { return Next() * 2.0 - 1.0; }
	double Jitter(double amount) { return 1.0 + Bipolar() * amount; }

private:
	std::uint32_t fState;
};

void SetCorner(AIPathSegment& segment, const AIRealPoint& point)
{
	segment.p = point;
	segment.in = point;
	segment.out = point;
	segment.corner = true;
}

class SelectedArt
{
public:
	SelectedArt() : fArt(nullptr), fCount(0), fError(kNoErr)
	{
		fError = sAIMatchingArt->GetSelectedArt(&fArt, &fCount);
		if (fError) {
			fArt = nullptr;
			fCount = 0;
		}
	}

	~SelectedArt()
	{
		if (fArt) {
			sAIMdMemory->MdMemoryDisposeHandle(
				reinterpret_cast<AIMdMemoryHandle>(fArt));
		}
	}

	ASErr Error() const { return fError; }
	ai::int32 Count() const { return fCount; }
	AIArtHandle operator[](ai::int32 index) const { return (*fArt)[index]; }

private:
	AIArtHandle** fArt;
	ai::int32 fCount;
	ASErr fError;
};

class MatchingArt
{
public:
	explicit MatchingArt(AIArtType type) : fArt(nullptr), fCount(0), fError(kNoErr)
	{
		AIMatchingArtSpec specification(type, 0, 0);
		fError = sAIMatchingArt->GetMatchingArt(
			&specification,
			1,
			&fArt,
			&fCount);
		if (fError) {
			fArt = nullptr;
			fCount = 0;
		}
	}

	~MatchingArt()
	{
		if (fArt) {
			sAIMdMemory->MdMemoryDisposeHandle(
				reinterpret_cast<AIMdMemoryHandle>(fArt));
		}
	}

	ASErr Error() const { return fError; }
	ai::int32 Count() const { return fCount; }
	AIArtHandle operator[](ai::int32 index) const { return (*fArt)[index]; }

private:
	AIArtHandle** fArt;
	ai::int32 fCount;
	ASErr fError;
};

class RasterizeSuiteLease
{
public:
	RasterizeSuiteLease() : fSuite(nullptr), fError(kNoErr)
	{
		const void* suite = nullptr;
		fError = sSPBasic->AcquireSuite(
			kAIRasterizeSuite,
			kAIRasterizeSuiteVersion,
			&suite);
		fSuite = const_cast<AIRasterizeSuite*>(
			static_cast<const AIRasterizeSuite*>(suite));
	}

	~RasterizeSuiteLease()
	{
		if (fSuite) {
			sSPBasic->ReleaseSuite(kAIRasterizeSuite, kAIRasterizeSuiteVersion);
		}
	}

	ASErr Error() const { return fError; }
	AIRasterizeSuite* operator->() const { return fSuite; }

private:
	AIRasterizeSuite* fSuite;
	ASErr fError;
};

bool HasFullySelectedAncestor(AIArtHandle art)
{
	AIArtHandle parent = nullptr;
	if (sAIArt->GetArtParent(art, &parent) != kNoErr) return false;
	while (parent) {
		ai::int32 attributes = 0;
		if (sAIArt->GetArtUserAttr(parent, kArtFullySelected, &attributes) == kNoErr &&
			(attributes & kArtFullySelected)) {
			return true;
		}
		AIArtHandle next = nullptr;
		if (sAIArt->GetArtParent(parent, &next) != kNoErr) break;
		parent = next;
	}
	return false;
}

bool IsFullySelectedRoot(AIArtHandle art)
{
	if (!art) return false;
	ai::int32 attributes = 0;
	const bool fullySelected = sAIArt->GetArtUserAttr(
		art,
		kArtFullySelected,
		&attributes) == kNoErr &&
		(attributes & kArtFullySelected);
	return VSIsWholeObjectSelectionRoot(
		fullySelected,
		HasFullySelectedAncestor(art));
}

ASErr CollectModulePaths(AIArtHandle art, std::vector<AIArtHandle>& paths)
{
	if (!art) return kNoErr;
	short type = kUnknownArt;
	ASErr error = sAIArt->GetArtType(art, &type);
	if (error) return error;
	if (type == kPathArt) paths.push_back(art);

	AIArtHandle child = nullptr;
	error = sAIArt->GetArtFirstChild(art, &child);
	if (error) return error;
	while (child) {
		error = CollectModulePaths(child, paths);
		if (error) return error;
		AIArtHandle sibling = nullptr;
		error = sAIArt->GetArtSibling(child, &sibling);
		if (error) return error;
		child = sibling;
	}
	return kNoErr;
}

ASErr ApplyCurrentStyle(
	AIArtHandle path,
	bool fill,
	bool stroke,
	AIReal minimumStrokeWidth = 0)
{
	AIPathStyle style;
	AIPathStyleMap styleMap;
	AIDictionaryRef advancedStroke = nullptr;
	ASErr error = sAIPathStyle->GetCurrentPathStyle(
		&style,
		&styleMap,
		&advancedStroke,
		nullptr);
	if (error) return error;

	style.fillPaint = fill;
	style.strokePaint = stroke;
	if (stroke) {
		if (minimumStrokeWidth > 0) {
			style.stroke.width = minimumStrokeWidth;
		}
		if (style.stroke.width <= 0) style.stroke.width = 1;
		style.stroke.cap = kAIRoundCap;
		style.stroke.join = kAIRoundJoin;
	}
	if (fill && style.fill.color.kind == kNoneColor) {
		style.fill.color.kind = kGrayColor;
		style.fill.color.c.g.gray = kAIRealOne;
	}
	return sAIPathStyle->SetPathStyle(path, &style);
}

ASErr CreatePath(
	const std::vector<AIPathSegment>& segments,
	AIBoolean closed,
	bool fill,
	bool stroke,
	AIReal minimumStrokeWidth = 0)
{
	if (segments.size() < 2 || segments.size() > 32767) return kBadParameterErr;
	AIArtHandle path = nullptr;
	ASErr error = sAIArt->NewArt(kPathArt, kPlaceAboveAll, nullptr, &path);
	if (error) return error;

	const ai::int16 count = static_cast<ai::int16>(segments.size());
	error = sAIPath->SetPathSegmentCount(path, count);
	if (!error) error = sAIPath->SetPathSegments(path, 0, count, segments.data());
	if (!error) error = sAIPath->SetPathClosed(path, closed);
	if (!error) error = ApplyCurrentStyle(path, fill, stroke, minimumStrokeWidth);
	return error;
}

ASErr CreateLine(
	const AIRealPoint& start,
	const AIRealPoint& end,
	AIReal minimumStrokeWidth = 0)
{
	std::vector<AIPathSegment> segments(2);
	SetCorner(segments[0], start);
	SetCorner(segments[1], end);
	return CreatePath(segments, false, false, true, minimumStrokeWidth);
}

ASErr CreateCircle(const AIRealPoint& center, AIReal radius, bool fill, AIReal strokeWidth = 1)
{
	if (radius < 0.2) return kNoErr;
	const AIReal tangent = radius * kCircleBezier;
	std::vector<AIPathSegment> segments(4);

	segments[0].p = {center.h + radius, center.v};
	segments[0].in = {center.h + radius, center.v - tangent};
	segments[0].out = {center.h + radius, center.v + tangent};
	segments[0].corner = false;

	segments[1].p = {center.h, center.v + radius};
	segments[1].in = {center.h + tangent, center.v + radius};
	segments[1].out = {center.h - tangent, center.v + radius};
	segments[1].corner = false;

	segments[2].p = {center.h - radius, center.v};
	segments[2].in = {center.h - radius, center.v + tangent};
	segments[2].out = {center.h - radius, center.v - tangent};
	segments[2].corner = false;

	segments[3].p = {center.h, center.v - radius};
	segments[3].in = {center.h - tangent, center.v - radius};
	segments[3].out = {center.h + tangent, center.v - radius};
	segments[3].corner = false;
	return CreatePath(segments, true, fill, !fill, fill ? 0 : strokeWidth);
}

ASErr CreateFractalSegment(
	AIArtHandle container,
	const AIRealPoint& start,
	const AIRealPoint& end,
	AIReal width,
	AIReal angleRadians,
	AIReal length,
	AIReal bow)
{
	AIArtHandle path = nullptr;
	ASErr error = sAIArt->NewArt(
		kPathArt,
		container ? kPlaceInsideOnTop : kPlaceAboveAll,
		container,
		&path);
	if (error) return error;

	std::vector<AIPathSegment> segments(2);
	SetCorner(segments[0], start);
	SetCorner(segments[1], end);
	if (std::abs(bow) > 0.0001) {
		const AIReal dx = std::cos(angleRadians) * length / 3.0;
		const AIReal dy = std::sin(angleRadians) * length / 3.0;
		const AIReal px = -std::sin(angleRadians) * bow;
		const AIReal py = std::cos(angleRadians) * bow;
		segments[0].out = {start.h + dx + px, start.v + dy + py};
		segments[1].in = {end.h - dx + px, end.v - dy + py};
		segments[0].corner = false;
		segments[1].corner = false;
	}

	error = sAIPath->SetPathSegmentCount(path, 2);
	if (!error) error = sAIPath->SetPathSegments(path, 0, 2, segments.data());
	if (!error) error = sAIPath->SetPathClosed(path, false);
	if (error) return error;

	AIPathStyle style;
	AIPathStyleMap styleMap;
	AIDictionaryRef advancedStroke = nullptr;
	error = sAIPathStyle->GetCurrentPathStyle(
		&style,
		&styleMap,
		&advancedStroke,
		nullptr);
	if (error) return error;
	style.fillPaint = false;
	style.strokePaint = true;
	style.stroke.width = std::max<AIReal>(0.05, width);
	style.stroke.cap = kAIRoundCap;
	style.stroke.join = kAIRoundJoin;
	style.stroke.color.kind = kGrayColor;
	style.stroke.color.c.g.gray = kAIRealOne;
	return sAIPathStyle->SetPathStyle(path, &style);
}

struct FractalContext
{
	AIArtHandle container;
	FractalRandom random;
	int count;
	int maximumPaths;
	int maximumIterations;
	AIReal minimumLength;
	AIReal initialAngle;
	AIReal angleFalloff;
	AIReal lengthFalloff;
	AIReal widthFalloff;
	AIReal lengthRandomness;
	AIReal angleRandomness;
	AIReal waveAmount;
	ASErr error;

	explicit FractalContext(const VSFractalSettings& settings)
		: container(nullptr),
		  random(settings.seed),
		  count(0),
		  maximumPaths(settings.maximumPaths),
		  maximumIterations(settings.maximumIterations),
		  minimumLength(settings.minimumLength),
		  initialAngle(settings.initialAngle),
		  angleFalloff(std::max<AIReal>(0.01, settings.angleFalloff)),
		  lengthFalloff(settings.lengthFalloff),
		  widthFalloff(settings.widthFalloff),
		  lengthRandomness(settings.lengthRandomness),
		  angleRandomness(settings.angleRandomness),
		  waveAmount(0),
		  error(kNoErr)
	{
		static const AIReal waves[] = {0, 0.04, 0.09, 0.16};
		waveAmount = waves[std::max(0, std::min(3, settings.wave))];
	}
};

void GrowFractalBranch(
	FractalContext& context,
	const AIRealPoint& start,
	AIReal angleDegrees,
	AIReal length,
	AIReal width,
	int depth)
{
	if (context.error ||
		context.count >= context.maximumPaths ||
		depth >= context.maximumIterations ||
		length < context.minimumLength) {
		return;
	}

	const AIReal radians = angleDegrees * kDegreesToRadians;
	const AIRealPoint end = {
		start.h + std::cos(radians) * length,
		start.v + std::sin(radians) * length
	};
	const AIReal bow = context.waveAmount > 0
		? length * context.waveAmount * context.random.Bipolar()
		: 0;
	context.error = CreateFractalSegment(
		context.container,
		start,
		end,
		width,
		radians,
		length,
		bow);
	if (context.error) return;
	++context.count;

	const AIReal spread = context.initialAngle /
		std::pow(context.angleFalloff, static_cast<AIReal>(depth));
	const AIReal nextWidth = std::max<AIReal>(0.05, width * context.widthFalloff);
	const AIReal baseLength = length * (1.0 - context.lengthFalloff);

	const AIReal rightLength = baseLength *
		context.random.Jitter(context.lengthRandomness);
	const AIReal rightAngle = angleDegrees + spread *
		context.random.Jitter(context.angleRandomness);
	GrowFractalBranch(
		context,
		end,
		rightAngle,
		rightLength,
		nextWidth,
		depth + 1);

	const AIReal leftLength = baseLength *
		context.random.Jitter(context.lengthRandomness);
	const AIReal leftAngle = angleDegrees - spread *
		context.random.Jitter(context.angleRandomness);
	GrowFractalBranch(
		context,
		end,
		leftAngle,
		leftLength,
		nextWidth,
		depth + 1);
}

ASErr TransformAroundCenter(
	AIArtHandle art,
	AIReal translateX,
	AIReal translateY,
	AIReal rotationDegrees,
	AIReal scaleX,
	AIReal scaleY)
{
	AIRealRect bounds;
	ASErr error = sAIArt->GetArtBounds(art, &bounds);
	if (error) return error;
	const AIReal centerX = (bounds.left + bounds.right) * 0.5;
	const AIReal centerY = (bounds.top + bounds.bottom) * 0.5;

	AIRealMatrix matrix;
	sAIRealMath->AIRealMatrixSetTranslate(&matrix, -centerX, -centerY);
	sAIRealMath->AIRealMatrixConcatScale(&matrix, scaleX, scaleY);
	sAIRealMath->AIRealMatrixConcatRotate(
		&matrix,
		sAIRealMath->DegreeToRadian(rotationDegrees));
	sAIRealMath->AIRealMatrixConcatTranslate(
		&matrix,
		centerX + translateX,
		centerY + translateY);

	const AIReal lineScale = std::sqrt(std::abs(scaleX * scaleY));
	return sAITransformArt->TransformArt(
		art,
		&matrix,
		lineScale,
		kTransformObjects | kTransformEntireArtStyle | kTransformChildren |
			kTransformLinkedMasks | kTransformNotifyPluginGroups);
}

ASErr DrawPrecisionLine(
	const AIRealPoint& start,
	const AIRealPoint& cursor,
	const VSPrecisionSettings& rawSettings)
{
	const VSPrecisionSettings settings = VSSanitizePrecision(rawSettings);
	const VSDrawingPoint calculated = VSPrecisionEndPoint(
		{start.h, start.v},
		{cursor.h, cursor.v},
		settings);
	const AIRealPoint end = {calculated.x, calculated.y};
	const AIReal dx = end.h - start.h;
	const AIReal dy = end.v - start.v;
	const AIReal length = std::sqrt(dx * dx + dy * dy);
	if (length < kMinimumDrag) return kNoErr;
	if (std::abs(settings.curveAmount) < 0.001) {
		return CreateLine(start, end, settings.strokeWidth);
	}

	const AIRealPoint normal = {
		-dy / length * length * settings.curveAmount / 3.0,
		dx / length * length * settings.curveAmount / 3.0
	};
	const AIRealPoint third = {dx / 3.0, dy / 3.0};
	std::vector<AIPathSegment> segments(2);
	segments[0].p = start;
	segments[0].in = start;
	segments[0].out = Add(Add(start, third), normal);
	segments[0].corner = false;
	segments[1].p = end;
	segments[1].in = Add(Subtract(end, third), normal);
	segments[1].out = end;
	segments[1].corner = false;
	return CreatePath(segments, false, false, true, settings.strokeWidth);
}

ASErr DrawSmoothGesture(
	const std::vector<AIRealPoint>& points,
	AIReal minimumStrokeWidth,
	AIReal smoothing = 0,
	AIBoolean closed = false)
{
	if (points.size() < 2) return kNoErr;
	std::vector<VSDrawingPoint> source;
	source.reserve(points.size());
	for (const AIRealPoint& point : points) source.push_back({point.h, point.v});
	const std::vector<VSDrawingPoint> smoothed =
		VSSmoothDrawingPoints(source, smoothing, closed != 0);
	std::vector<AIPathSegment> segments(smoothed.size());
	for (size_t index = 0; index < smoothed.size(); ++index) {
		const VSDrawingPoint previous = smoothed[index == 0 ? index : index - 1];
		const VSDrawingPoint next = smoothed[
			index + 1 < smoothed.size() ? index + 1 : index];
		const AIRealPoint point = {smoothed[index].x, smoothed[index].y};
		const AIRealPoint previousPoint = {previous.x, previous.y};
		const AIRealPoint nextPoint = {next.x, next.y};
		const AIRealPoint tangent = Scale(
			Subtract(nextPoint, previousPoint),
			1.0 / 6.0);
		segments[index].p = point;
		segments[index].in = Subtract(point, tangent);
		segments[index].out = Add(point, tangent);
		segments[index].corner = false;
	}
	return CreatePath(segments, closed, false, true, minimumStrokeWidth);
}

ASErr DrawCalligraphicGesture(
	const std::vector<AIRealPoint>& points,
	const VSInkSettings& rawSettings)
{
	if (points.size() < 2) return kNoErr;
	std::vector<VSDrawingPoint> centerline;
	centerline.reserve(points.size());
	for (const AIRealPoint& point : points) centerline.push_back({point.h, point.v});
	const std::vector<VSDrawingPoint> outline =
		VSCalligraphicOutline(centerline, rawSettings);
	if (outline.size() < 4) return kNoErr;
	std::vector<AIPathSegment> segments(outline.size());
	for (size_t index = 0; index < outline.size(); ++index) {
		SetCorner(segments[index], {outline[index].x, outline[index].y});
	}
	return CreatePath(segments, true, true, false);
}

ASErr DrawSemicircle(
	const AIRealPoint& start,
	const AIRealPoint& end,
	AIReal strokeWidth)
{
	const AIRealPoint chord = Subtract(end, start);
	const AIReal length = Distance(start, end);
	if (length < kMinimumDrag) return kNoErr;
	const AIReal radius = length * 0.5;
	const AIRealPoint center = Scale(Add(start, end), 0.5);
	const AIRealPoint normal = Scale(
		AIRealPoint{-chord.v, chord.h},
		1.0 / length);
	const AIRealPoint top = Add(center, Scale(normal, radius));
	const AIRealPoint direction = Scale(chord, 1.0 / length);
	const AIReal tangent = radius * kCircleBezier;
	std::vector<AIPathSegment> segments(3);
	segments[0].p = start;
	segments[0].in = start;
	segments[0].out = Add(start, Scale(normal, tangent));
	segments[0].corner = false;
	segments[1].p = top;
	segments[1].in = Subtract(top, Scale(direction, tangent));
	segments[1].out = Add(top, Scale(direction, tangent));
	segments[1].corner = false;
	segments[2].p = end;
	segments[2].in = Add(end, Scale(normal, tangent));
	segments[2].out = end;
	segments[2].corner = false;
	return CreatePath(segments, false, false, true, strokeWidth);
}

ASErr DrawGeometryConstruction(
	const AIRealPoint& start,
	const AIRealPoint& end,
	const VSGeometrySettings& rawSettings)
{
	const VSGeometrySettings settings = VSSanitizeGeometry(rawSettings);
	const AIReal radius = Distance(start, end);
	if (settings.mode == 3) return DrawSemicircle(start, end, settings.strokeWidth);
	ASErr error = kNoErr;
	if (settings.mode == 0 || settings.mode == 1) {
		error = CreateCircle(start, radius, false, settings.strokeWidth);
		if (error) return error;
	}
	if (settings.mode == 1) return kNoErr;
	if (radius < kMinimumDrag) return kNoErr;
	const AIRealPoint radial = Subtract(end, start);
	const AIReal length = std::max<AIReal>(radius * 0.75, 12);
	const AIRealPoint normal = Scale(
		AIRealPoint{-radial.v, radial.h},
		length / radius);
	return CreateLine(
		Subtract(end, normal),
		Add(end, normal),
		settings.strokeWidth);
}

bool ClipLineToRectangle(
	AIReal left,
	AIReal top,
	AIReal right,
	AIReal bottom,
	AIRealPoint& start,
	AIRealPoint& end)
{
	const AIReal dx = end.h - start.h;
	const AIReal dy = end.v - start.v;
	AIReal minimum = 0;
	AIReal maximum = 1;
	auto clip = [&](AIReal p, AIReal q) {
		if (std::abs(p) < 1.0e-12) return q >= 0;
		const AIReal ratio = q / p;
		if (p < 0) {
			if (ratio > maximum) return false;
			if (ratio > minimum) minimum = ratio;
		}
		else {
			if (ratio < minimum) return false;
			if (ratio < maximum) maximum = ratio;
		}
		return true;
	};
	if (!clip(-dx, start.h - left) ||
		!clip(dx, right - start.h) ||
		!clip(-dy, start.v - top) ||
		!clip(dy, bottom - start.v)) {
		return false;
	}
	const AIRealPoint original = start;
	start = {original.h + minimum * dx, original.v + minimum * dy};
	end = {original.h + maximum * dx, original.v + maximum * dy};
	return true;
}

ASErr DrawTexture(
	const AIRealPoint& start,
	const AIRealPoint& end,
	const VSTextureSettings& rawSettings)
{
	const VSTextureSettings settings = VSSanitizeTexture(rawSettings);
	const AIReal left = std::min(start.h, end.h);
	const AIReal right = std::max(start.h, end.h);
	const AIReal top = std::min(start.v, end.v);
	const AIReal bottom = std::max(start.v, end.v);
	if (right - left < 2 || bottom - top < 2) return kNoErr;
	const AIRealPoint center = {(left + right) * 0.5, (top + bottom) * 0.5};
	const AIReal halfWidth = (right - left) * 0.5;
	const AIReal halfHeight = (bottom - top) * 0.5;
	const AIReal diagonal = std::sqrt(halfWidth * halfWidth + halfHeight * halfHeight) * 1.5;
	auto drawFamily = [&](AIReal angleDegrees) -> ASErr {
		const AIReal radians = angleDegrees * kDegreesToRadians;
		const AIRealPoint direction = {std::cos(radians), std::sin(radians)};
		const AIRealPoint normal = {-direction.v, direction.h};
		const AIReal extent = std::abs(normal.h) * halfWidth +
			std::abs(normal.v) * halfHeight;
		for (AIReal offset = -extent; offset <= extent + settings.spacing * 0.5;
			offset += settings.spacing) {
			const AIRealPoint base = Add(center, Scale(normal, offset));
			AIRealPoint lineStart = Subtract(base, Scale(direction, diagonal));
			AIRealPoint lineEnd = Add(base, Scale(direction, diagonal));
			if (!ClipLineToRectangle(left, top, right, bottom, lineStart, lineEnd)) continue;
			ASErr error = CreateLine(lineStart, lineEnd, settings.strokeWidth);
			if (error) return error;
		}
		return kNoErr;
	};
	ASErr error = drawFamily(settings.angle);
	if (!error && settings.crosshatch) error = drawFamily(settings.angle + 90.0);
	if (error) return error;
	return kNoErr;
}

ASErr DrawStipple(
	const AIRealPoint& start,
	const AIRealPoint& end,
	const VSStippleSettings& rawSettings)
{
	const VSStippleSettings settings = VSSanitizeStipple(rawSettings);
	const AIReal left = std::min(start.h, end.h);
	const AIReal right = std::max(start.h, end.h);
	const AIReal top = std::min(start.v, end.v);
	const AIReal bottom = std::max(start.v, end.v);
	if (right - left < 4 || bottom - top < 4) return kNoErr;

	int row = 0;
	int created = 0;
	for (AIReal y = top + settings.spacing * 0.5;
		y < bottom && created < settings.maximumDots;
		y += settings.spacing, ++row) {
		const AIReal offset = row % 2 ? settings.spacing * 0.5 : 0;
		for (AIReal x = left + settings.spacing * 0.5 + offset;
			x < right && created < settings.maximumDots;
			x += settings.spacing) {
			const AIReal phase = static_cast<AIReal>((row * 37 + created * 17) % 101) / 100.0;
			const AIReal radius = settings.radius *
				(1.0 + (phase * 2.0 - 1.0) * settings.variation);
			ASErr error = CreateCircle({x, y}, radius, true);
			if (error) return error;
			++created;
		}
	}
	return kNoErr;
}

ASErr AdjustStrokeWidth(AIReal delta)
{
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	const AIReal widthChange = delta / 24.0;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		AIArtHandle art = selection[index];
		if (!IsFullySelectedRoot(art)) continue;
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type) != kNoErr || type != kPathArt) continue;
		AIPathStyle style;
		AIBoolean hasAdvancedFill = false;
		ASErr error = sAIPathStyle->GetPathStyle(art, &style, &hasAdvancedFill);
		if (error) return error;
		style.strokePaint = true;
		style.stroke.width = Clamp(style.stroke.width + widthChange, 0.1, 1000);
		style.stroke.cap = kAIRoundCap;
		error = sAIPathStyle->SetPathStyle(art, &style);
		if (error) return error;
	}
	return kNoErr;
}

ASErr ApplyWidthSettings(const VSWidthSettings& rawSettings)
{
	const VSWidthSettings settings = VSSanitizeWidth(rawSettings);
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	std::vector<AIArtHandle> paths;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		if (!IsFullySelectedRoot(selection[index])) continue;
		ASErr error = CollectModulePaths(selection[index], paths);
		if (error) return error;
	}
	for (AIArtHandle path : paths) {
		AIPathStyle style;
		AIBoolean advanced = false;
		ASErr error = sAIPathStyle->GetPathStyle(path, &style, &advanced);
		if (error) return error;
		if (!style.strokePaint) continue;
		style.stroke.width = settings.mode == kVSWidthMultiply
			? Clamp(style.stroke.width * settings.value, 0.01, 1000)
			: settings.value;
		style.stroke.cap = static_cast<AILineCap>(settings.cap);
		style.stroke.join = static_cast<AILineJoin>(settings.join);
		error = sAIPathStyle->SetPathStyle(path, &style);
		if (error) return error;
	}
	return kNoErr;
}

AIReal DistanceFromLine(
	const AIRealPoint& point,
	const AIRealPoint& start,
	const AIRealPoint& end)
{
	const AIReal dx = end.h - start.h;
	const AIReal dy = end.v - start.v;
	const AIReal denominator = std::sqrt(dx * dx + dy * dy);
	if (denominator < 0.0001) return Distance(point, start);
	return std::abs(dy * point.h - dx * point.v + end.h * start.v - end.v * start.h) /
		denominator;
}

ASErr SimplifySelectedPaths(AIReal tolerance, bool preserveCurves = true)
{
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	std::vector<AIArtHandle> paths;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		AIArtHandle art = selection[index];
		if (!IsFullySelectedRoot(art)) continue;
		ASErr error = CollectModulePaths(art, paths);
		if (error) return error;
	}
	for (AIArtHandle art : paths) {

		ai::int16 count = 0;
		ASErr error = sAIPath->GetPathSegmentCount(art, &count);
		if (error || count < 3) continue;
		std::vector<AIPathSegment> source(static_cast<size_t>(count));
		error = sAIPath->GetPathSegments(art, 0, count, source.data());
		if (error) return error;

		AIBoolean closed = false;
		error = sAIPath->GetPathClosed(art, &closed);
		if (error) return error;
		std::vector<AIPathSegment> reduced;
		reduced.push_back(source.front());
		for (ai::int16 point = 1; point < count - 1; ++point) {
			const bool curved = Distance(source[point].in, source[point].p) > 0.001 ||
				Distance(source[point].out, source[point].p) > 0.001;
			if ((preserveCurves && curved) || DistanceFromLine(
					source[point].p,
					reduced.back().p,
					source[point + 1].p) >= tolerance) {
				reduced.push_back(source[point]);
			}
		}
		reduced.push_back(source.back());
		if (closed && reduced.size() < 3) continue;
		if (reduced.size() >= source.size()) continue;
		error = sAIPath->SetPathSegmentCount(
			art,
			static_cast<ai::int16>(reduced.size()));
		if (!error) {
			error = sAIPath->SetPathSegments(
				art,
				0,
				static_cast<ai::int16>(reduced.size()),
				reduced.data());
		}
		if (error) return error;
	}
	return kNoErr;
}

ASErr ConvertSelectedPathAnchors(bool smooth)
{
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	std::vector<AIArtHandle> paths;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		if (!IsFullySelectedRoot(selection[index])) continue;
		ASErr error = CollectModulePaths(selection[index], paths);
		if (error) return error;
	}
	for (AIArtHandle path : paths) {
		ai::int16 count = 0;
		ASErr error = sAIPath->GetPathSegmentCount(path, &count);
		if (error || count < 1) continue;
		std::vector<AIPathSegment> segments(static_cast<size_t>(count));
		error = sAIPath->GetPathSegments(path, 0, count, segments.data());
		if (error) return error;
		AIBoolean closed = false;
		error = sAIPath->GetPathClosed(path, &closed);
		if (error) return error;

		for (ai::int16 index = 0; index < count; ++index) {
			AIPathSegment& segment = segments[index];
			if (!smooth) {
				segment.in = segment.p;
				segment.out = segment.p;
				segment.corner = true;
				continue;
			}
			const bool hasPrevious = closed || index > 0;
			const bool hasNext = closed || index + 1 < count;
			if (!hasPrevious || !hasNext) {
				segment.in = segment.p;
				segment.out = segment.p;
				segment.corner = false;
				continue;
			}
			const AIRealPoint previous = segments[(index + count - 1) % count].p;
			const AIRealPoint next = segments[(index + 1) % count].p;
			const AIRealPoint tangent = Scale(Subtract(next, previous), 1.0 / 6.0);
			segment.in = Subtract(segment.p, tangent);
			segment.out = Add(segment.p, tangent);
			segment.corner = false;
		}
		error = sAIPath->SetPathSegments(path, 0, count, segments.data());
		if (error) return error;
	}
	return kNoErr;
}

ASErr ReverseSelectedPaths()
{
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	std::vector<AIArtHandle> paths;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		if (!IsFullySelectedRoot(selection[index])) continue;
		ASErr error = CollectModulePaths(selection[index], paths);
		if (error) return error;
	}
	for (AIArtHandle path : paths) {
		ASErr error = sAIPath->ReversePathDirection(path);
		if (error) return error;
	}
	return kNoErr;
}

ASErr MoveSelection(AIReal dx, AIReal dy)
{
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		if (!IsFullySelectedRoot(selection[index])) continue;
		ASErr error = TransformAroundCenter(selection[index], dx, dy, 0, 1, 1);
		if (error) return error;
	}
	return kNoErr;
}

ASErr MirrorSelection(
	const VSMirrorSettings& rawSettings,
	const AIRealPoint* interactiveOrigin = nullptr)
{
	const VSMirrorSettings settings = VSSanitizeMirror(rawSettings);
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	if (selection.Count() == 0) return kNoErr;

	std::vector<AIArtHandle> sources;
	AIRealRect selectionBounds = {};
	bool hasBounds = false;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		AIArtHandle source = selection[index];
		if (!IsFullySelectedRoot(source)) continue;
		AIRealRect bounds = {};
		ASErr error = sAIArt->GetArtBounds(source, &bounds);
		if (error) return error;
		if (!hasBounds) {
			selectionBounds = bounds;
			hasBounds = true;
		}
		else {
			selectionBounds.left = std::min(selectionBounds.left, bounds.left);
			selectionBounds.right = std::max(selectionBounds.right, bounds.right);
			selectionBounds.top = std::max(selectionBounds.top, bounds.top);
			selectionBounds.bottom = std::min(selectionBounds.bottom, bounds.bottom);
		}
		sources.push_back(source);
	}
	if (!hasBounds || sources.empty()) return kNoErr;

	AIReal originX = interactiveOrigin
		? interactiveOrigin->h
		: (selectionBounds.left + selectionBounds.right) * 0.5;
	AIReal originY = interactiveOrigin
		? interactiveOrigin->v
		: (selectionBounds.top + selectionBounds.bottom) * 0.5;
	if (settings.mode == kVSMirrorVertical ||
		settings.mode == kVSMirrorBoth ||
		settings.mode == kVSMirrorRadial) {
		originX += settings.axisOffset;
	}
	if (settings.mode == kVSMirrorHorizontal || settings.mode == kVSMirrorBoth) {
		originY += settings.axisOffset;
	}

	for (AIArtHandle source : sources) {
		AIRealRect bounds = {};
		ASErr error = sAIArt->GetArtBounds(source, &bounds);
		if (error) return error;
		const AIReal centerX = (bounds.left + bounds.right) * 0.5;
		const AIReal centerY = (bounds.top + bounds.bottom) * 0.5;

		auto duplicateAndTransform = [&](AIReal translateX,
									 AIReal translateY,
									 AIReal rotation,
									 AIReal scaleX,
									 AIReal scaleY) -> ASErr {
			AIArtHandle duplicate = nullptr;
			ASErr duplicateError = sAIArt->DuplicateArt(
				source,
				kPlaceAbove,
				source,
				&duplicate);
			if (duplicateError) return duplicateError;
			return TransformAroundCenter(
				duplicate,
				translateX,
				translateY,
				rotation,
				scaleX,
				scaleY);
		};

		if (settings.mode == kVSMirrorVertical || settings.mode == kVSMirrorBoth) {
			error = duplicateAndTransform(2 * (originX - centerX), 0, 0, -1, 1);
			if (error) return error;
		}
		if (settings.mode == kVSMirrorHorizontal || settings.mode == kVSMirrorBoth) {
			error = duplicateAndTransform(0, 2 * (originY - centerY), 0, 1, -1);
			if (error) return error;
		}
		if (settings.mode == kVSMirrorBoth) {
			error = duplicateAndTransform(
				2 * (originX - centerX),
				2 * (originY - centerY),
				0,
				-1,
				-1);
			if (error) return error;
		}
		if (settings.mode == kVSMirrorRadial) {
			for (int copy = 0; copy < VSMirrorGeneratedCopyCount(settings); ++copy) {
				const AIReal angle = VSMirrorRadialAngle(settings, copy);
				const AIReal radians = angle * kDegreesToRadians;
				const AIReal vectorX = centerX - originX;
				const AIReal vectorY = centerY - originY;
				const AIReal targetX = originX +
					std::cos(radians) * vectorX - std::sin(radians) * vectorY;
				const AIReal targetY = originY +
					std::sin(radians) * vectorX + std::cos(radians) * vectorY;
				error = duplicateAndTransform(
					targetX - centerX,
					targetY - centerY,
					angle,
					1,
					1);
				if (error) return error;
			}
		}
	}
	return kNoErr;
}

ASErr AlignSelectionToCollision(const VSCollisionSettings& rawSettings)
{
	const VSCollisionSettings settings = VSSanitizeCollision(rawSettings);
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	std::vector<AIArtHandle> art;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		if (IsFullySelectedRoot(selection[index])) art.push_back(selection[index]);
	}
	if (art.size() < 2) {
		if (sAIUser) sAIUser->MessageAlert(ai::UnicodeString(
			"Collision Align: seleziona almeno due oggetti. Il primo è il riferimento."));
		return kNoErr;
	}

	AIRealRect anchorBounds = {};
	ASErr error = sAIArt->GetArtBounds(art.front(), &anchorBounds);
	if (error) return error;
	for (size_t index = 1; index < art.size(); ++index) {
		AIRealRect movingBounds = {};
		error = sAIArt->GetArtBounds(art[index], &movingBounds);
		if (error) return error;
		const VSBounds anchor = {
			anchorBounds.left,
			anchorBounds.top,
			anchorBounds.right,
			anchorBounds.bottom
		};
		const VSBounds moving = {
			movingBounds.left,
			movingBounds.top,
			movingBounds.right,
			movingBounds.bottom
		};
		const VSTransformSample placement =
			VSCollisionPlacement(anchor, moving, settings);
		error = TransformAroundCenter(
			art[index],
			placement.translateX,
			placement.translateY,
			0,
			1,
			1);
		if (error) return error;
		error = sAIArt->GetArtBounds(art[index], &anchorBounds);
		if (error) return error;
	}
	return kNoErr;
}

ASErr ReformNearestPoint(const AIRealPoint& cursor)
{
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	AIArtHandle target = nullptr;
	AIReal bestDistance = 1.0e20;
	ai::int16 bestIndex = -1;

	for (ai::int32 artIndex = 0; artIndex < selection.Count(); ++artIndex) {
		AIArtHandle art = selection[artIndex];
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type) != kNoErr || type != kPathArt) continue;
		ai::int16 count = 0;
		if (sAIPath->GetPathSegmentCount(art, &count) != kNoErr || count < 1) continue;
		std::vector<AIPathSegment> segments(static_cast<size_t>(count));
		if (sAIPath->GetPathSegments(art, 0, count, segments.data()) != kNoErr) continue;
		for (ai::int16 index = 0; index < count; ++index) {
			const AIReal candidate = Distance(segments[index].p, cursor);
			if (candidate < bestDistance) {
				bestDistance = candidate;
				bestIndex = index;
				target = art;
			}
		}
	}
	if (!target || bestIndex < 0) return kNoErr;

	AIPathSegment segment;
	ASErr error = sAIPath->GetPathSegments(target, bestIndex, 1, &segment);
	if (error) return error;
	const AIRealPoint delta = Subtract(cursor, segment.p);
	segment.p = cursor;
	segment.in = Add(segment.in, delta);
	segment.out = Add(segment.out, delta);
	return sAIPath->SetPathSegments(target, bestIndex, 1, &segment);
}

ASErr ReformPointsWithFalloff(
	const AIRealPoint& origin,
	const AIRealPoint& cursor,
	const VSShapeSettings& rawSettings)
{
	const VSShapeSettings settings = VSSanitizeShape(rawSettings);
	const AIRealPoint delta = Subtract(cursor, origin);
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	std::vector<AIArtHandle> paths;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		if (!IsFullySelectedRoot(selection[index])) continue;
		ASErr error = CollectModulePaths(selection[index], paths);
		if (error) return error;
	}
	for (AIArtHandle path : paths) {
		ai::int16 count = 0;
		ASErr error = sAIPath->GetPathSegmentCount(path, &count);
		if (error || count < 1) continue;
		std::vector<AIPathSegment> segments(static_cast<size_t>(count));
		error = sAIPath->GetPathSegments(path, 0, count, segments.data());
		if (error) return error;
		bool changed = false;
		for (AIPathSegment& segment : segments) {
			const AIReal weight = VSShapeFalloff(Distance(origin, segment.p), settings);
			if (weight <= 0) continue;
			const AIRealPoint movement = Scale(delta, weight);
			segment.p = Add(segment.p, movement);
			if (settings.preserveHandles) {
				segment.in = Add(segment.in, movement);
				segment.out = Add(segment.out, movement);
			}
			else {
				segment.in = segment.p;
				segment.out = segment.p;
				segment.corner = true;
			}
			changed = true;
		}
		if (changed) {
			error = sAIPath->SetPathSegments(path, 0, count, segments.data());
			if (error) return error;
		}
	}
	return kNoErr;
}

ASErr AdjustOpacity(AIReal delta)
{
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	const AIReal opacity = Clamp(0.5 + delta / 240.0, 0.05, 1.0);
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		if (!IsFullySelectedRoot(selection[index])) continue;
		ASErr error = sAIBlendStyle->SetOpacity(selection[index], opacity);
		if (error) return error;
	}
	return kNoErr;
}

ASErr ApplyLiveStyleSettings(const VSLiveStyleSettings& rawSettings)
{
	const VSLiveStyleSettings settings = VSSanitizeLiveStyle(rawSettings);
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		AIArtHandle art = selection[index];
		if (!IsFullySelectedRoot(art)) continue;
		ASErr error = sAIBlendStyle->SetOpacity(art, settings.opacity);
		if (!error) {
			error = sAIBlendStyle->SetBlendingMode(
				art,
				static_cast<AIBlendingMode>(settings.blendMode));
		}
		if (!error) error = sAIBlendStyle->SetIsolated(art, settings.isolated != 0);
		if (error) return error;
	}
	return kNoErr;
}

bool AdjustIllustratorColor(AIColor& color, const VSColorSettings& settings)
{
	if (color.kind == kThreeColor) {
		const VSRGBColor adjusted = VSAdjustRGB(
			{color.c.rgb.red, color.c.rgb.green, color.c.rgb.blue},
			settings);
		color.c.rgb.red = adjusted.red;
		color.c.rgb.green = adjusted.green;
		color.c.rgb.blue = adjusted.blue;
		return true;
	}
	if (color.kind == kGrayColor) {
		const VSRGBColor adjusted = VSAdjustRGB(
			{color.c.g.gray, color.c.g.gray, color.c.g.gray},
			settings);
		color.c.g.gray = VSUnit(
			adjusted.red * 0.2126 + adjusted.green * 0.7152 + adjusted.blue * 0.0722);
		return true;
	}
	if (color.kind == kFourColor) {
		VSRGBColor rgb = VSCMYKToRGB(
			color.c.f.cyan,
			color.c.f.magenta,
			color.c.f.yellow,
			color.c.f.black);
		rgb = VSAdjustRGB(rgb, settings);
		double cyan = 0;
		double magenta = 0;
		double yellow = 0;
		double black = 0;
		VSRGBToCMYK(rgb, cyan, magenta, yellow, black);
		color.c.f.cyan = cyan;
		color.c.f.magenta = magenta;
		color.c.f.yellow = yellow;
		color.c.f.black = black;
		return true;
	}
	return false;
}

ASErr ApplyColorSettings(const VSColorSettings& rawSettings)
{
	const VSColorSettings settings = VSSanitizeColor(rawSettings);
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	std::vector<AIArtHandle> paths;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		if (!IsFullySelectedRoot(selection[index])) continue;
		ASErr error = CollectModulePaths(selection[index], paths);
		if (error) return error;
	}
	for (AIArtHandle path : paths) {
		AIPathStyle style;
		AIBoolean advanced = false;
		ASErr error = sAIPathStyle->GetPathStyle(path, &style, &advanced);
		if (error) return error;
		bool changed = false;
		if (style.fillPaint) changed |= AdjustIllustratorColor(style.fill.color, settings);
		if (style.strokePaint) changed |= AdjustIllustratorColor(style.stroke.color, settings);
		if (changed) {
			error = sAIPathStyle->SetPathStyle(path, &style);
			if (error) return error;
		}
	}
	return kNoErr;
}

ASErr AdjustMonochrome(AIReal delta)
{
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	const AIReal gray = Clamp(0.5 + delta / 240.0, 0, 1);
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		AIArtHandle art = selection[index];
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type) != kNoErr || type != kPathArt) continue;
		AIPathStyle style;
		AIBoolean advanced = false;
		ASErr error = sAIPathStyle->GetPathStyle(art, &style, &advanced);
		if (error) return error;
		if (style.fillPaint) {
			style.fill.color.kind = kGrayColor;
			style.fill.color.c.g.gray = gray;
		}
		if (style.strokePaint) {
			style.stroke.color.kind = kGrayColor;
			style.stroke.color.c.g.gray = gray;
		}
		error = sAIPathStyle->SetPathStyle(art, &style);
		if (error) return error;
	}
	return kNoErr;
}

ASErr RandomizeSelection(AIReal amount)
{
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	const AIReal magnitude = Clamp(std::abs(amount), 0, 240);
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		if (!IsFullySelectedRoot(selection[index])) continue;
		const AIReal phase = static_cast<AIReal>((index * 137 + 43) % 360);
		const AIReal radians = phase * 3.14159265358979323846 / 180.0;
		const AIReal dx = std::cos(radians) * magnitude * 0.3;
		const AIReal dy = std::sin(radians) * magnitude * 0.3;
		const AIReal rotation = std::sin(radians * 1.7) * magnitude * 0.18;
		const AIReal scale = Clamp(1.0 + std::cos(radians * 2.3) * magnitude / 600.0, 0.25, 2.0);
		ASErr error = TransformAroundCenter(
			selection[index],
			dx,
			dy,
			rotation,
			scale,
			scale);
		if (error) return error;
	}
	return kNoErr;
}

ASErr RandomizeSelection(const VSRandomizeSettings& rawSettings)
{
	const VSRandomizeSettings settings = VSSanitizeRandomize(rawSettings);
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	std::uint32_t item = 0;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		if (!IsFullySelectedRoot(selection[index])) continue;
		const VSTransformSample sample = VSSampleTransform(settings, item++);
		ASErr error = TransformAroundCenter(
			selection[index],
			sample.translateX,
			sample.translateY,
			sample.rotation,
			sample.scaleX,
			sample.scaleY);
		if (error) return error;
	}
	return kNoErr;
}

VSFindColor FindColorDescriptor(const AIColor& color)
{
	VSFindColor descriptor = {};
	descriptor.kind = color.kind;
	switch (color.kind) {
		case kGrayColor:
			descriptor.first = color.c.g.gray;
			break;
		case kFourColor:
			descriptor.first = color.c.f.cyan;
			descriptor.second = color.c.f.magenta;
			descriptor.third = color.c.f.yellow;
			descriptor.fourth = color.c.f.black;
			break;
		case kThreeColor:
			descriptor.first = color.c.rgb.red;
			descriptor.second = color.c.rgb.green;
			descriptor.third = color.c.rgb.blue;
			break;
		case kCustomColor:
			descriptor.first = color.c.c.tint;
			descriptor.identity = reinterpret_cast<std::uintptr_t>(color.c.c.color);
			break;
		case kPattern:
			descriptor.first = color.c.p.rotate;
			descriptor.identity = reinterpret_cast<std::uintptr_t>(color.c.p.pattern);
			break;
		case kGradient:
			descriptor.first = color.c.b.gradientAngle;
			descriptor.identity = reinterpret_cast<std::uintptr_t>(color.c.b.gradient);
			break;
		default:
			break;
	}
	return descriptor;
}

ASErr BuildFindDescriptor(AIArtHandle art, VSFindDescriptor& descriptor)
{
	AIPathStyle style;
	AIBoolean advanced = false;
	ASErr error = sAIPathStyle->GetPathStyle(art, &style, &advanced);
	if (error) return error;
	descriptor.appearance.fillPaint = style.fillPaint;
	descriptor.appearance.fill = FindColorDescriptor(style.fill.color);
	descriptor.appearance.strokePaint = style.strokePaint;
	descriptor.appearance.stroke = FindColorDescriptor(style.stroke.color);
	descriptor.appearance.strokeWidth = style.stroke.width;
	descriptor.appearance.opacity = sAIBlendStyle->GetOpacity(art);

	ai::int16 segments = 0;
	error = sAIPath->GetPathSegmentCount(art, &segments);
	if (error) return error;
	AIBoolean closed = false;
	error = sAIPath->GetPathClosed(art, &closed);
	if (error) return error;
	AIRealRect bounds = {};
	error = sAIArt->GetArtBounds(art, &bounds);
	if (error) return error;
	descriptor.geometry.segmentCount = segments;
	descriptor.geometry.closed = closed;
	descriptor.geometry.width = std::abs(bounds.right - bounds.left);
	descriptor.geometry.height = std::abs(bounds.top - bounds.bottom);
	return kNoErr;
}

ASErr FirstSelectedPath(AIArtHandle& reference)
{
	reference = nullptr;
	SelectedArt selected;
	if (selected.Error()) return selected.Error();
	for (ai::int32 index = 0; index < selected.Count(); ++index) {
		if (!IsFullySelectedRoot(selected[index])) continue;
		std::vector<AIArtHandle> paths;
		ASErr error = CollectModulePaths(selected[index], paths);
		if (error) return error;
		if (!paths.empty()) {
			reference = paths.front();
			return kNoErr;
		}
	}
	return kNoErr;
}

ASErr SelectMatchingPaths(VSFindCriterion criterion)
{
	AIArtHandle referenceArt = nullptr;
	ASErr error = FirstSelectedPath(referenceArt);
	if (error) return error;
	if (!referenceArt) {
		if (sAIUser) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Smart Find: seleziona un tracciato di riferimento."));
		}
		return kNoErr;
	}
	VSFindDescriptor reference = {};
	error = BuildFindDescriptor(referenceArt, reference);
	if (error) return error;

	MatchingArt paths(kPathArt);
	if (paths.Error()) return paths.Error();
	int matches = 0;
	int skipped = 0;
	for (ai::int32 index = 0; index < paths.Count(); ++index) {
		ai::int32 attributes = 0;
		error = sAIArt->GetArtUserAttr(
			paths[index],
			kArtLocked | kArtHidden,
			&attributes);
		if (error) return error;
		if (attributes & (kArtLocked | kArtHidden)) {
			++skipped;
			continue;
		}
		VSFindDescriptor candidate = {};
		error = BuildFindDescriptor(paths[index], candidate);
		if (error) return error;
		const bool matchesCandidate = VSFindDescriptorMatches(
			reference,
			candidate,
			criterion,
			0.1,
			0.01);
		error = sAIArt->SetArtUserAttr(
			paths[index],
			kArtSelected | kArtFullySelected,
			matchesCandidate
				? kArtSelected | kArtFullySelected
				: 0);
		if (error) return error;
		if (matchesCandidate) ++matches;
	}
	char report[192] = {};
	std::snprintf(
		report,
		sizeof(report),
		"Smart Find completato\nCorrispondenze: %d\nOggetti bloccati o nascosti ignorati: %d",
		matches,
		skipped);
	if (sAIUser) sAIUser->MessageAlert(ai::UnicodeString(report));
	return kNoErr;
}

ASErr ApplyCurrentStyleToSelectedPaths()
{
	AIPathStyle style;
	AIPathStyleMap styleMap;
	AIDictionaryRef advancedStroke = nullptr;
	ASErr error = sAIPathStyle->GetCurrentPathStyle(
		&style,
		&styleMap,
		&advancedStroke,
		nullptr);
	if (error) return error;
	SelectedArt selected;
	if (selected.Error()) return selected.Error();
	std::vector<AIArtHandle> paths;
	for (ai::int32 index = 0; index < selected.Count(); ++index) {
		if (!IsFullySelectedRoot(selected[index])) continue;
		error = CollectModulePaths(selected[index], paths);
		if (error) return error;
	}
	int applied = 0;
	for (AIArtHandle path : paths) {
		error = sAIPathStyle->SetPathStyle(path, &style);
		if (error) return error;
		++applied;
	}
	char report[128] = {};
	std::snprintf(
		report,
		sizeof(report),
		"Smart Find\nStile corrente applicato a %d tracciati.",
		applied);
	if (sAIUser) sAIUser->MessageAlert(ai::UnicodeString(report));
	return kNoErr;
}

ASErr RepairSelectedPaths()
{
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	std::vector<AIArtHandle> paths;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		AIArtHandle art = selection[index];
		if (!IsFullySelectedRoot(art)) continue;
		ASErr error = CollectModulePaths(art, paths);
		if (error) return error;
	}
	if (paths.empty()) {
		if (sAIUser) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Vector Suite: seleziona tracciati, gruppi o tracciati composti da riparare."));
		}
		return kNoErr;
	}

	int repairedPaths = 0;
	int removedAnchors = 0;
	int skippedDegenerate = 0;
	for (AIArtHandle art : paths) {
		ai::int16 count = 0;
		ASErr error = sAIPath->GetPathSegmentCount(art, &count);
		if (error) return error;
		if (count < 2) {
			++skippedDegenerate;
			continue;
		}

		std::vector<AIPathSegment> source(static_cast<size_t>(count));
		error = sAIPath->GetPathSegments(art, 0, count, source.data());
		if (error) return error;
		AIBoolean closed = false;
		error = sAIPath->GetPathClosed(art, &closed);
		if (error) return error;

		std::vector<VSRepairAnchor> anchors;
		std::vector<bool> protectedAnchors;
		anchors.reserve(source.size());
		protectedAnchors.reserve(source.size());
		for (const AIPathSegment& segment : source) {
			anchors.push_back({segment.p.h, segment.p.v});
			protectedAnchors.push_back(
				Distance(segment.in, segment.p) > 0.001 ||
				Distance(segment.out, segment.p) > 0.001);
		}
		const std::vector<size_t> keep = VSComputeRepairKeepIndices(
			anchors,
			protectedAnchors,
			closed,
			0.05,
			true,
			true);
		std::vector<AIPathSegment> repaired;
		repaired.reserve(keep.size());
		for (size_t keptIndex : keep) repaired.push_back(source[keptIndex]);
		const size_t minimum = closed ? 3 : 2;
		if (repaired.size() < minimum) {
			++skippedDegenerate;
			continue;
		}
		if (repaired.size() == source.size()) continue;
		error = sAIPath->SetPathSegmentCount(
			art,
			static_cast<ai::int16>(repaired.size()));
		if (!error) {
			error = sAIPath->SetPathSegments(
				art,
				0,
				static_cast<ai::int16>(repaired.size()),
				repaired.data());
		}
		if (error) return error;
		++repairedPaths;
		removedAnchors += static_cast<int>(source.size() - repaired.size());
	}
	char report[256] = {};
	std::snprintf(
		report,
		sizeof(report),
		"Vector Repair completato\nTracciati analizzati: %zu\nTracciati riparati: %d\nPunti rimossi: %d\nTracciati degeneri lasciati intatti: %d",
		paths.size(),
		repairedPaths,
		removedAnchors,
		skippedDegenerate);
	if (sAIUser) sAIUser->MessageAlert(ai::UnicodeString(report));
	return kNoErr;
}

ASErr SelectRasterArt()
{
	MatchingArt placed(kPlacedArt);
	if (placed.Error()) return placed.Error();
	MatchingArt raster(kRasterArt);
	if (raster.Error()) return raster.Error();

	for (ai::int32 index = 0; index < placed.Count(); ++index) {
		ASErr error = sAIArt->SetArtUserAttr(
			placed[index],
			kArtSelected | kArtFullySelected,
			kArtSelected | kArtFullySelected);
		if (error) return error;
	}
	for (ai::int32 index = 0; index < raster.Count(); ++index) {
		ASErr error = sAIArt->SetArtUserAttr(
			raster[index],
			kArtSelected | kArtFullySelected,
			kArtSelected | kArtFullySelected);
		if (error) return error;
	}
	return kNoErr;
}

ASErr EmbedSelectedRasterArt()
{
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	int embedded = 0;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		AIArtHandle art = selection[index];
		if (!IsFullySelectedRoot(art)) continue;
		short type = kUnknownArt;
		ASErr error = sAIArt->GetArtType(art, &type);
		if (error) return error;
		if (type != kPlacedArt) continue;
		AIArtHandle native = nullptr;
		error = sAIPlaced->MakePlacedObjectNative(art, &native, false);
		if (error) return error;
		++embedded;
	}
	if (sAIUser) {
		char report[160] = {};
		std::snprintf(report, sizeof(report),
			"Raster Lab: %d collegamenti incorporati.", embedded);
		sAIUser->MessageAlert(ai::UnicodeString(report));
	}
	return kNoErr;
}

ASErr ResampleSelectedRasterArt(const VSRasterSettings& rawSettings)
{
	const VSRasterSettings settings = VSSanitizeRaster(rawSettings);
	RasterizeSuiteLease rasterize;
	if (rasterize.Error()) {
		if (sAIUser) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Raster Lab: il motore di ricampionamento non è ancora disponibile. "
				"Riprova dopo l’apertura completa del documento."));
		}
		return rasterize.Error();
	}
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	int created = 0;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		AIArtHandle art = selection[index];
		if (!IsFullySelectedRoot(art)) continue;
		short type = kUnknownArt;
		ASErr error = sAIArt->GetArtType(art, &type);
		if (error) return error;
		if (type != kRasterArt) continue;
		AIArtHandle resampled = nullptr;
		error = rasterize->ImageResample(
			art,
			static_cast<AIResamplingType>(settings.resampling),
			settings.resolution,
			kPlaceAbove,
			art,
			&resampled);
		if (error) return error;
		if (resampled) {
			sAIArt->SetArtUserAttr(
				art,
				kArtSelected | kArtFullySelected,
				0);
			sAIArt->SetArtUserAttr(
				resampled,
				kArtSelected | kArtFullySelected,
				kArtSelected | kArtFullySelected);
			++created;
		}
	}
	if (sAIUser) {
		char report[192] = {};
		std::snprintf(report, sizeof(report),
			"Raster Lab: %d copie ricampionate a %.0f ppi. Gli originali sono rimasti intatti.",
			created,
			settings.resolution);
		sAIUser->MessageAlert(ai::UnicodeString(report));
	}
	return kNoErr;
}

ASErr SetSelectedRasterResolution(const VSRasterSettings& rawSettings)
{
	const VSRasterSettings settings = VSSanitizeRaster(rawSettings);
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	int changed = 0;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		AIArtHandle art = selection[index];
		if (!IsFullySelectedRoot(art)) continue;
		short type = kUnknownArt;
		ASErr error = sAIArt->GetArtType(art, &type);
		if (error) return error;
		if (type != kRasterArt && type != kPlacedArt) continue;
		error = sAIRaster->SetRasterNativeResolution(
			art,
			settings.resolution,
			settings.resolution);
		if (error) return error;
		++changed;
	}
	if (sAIUser) {
		char report[160] = {};
		std::snprintf(report, sizeof(report),
			"Raster Lab: risoluzione nativa impostata su %d oggetti.", changed);
		sAIUser->MessageAlert(ai::UnicodeString(report));
	}
	return kNoErr;
}

bool ModuleNeedsSelection(VSModuleID module)
{
	switch (module) {
		case kVSWidthStudio:
		case kVSPathStudio:
		case kVSCollisionAlign:
		case kVSMirrorStudio:
		case kVSShapeReform:
		case kVSLiveStyle:
		case kVSColorLab:
		case kVSRandomize:
			return true;
		default:
			return false;
	}
}

} // namespace

void VSSnapSet(const VSSnap::Settings* settings)
{
	if (!settings) return;
	gSnapSettings = *settings;
	gSnapSettings.modes &= VSSnap::All;
	gSnapSettings.pixels = std::isfinite(settings->pixels)
		? std::clamp(settings->pixels, 1.0, 32.0) : 8.0;
}

VSSnap::Settings VSSnapGet() { return gSnapSettings; }

void VectorSuitePlugin::RefreshSnapGeometry()
{
	fSnapGeometry = {};
	if (!gSnapSettings.enabled) return;
	MatchingArt art(kPathArt);
	if (art.Error()) return;
	for (ai::int32 i=0; i<art.Count(); ++i) {
		bool excluded=false;
		for (AIArtHandle ancestor=art[i]; ancestor;) {
			ai::int32 flags=0;
			if (sAIArt->GetArtUserAttr(ancestor,kArtHidden|kArtLocked,&flags)||flags) { excluded=true;break; }
			AIArtHandle parent=nullptr;
			if(sAIArt->GetArtParent(ancestor,&parent)) { excluded=true;break; }
			ancestor=parent;
		}
		if(excluded)continue;
		AIBoolean guide=false,closed=false;
		short count=0;
		if(sAIPath->GetPathGuide(art[i],&guide)||guide||sAIPath->GetPathSegmentCount(art[i],&count)||count<1)continue;
		std::vector<AIPathSegment> points(count);
		if(sAIPath->GetPathSegments(art[i],0,count,points.data())||sAIPath->GetPathClosed(art[i],&closed))continue;
		bool valid=true;
		for(const auto& point:points)for(const AIRealPoint p:{point.p,point.in,point.out})
			if(!std::isfinite(p.h)||!std::isfinite(p.v))valid=false;
		if(!valid)continue;
		for(const auto& point:points)fSnapGeometry.anchors.push_back({point.p.h,point.p.v});
		std::vector<VSSnap::Cubic> pathCurves;
		for(int j=0;j<(closed?count:count-1);++j) {
			const auto& a=points[j];const auto& b=points[(j+1)%count];
			VSSnap::Cubic curve;
			curve.p={VSSnap::Point{a.p.h,a.p.v},VSSnap::Point{a.out.h,a.out.v},VSSnap::Point{b.in.h,b.in.v},VSSnap::Point{b.p.h,b.p.v}};
			if(gSnapSettings.modes&VSSnap::Center)pathCurves.push_back(curve);
			if(Distance(a.p,a.out)>1e-9||Distance(b.p,b.in)>1e-9) {
				if(gSnapSettings.modes&VSSnap::Midpoint)curve.midpoint=VSSnap::arcMidpoint(curve);
				fSnapGeometry.curves.push_back(curve);
			} else fSnapGeometry.lines.push_back({{a.p.h,a.p.v},{b.p.h,b.p.v}});
		}
		VSSnap::Point center;
		if(closed&&VSSnap::centroid(pathCurves,center))fSnapGeometry.centers.push_back(center);
	}
}

void VSRandomizeSet(const VSRandomizeSettings* settings)
{
	if (settings) gRandomizeSettings = VSSanitizeRandomize(*settings);
}

void VSMirrorSet(const VSMirrorSettings* settings)
{
	if (settings) gMirrorSettings = VSSanitizeMirror(*settings);
}

void VSCollisionSet(const VSCollisionSettings* settings)
{
	if (settings) gCollisionSettings = VSSanitizeCollision(*settings);
}

void VSWidthSet(const VSWidthSettings* settings)
{
	if (settings) gWidthSettings = VSSanitizeWidth(*settings);
}

void VSLiveStyleSet(const VSLiveStyleSettings* settings)
{
	if (settings) gLiveStyleSettings = VSSanitizeLiveStyle(*settings);
}

void VSColorSet(const VSColorSettings* settings)
{
	if (settings) gColorSettings = VSSanitizeColor(*settings);
}

void VSAutoSaveSet(const VSAutoSaveSettings* settings)
{
	if (settings) gAutoSaveSettings = VSSanitizeAutoSave(*settings);
}

VSAutoSaveSettings VSAutoSaveGet(void)
{
	return gAutoSaveSettings;
}

void VSRasterSet(const VSRasterSettings* settings)
{
	if (settings) gRasterSettings = VSSanitizeRaster(*settings);
}

VSRasterSettings VSRasterGet(void)
{
	return gRasterSettings;
}

void VSPathSet(const VSPathSettings* settings)
{
	if (settings) gPathSettings = VSSanitizePath(*settings);
}

void VSPrecisionSet(const VSPrecisionSettings* settings)
{
	if (settings) gPrecisionSettings = VSSanitizePrecision(*settings);
}

void VSFluidSet(const VSFluidSettings* settings)
{
	if (settings) gFluidSettings = VSSanitizeFluid(*settings);
}

void VSInkSet(const VSInkSettings* settings)
{
	if (settings) gInkSettings = VSSanitizeInk(*settings);
}

void VSTextureSet(const VSTextureSettings* settings)
{
	if (settings) gTextureSettings = VSSanitizeTexture(*settings);
}

void VSStippleSet(const VSStippleSettings* settings)
{
	if (settings) gStippleSettings = VSSanitizeStipple(*settings);
}

void VSGeometrySet(const VSGeometrySettings* settings)
{
	if (settings) gGeometrySettings = VSSanitizeGeometry(*settings);
}

void VSShapeSet(const VSShapeSettings* settings)
{
	if (settings) gShapeSettings = VSSanitizeShape(*settings);
}

VSPathSettings VSPathGet(void)
{
	return gPathSettings;
}

ASErr VectorSuitePlugin::ExecuteModuleCommand(VSModuleID module)
{
	switch (module) {
		case kVSSmartFind:
			return SelectMatchingPaths(kVSFindByAppearance);
		case kVSVectorRepair:
			return RepairSelectedPaths();
		case kVSRasterLab:
			return SelectRasterArt();
		case kVSAutoSave:
			return sAIActionManager->PlayActionEvent(
				kAISaveDocumentAction,
				kDialogOff,
				nullptr);
		default:
			return kNoErr;
	}
}

ASErr VectorSuitePlugin::ExecuteSmartFindCommand(int command)
{
	switch (command) {
		case kVSPanelSmartFindAppearance:
			return SelectMatchingPaths(kVSFindByAppearance);
		case kVSPanelSmartFindGeometry:
			return SelectMatchingPaths(kVSFindByGeometry);
		case kVSPanelSmartFindExact:
			return SelectMatchingPaths(kVSFindByAppearanceAndGeometry);
		case kVSPanelSmartFindApplyStyle:
			return ApplyCurrentStyleToSelectedPaths();
		default:
			return kBadParameterErr;
	}
}

ASErr VectorSuitePlugin::ExecuteTransformCommand(int command)
{
	switch (command) {
		case kVSPanelCollisionApply:
			return AlignSelectionToCollision(gCollisionSettings);
		case kVSPanelMirrorApply:
			return MirrorSelection(gMirrorSettings);
		case kVSPanelRandomizeApply:
			return RandomizeSelection(gRandomizeSettings);
		default:
			return kBadParameterErr;
	}
}

ASErr VectorSuitePlugin::ExecuteStyleCommand(int command)
{
	switch (command) {
		case kVSPanelWidthApply:
			return ApplyWidthSettings(gWidthSettings);
		case kVSPanelLiveStyleApply:
			return ApplyLiveStyleSettings(gLiveStyleSettings);
		case kVSPanelColorApply:
			return ApplyColorSettings(gColorSettings);
		default:
			return kBadParameterErr;
	}
}

ASErr VectorSuitePlugin::ExecuteRasterCommand(int command)
{
	switch (command) {
		case kVSPanelRasterSelect:
			return SelectRasterArt();
		case kVSPanelRasterEmbed:
			return EmbedSelectedRasterArt();
		case kVSPanelRasterResample:
			return ResampleSelectedRasterArt(gRasterSettings);
		case kVSPanelRasterSetResolution:
			return SetSelectedRasterResolution(gRasterSettings);
		default:
			return kBadParameterErr;
	}
}

ASErr VectorSuitePlugin::ExecutePathCommand(int command)
{
	switch (command) {
		case kVSPanelPathSimplify:
			return SimplifySelectedPaths(
				gPathSettings.tolerance,
				gPathSettings.preserveCurves != 0);
		case kVSPanelPathCorner:
			return ConvertSelectedPathAnchors(false);
		case kVSPanelPathSmooth:
			return ConvertSelectedPathAnchors(true);
		case kVSPanelPathReverse:
			return ReverseSelectedPaths();
		default:
			return kBadParameterErr;
	}
}

int VectorSuitePlugin::GenerateFractalFromPanel(const VSFractalSettings* settings)
{
	if (!settings) return -1;

	ai::ArtboardList artboards;
	ASErr error = sAIArtboard->GetArtboardList(artboards);
	if (error) {
		sAIUser->MessageAlert(ai::UnicodeString(
			"Vector Suite: apri un documento prima di generare un albero."));
		return -1;
	}

	ai::ArtboardID active = -1;
	error = sAIArtboard->GetActive(artboards, active);
	if (error || active < 0) {
		sAIUser->MessageAlert(ai::UnicodeString(
			"Vector Suite: nessuna tavola da disegno attiva."));
		return -1;
	}

	ai::ArtboardProperties properties;
	error = sAIArtboard->GetArtboardProperties(artboards, active, properties);
	AIRealRect bounds = {0, 0, 0, 0};
	if (!error) error = sAIArtboard->GetPosition(properties, bounds);
	if (error) {
		sAIUser->MessageAlert(ai::UnicodeString(
			"Vector Suite: impossibile leggere la tavola da disegno attiva."));
		return -1;
	}

	if (settings->replace && settings->group && fLastFractalGroup &&
		sAIArt->ValidArt(fLastFractalGroup, true)) {
		error = sAIArt->DisposeArt(fLastFractalGroup);
		if (error) return -1;
		fLastFractalGroup = nullptr;
	}

	AIArtHandle group = nullptr;
	if (settings->group) {
		error = sAIArt->NewArt(kGroupArt, kPlaceAboveAll, nullptr, &group);
		if (error) return -1;
		error = sAIArt->SetArtName(
			group,
			ai::UnicodeString("Vector Suite — Fractal Grove"));
		if (error) {
			sAIArt->DisposeArt(group);
			return -1;
		}
	}

	FractalContext context(*settings);
	context.container = group;
	const AIRealPoint origin = {
		(bounds.left + bounds.right) * 0.5,
		bounds.bottom
	};
	GrowFractalBranch(
		context,
		origin,
		90,
		settings->initialLength,
		settings->initialWidth,
		0);

	if (context.error) {
		if (group && sAIArt->ValidArt(group, true)) sAIArt->DisposeArt(group);
		sAIUser->MessageAlert(ai::UnicodeString(
			"Vector Suite: la generazione dell’albero non è stata completata."));
		return -1;
	}
	if (group) {
		if (context.count == 0) {
			sAIArt->DisposeArt(group);
			fLastFractalGroup = nullptr;
		}
		else {
			fLastFractalGroup = group;
		}
	}
	return context.count;
}

ASErr VectorSuitePlugin::BeginModuleGesture(AIToolMessage* message, int toolIndex)
{
	ai::NOTUSED(message);
	if (toolIndex < 0 || toolIndex >= kVSToolCount) return kNoErr;
	const VSModuleID module = kVSTools[toolIndex].module;

	switch (module) {
		case kVSSmartFind:
		case kVSVectorRepair:
		case kVSRasterLab:
		case kVSAutoSave:
			fGestureActive = false;
			fGestureToolIndex = -1;
			return ExecuteModuleCommand(module);
		default:
			break;
	}

	if (ModuleNeedsSelection(module)) {
		SelectedArt selection;
		if (selection.Error()) return selection.Error();
		if (selection.Count() == 0) {
			fGestureActive = false;
			fGestureToolIndex = -1;
			sAIUser->MessageAlert(ai::UnicodeString(
				"Vector Suite: seleziona almeno un oggetto prima di usare questo modulo."));
			return kNoErr;
		}
	}
	return sAIAnnotator->SetAnnotatorActive(fAnnotatorHandle, true);
}

ASErr VectorSuitePlugin::DrawModuleAnnotation(
	AIAnnotatorMessage* message,
	AIRect& bounds)
{
	bounds = {0, 0, 0, 0};
	if (!message || !message->drawer || !fGestureActive ||
		fGestureToolIndex < 0 || fGestureToolIndex >= kVSToolCount) {
		return kNoErr;
	}

	bool hasBounds = false;
	auto includePoint = [&](const AIPoint& point, ai::int32 padding = 6) {
		if (!hasBounds) {
			bounds.left = point.h - padding;
			bounds.top = point.v - padding;
			bounds.right = point.h + padding;
			bounds.bottom = point.v + padding;
			hasBounds = true;
			return;
		}
		bounds.left = std::min<ai::int32>(bounds.left, point.h - padding);
		bounds.top = std::min<ai::int32>(bounds.top, point.v - padding);
		bounds.right = std::max<ai::int32>(bounds.right, point.h + padding);
		bounds.bottom = std::max<ai::int32>(bounds.bottom, point.v + padding);
	};
	auto toView = [&](const AIRealPoint& artwork, AIPoint& view) -> ASErr {
		return sAIDocumentView->ArtworkPointToViewPoint(message->view, &artwork, &view);
	};
	const AIRGBColor black = {0, 0, 0};
	const AIRGBColor white = {65535, 65535, 65535};
	auto drawLine = [&](const AIPoint& start, const AIPoint& end,
		AIBoolean dashed = false) -> ASErr {
		includePoint(start);
		includePoint(end);
		sAIAnnotatorDrawer->SetLineDashed(message->drawer, dashed);
		sAIAnnotatorDrawer->SetColor(message->drawer, black);
		sAIAnnotatorDrawer->SetLineWidth(message->drawer, 3.0);
		ASErr error = sAIAnnotatorDrawer->DrawLine(message->drawer, start, end);
		if (error) return error;
		sAIAnnotatorDrawer->SetColor(message->drawer, white);
		sAIAnnotatorDrawer->SetLineWidth(message->drawer, 1.0);
		return sAIAnnotatorDrawer->DrawLine(message->drawer, start, end);
	};
	auto drawEllipse = [&](const AIRect& rect) -> ASErr {
		includePoint({rect.left, rect.top});
		includePoint({rect.right, rect.bottom});
		sAIAnnotatorDrawer->SetLineDashed(message->drawer, false);
		sAIAnnotatorDrawer->SetColor(message->drawer, black);
		sAIAnnotatorDrawer->SetLineWidth(message->drawer, 3.0);
		ASErr error = sAIAnnotatorDrawer->DrawEllipse(message->drawer, rect, false);
		if (error) return error;
		sAIAnnotatorDrawer->SetColor(message->drawer, white);
		sAIAnnotatorDrawer->SetLineWidth(message->drawer, 1.0);
		return sAIAnnotatorDrawer->DrawEllipse(message->drawer, rect, false);
	};
	auto drawRect = [&](const AIRect& rect) -> ASErr {
		includePoint({rect.left, rect.top});
		includePoint({rect.right, rect.bottom});
		sAIAnnotatorDrawer->SetLineDashed(message->drawer, true);
		sAIAnnotatorDrawer->SetColor(message->drawer, black);
		sAIAnnotatorDrawer->SetLineWidth(message->drawer, 3.0);
		ASErr error = sAIAnnotatorDrawer->DrawRect(message->drawer, rect, false);
		if (error) return error;
		sAIAnnotatorDrawer->SetColor(message->drawer, white);
		sAIAnnotatorDrawer->SetLineWidth(message->drawer, 1.0);
		return sAIAnnotatorDrawer->DrawRect(message->drawer, rect, false);
	};

	AIPoint startView = {};
	AIPoint endView = {};
	ASErr error = toView(fStartingPoint, startView);
	if (error) return error;
	error = toView(fEndPoint, endView);
	if (error) return error;

	const VSModuleID module = kVSTools[fGestureToolIndex].module;
	if ((module == kVSFluidSketch || module == kVSInkStudio) &&
		fGesturePoints.size() > 1) {
		AIPoint previous = startView;
		for (size_t index = 1; index < fGesturePoints.size(); ++index) {
			AIPoint current = {};
			error = toView(fGesturePoints[index], current);
			if (error) return error;
			error = drawLine(previous, current);
			if (error) return error;
			previous = current;
		}
	}
	else {
		error = drawLine(startView, endView, true);
		if (error) return error;
	}

	if (module == kVSMirrorStudio) {
		const VSMirrorSettings settings = VSSanitizeMirror(gMirrorSettings);
		AIRealPoint origin = fEndPoint;
		if (settings.mode == kVSMirrorVertical ||
			settings.mode == kVSMirrorBoth ||
			settings.mode == kVSMirrorRadial) {
			origin.h += settings.axisOffset;
		}
		if (settings.mode == kVSMirrorHorizontal || settings.mode == kVSMirrorBoth) {
			origin.v += settings.axisOffset;
		}
		AIPoint originView = {};
		error = toView(origin, originView);
		if (error) return error;
		AIRealRect visible = {};
		error = sAIDocumentView->GetDocumentViewVisibleArea(message->view, &visible);
		if (error) return error;
		auto drawArtworkLine = [&](const AIRealPoint& a, const AIRealPoint& b) -> ASErr {
			AIPoint av = {};
			AIPoint bv = {};
			ASErr lineError = toView(a, av);
			if (lineError) return lineError;
			lineError = toView(b, bv);
			return lineError ? lineError : drawLine(av, bv, true);
		};
		if (settings.mode == kVSMirrorVertical || settings.mode == kVSMirrorBoth) {
			error = drawArtworkLine({origin.h, visible.top}, {origin.h, visible.bottom});
			if (error) return error;
		}
		if (settings.mode == kVSMirrorHorizontal || settings.mode == kVSMirrorBoth) {
			error = drawArtworkLine({visible.left, origin.v}, {visible.right, origin.v});
			if (error) return error;
		}
		if (settings.mode == kVSMirrorRadial) {
			const AIReal dragRadius = std::hypot(
				static_cast<AIReal>(endView.h - startView.h),
				static_cast<AIReal>(endView.v - startView.v));
			const AIReal radius = Clamp(dragRadius, 48.0, 180.0);
			for (int sector = 0; sector < settings.copies; ++sector) {
				const AIReal angle = 2.0 * 3.14159265358979323846 * sector /
					static_cast<AIReal>(settings.copies);
				const AIPoint rayEnd = {
					originView.h + static_cast<ai::int32>(std::cos(angle) * radius),
					originView.v + static_cast<ai::int32>(std::sin(angle) * radius)};
				error = drawLine(originView, rayEnd, true);
				if (error) return error;
			}
			error = drawEllipse({
				originView.h - static_cast<ai::int32>(radius),
				originView.v - static_cast<ai::int32>(radius),
				originView.h + static_cast<ai::int32>(radius),
				originView.v + static_cast<ai::int32>(radius)});
			if (error) return error;
		}
	}
	else if (module == kVSGeometryLab) {
		const VSGeometrySettings settings = VSSanitizeGeometry(gGeometrySettings);
		const AIReal radius = std::hypot(
			static_cast<AIReal>(endView.h - startView.h),
			static_cast<AIReal>(endView.v - startView.v));
		if (settings.mode == 0 || settings.mode == 1) {
			error = drawEllipse({
				startView.h - static_cast<ai::int32>(radius),
				startView.v - static_cast<ai::int32>(radius),
				startView.h + static_cast<ai::int32>(radius),
				startView.v + static_cast<ai::int32>(radius)});
			if (error) return error;
		}
		if ((settings.mode == 0 || settings.mode == 2) && radius > 0.5) {
			const AIReal dx = endView.h - startView.h;
			const AIReal dy = endView.v - startView.v;
			const AIReal halfLength = std::max<AIReal>(12.0, radius * 0.75);
			const AIPoint tangentStart = {
				endView.h + static_cast<ai::int32>(-dy / radius * halfLength),
				endView.v + static_cast<ai::int32>(dx / radius * halfLength)};
			const AIPoint tangentEnd = {
				endView.h - static_cast<ai::int32>(-dy / radius * halfLength),
				endView.v - static_cast<ai::int32>(dx / radius * halfLength)};
			error = drawLine(tangentStart, tangentEnd);
			if (error) return error;
		}
	}
	else if (module == kVSShapeReform) {
		AIRealPoint radiusPoint = {fStartingPoint.h + gShapeSettings.radius, fStartingPoint.v};
		AIPoint radiusView = {};
		error = toView(radiusPoint, radiusView);
		if (error) return error;
		const ai::int32 radius = std::abs(radiusView.h - startView.h);
		error = drawEllipse({
			startView.h - radius,
			startView.v - radius,
			startView.h + radius,
			startView.v + radius});
		if (error) return error;
	}
	else if (module == kVSTextureLab || module == kVSStippleLab) {
		error = drawRect({
			std::min(startView.h, endView.h),
			std::min(startView.v, endView.v),
			std::max(startView.h, endView.h),
			std::max(startView.v, endView.v)});
		if (error) return error;
	}

	const ai::int32 handleRadius = 4;
	const AIRect startHandle = {
		startView.h - handleRadius,
		startView.v - handleRadius,
		startView.h + handleRadius,
		startView.v + handleRadius};
	includePoint(startView, handleRadius + 2);
	sAIAnnotatorDrawer->SetLineDashed(message->drawer, false);
	sAIAnnotatorDrawer->SetColor(message->drawer, black);
	error = sAIAnnotatorDrawer->DrawEllipse(message->drawer, startHandle, true);
	if (error) return error;
	sAIAnnotatorDrawer->SetColor(message->drawer, white);
	sAIAnnotatorDrawer->SetLineWidth(message->drawer, 1.0);
	return sAIAnnotatorDrawer->DrawEllipse(message->drawer, startHandle, false);
}

ASErr VectorSuitePlugin::ContinueModuleGesture(AIToolMessage* message, int toolIndex)
{
	if (toolIndex < 0 || toolIndex >= kVSToolCount) return kNoErr;
	const VSModuleID module = kVSTools[toolIndex].module;
	fEndPoint = message->cursor;
	// Fluid Sketch e Ink Studio devono seguire fedelmente ogni campione del
	// gesto. Gli altri strumenti sono geometrici o trasformativi e usano lo
	// snapping nativo di Illustrator per punti, Smart Guides, griglia e tavola.
	SnapCursor(message, fEndPoint);
	const bool straight = message->event && (message->event->modifiers & aiEventModifiers_shiftKey);
	if (straight) {
		const AIReal dx = fEndPoint.h - fStartingPoint.h;
		const AIReal dy = fEndPoint.v - fStartingPoint.v;
		const AIReal angle = std::round(std::atan2(dy, dx) / (kDegreesToRadians * 45)) * kDegreesToRadians * 45;
		const AIReal length = std::hypot(dx, dy);
		fEndPoint = {fStartingPoint.h + length * std::cos(angle), fStartingPoint.v + length * std::sin(angle)};
	}

	if (module == kVSFluidSketch || module == kVSInkStudio) {
		if (straight) fGesturePoints = {fStartingPoint, fEndPoint};
		const AIReal sampleDistance = module == kVSFluidSketch
			? gFluidSettings.sampleDistance
			: std::max<AIReal>(0.5, gInkSettings.nibWidth * 0.12);
		if (fGesturePoints.empty() ||
			Distance(fGesturePoints.back(), fEndPoint) >= sampleDistance) {
			fGesturePoints.push_back(fEndPoint);
		}
	}

	ASErr error = sAIUndo->UndoChanges();
	if (error) return error;

	const AIReal dx = fEndPoint.h - fStartingPoint.h;
	const AIReal dy = fEndPoint.v - fStartingPoint.v;
	error = InvalidateRect(oldAnnotatorRect);
	if (error) return error;

	switch (module) {
		case kVSPrecisionPen: {
			VSPrecisionSettings settings=gPrecisionSettings;
			if (fHasCustomSnap) { settings.angleStep=0;settings.fixedLength=0; }
			return DrawPrecisionLine(fStartingPoint, fEndPoint, settings);
		}
		case kVSFluidSketch:
			return DrawSmoothGesture(
				fGesturePoints,
				gFluidSettings.strokeWidth,
				gFluidSettings.smoothing,
				gFluidSettings.closePath != 0);
		case kVSInkStudio:
			return DrawCalligraphicGesture(fGesturePoints, gInkSettings);
		case kVSWidthStudio:
			return AdjustStrokeWidth(-dy);
		case kVSPathStudio:
			return SimplifySelectedPaths(Clamp(std::abs(dx) / 20.0, 0.25, 12));
		case kVSGeometryLab:
			return DrawGeometryConstruction(
				fStartingPoint,
				fEndPoint,
				gGeometrySettings);
		case kVSCollisionAlign:
			return MoveSelection(dx, dy);
		case kVSMirrorStudio:
			return MirrorSelection(gMirrorSettings, &fEndPoint);
		case kVSShapeReform:
			return ReformPointsWithFalloff(
				fStartingPoint,
				fEndPoint,
				gShapeSettings);
		case kVSLiveStyle:
			return AdjustOpacity(dx);
		case kVSColorLab:
			return AdjustMonochrome(dx);
		case kVSTextureLab:
			return DrawTexture(fStartingPoint, fEndPoint, gTextureSettings);
		case kVSStippleLab:
			return DrawStipple(fStartingPoint, fEndPoint, gStippleSettings);
		case kVSRandomize:
			return RandomizeSelection(dx);
		default:
			return kNoErr;
	}
}

ASErr VectorSuitePlugin::EndModuleGesture(AIToolMessage* message, int toolIndex)
{
	ai::NOTUSED(message);
	if (toolIndex < 0 || toolIndex >= kVSToolCount) return kNoErr;
	const VSModuleID module = kVSTools[toolIndex].module;
	fGesturePoints.clear();
	switch (module) {
		case kVSSmartFind:
		case kVSVectorRepair:
		case kVSRasterLab:
		case kVSAutoSave:
			return kNoErr;
		default:
			return sAIAnnotator->SetAnnotatorActive(fAnnotatorHandle, false);
	}
}
