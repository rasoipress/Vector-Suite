#include "IllustratorSDK.h"
#include "SDKErrors.h"
#include "actions/AIDocumentAction.h"

#include "VectorSuitePlugin.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr AIReal kMinimumDrag = 0.01;
constexpr AIReal kCircleBezier = 0.5522847498307936;
constexpr AIReal kDegreesToRadians = 3.14159265358979323846 / 180.0;

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

bool HasSelectedAncestor(AIArtHandle art)
{
	AIArtHandle parent = nullptr;
	if (sAIArt->GetArtParent(art, &parent) != kNoErr) return false;
	while (parent) {
		ai::int32 attributes = 0;
		if (sAIArt->GetArtUserAttr(parent, kArtSelected, &attributes) == kNoErr &&
			(attributes & kArtSelected)) {
			return true;
		}
		AIArtHandle next = nullptr;
		if (sAIArt->GetArtParent(parent, &next) != kNoErr) break;
		parent = next;
	}
	return false;
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
		if (style.stroke.width < minimumStrokeWidth) {
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

ASErr CreateCircle(const AIRealPoint& center, AIReal radius, bool fill)
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
	return CreatePath(segments, true, fill, !fill, fill ? 0 : 1);
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

ASErr DrawPrecisionLine(const AIRealPoint& start, const AIRealPoint& cursor)
{
	const AIReal dx = cursor.h - start.h;
	const AIReal dy = cursor.v - start.v;
	const AIReal length = std::sqrt(dx * dx + dy * dy);
	if (length < kMinimumDrag) return kNoErr;
	const AIReal step = 15.0;
	const AIReal angle = std::atan2(dy, dx) * 180.0 / 3.14159265358979323846;
	const AIReal snapped = std::round(angle / step) * step;
	const AIReal radians = snapped * 3.14159265358979323846 / 180.0;
	const AIRealPoint end = {
		start.h + std::cos(radians) * length,
		start.v + std::sin(radians) * length
	};
	return CreateLine(start, end, 1);
}

ASErr DrawSmoothGesture(
	const std::vector<AIRealPoint>& points,
	AIReal minimumStrokeWidth)
{
	if (points.size() < 2) return kNoErr;
	std::vector<AIPathSegment> segments(points.size());
	for (size_t index = 0; index < points.size(); ++index) {
		const AIRealPoint previous = points[index == 0 ? index : index - 1];
		const AIRealPoint next = points[index + 1 < points.size() ? index + 1 : index];
		const AIRealPoint tangent = Scale(Subtract(next, previous), 1.0 / 6.0);
		segments[index].p = points[index];
		segments[index].in = Subtract(points[index], tangent);
		segments[index].out = Add(points[index], tangent);
		segments[index].corner = false;
	}
	return CreatePath(segments, false, false, true, minimumStrokeWidth);
}

ASErr DrawGeometryConstruction(const AIRealPoint& start, const AIRealPoint& end)
{
	const AIReal radius = Distance(start, end);
	ASErr error = CreateCircle(start, radius, false);
	if (error) return error;
	if (radius < kMinimumDrag) return kNoErr;
	const AIRealPoint radial = Subtract(end, start);
	const AIReal length = std::max<AIReal>(radius * 0.75, 12);
	const AIRealPoint normal = Scale(
		AIRealPoint{-radial.v, radial.h},
		length / radius);
	return CreateLine(Subtract(end, normal), Add(end, normal), 1);
}

ASErr DrawTexture(const AIRealPoint& start, const AIRealPoint& end)
{
	const AIReal left = std::min(start.h, end.h);
	const AIReal right = std::max(start.h, end.h);
	const AIReal top = std::min(start.v, end.v);
	const AIReal bottom = std::max(start.v, end.v);
	if (right - left < 2 || bottom - top < 2) return kNoErr;

	const AIReal spacing = Clamp((right - left + bottom - top) / 28.0, 5, 18);
	for (AIReal y = top; y <= bottom; y += spacing) {
		ASErr error = CreateLine({left, y}, {right, y}, 0.7);
		if (error) return error;
	}
	return kNoErr;
}

ASErr DrawStipple(const AIRealPoint& start, const AIRealPoint& end)
{
	const AIReal left = std::min(start.h, end.h);
	const AIReal right = std::max(start.h, end.h);
	const AIReal top = std::min(start.v, end.v);
	const AIReal bottom = std::max(start.v, end.v);
	if (right - left < 4 || bottom - top < 4) return kNoErr;

	const AIReal spacing = Clamp(std::sqrt((right - left) * (bottom - top) / 120.0), 6, 18);
	int row = 0;
	int created = 0;
	for (AIReal y = top + spacing * 0.5; y < bottom && created < 400; y += spacing, ++row) {
		const AIReal offset = row % 2 ? spacing * 0.5 : 0;
		for (AIReal x = left + spacing * 0.5 + offset; x < right && created < 400; x += spacing) {
			const AIReal radius = 0.9 + static_cast<AIReal>((row + created) % 4) * 0.35;
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
		if (HasSelectedAncestor(art)) continue;
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

ASErr SimplifySelectedPaths(AIReal tolerance)
{
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		AIArtHandle art = selection[index];
		if (HasSelectedAncestor(art)) continue;
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type) != kNoErr || type != kPathArt) continue;

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
			if (DistanceFromLine(
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

ASErr MoveSelection(AIReal dx, AIReal dy)
{
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		if (HasSelectedAncestor(selection[index])) continue;
		ASErr error = TransformAroundCenter(selection[index], dx, dy, 0, 1, 1);
		if (error) return error;
	}
	return kNoErr;
}

ASErr MirrorSelection(const AIRealPoint& origin, const AIRealPoint& cursor)
{
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	const bool vertical = std::abs(cursor.h - origin.h) >= std::abs(cursor.v - origin.v);
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		AIArtHandle source = selection[index];
		if (HasSelectedAncestor(source)) continue;
		AIArtHandle duplicate = nullptr;
		ASErr error = sAIArt->DuplicateArt(source, kPlaceAbove, source, &duplicate);
		if (error) return error;

		AIRealRect bounds;
		error = sAIArt->GetArtBounds(duplicate, &bounds);
		if (error) return error;
		const AIReal centerX = (bounds.left + bounds.right) * 0.5;
		const AIReal centerY = (bounds.top + bounds.bottom) * 0.5;
		const AIReal translateX = vertical ? 2 * (origin.h - centerX) : 0;
		const AIReal translateY = vertical ? 0 : 2 * (origin.v - centerY);
		error = TransformAroundCenter(
			duplicate,
			translateX,
			translateY,
			0,
			vertical ? -1 : 1,
			vertical ? 1 : -1);
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

ASErr AdjustOpacity(AIReal delta)
{
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	const AIReal opacity = Clamp(0.5 + delta / 240.0, 0.05, 1.0);
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		if (HasSelectedAncestor(selection[index])) continue;
		ASErr error = sAIBlendStyle->SetOpacity(selection[index], opacity);
		if (error) return error;
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
		if (HasSelectedAncestor(selection[index])) continue;
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

bool StylesMatch(AIArtHandle candidate, const AIPathStyle& reference)
{
	short type = kUnknownArt;
	if (sAIArt->GetArtType(candidate, &type) != kNoErr || type != kPathArt) return false;
	AIPathStyle style;
	AIBoolean advanced = false;
	if (sAIPathStyle->GetPathStyle(candidate, &style, &advanced) != kNoErr) return false;
	if (style.fillPaint != reference.fillPaint || style.strokePaint != reference.strokePaint) return false;
	if (style.strokePaint && std::abs(style.stroke.width - reference.stroke.width) > 0.01) return false;
	if (style.fillPaint && style.fill.color.kind != reference.fill.color.kind) return false;
	if (style.strokePaint && style.stroke.color.kind != reference.stroke.color.kind) return false;
	return true;
}

ASErr SelectSimilarPaths()
{
	SelectedArt selected;
	if (selected.Error()) return selected.Error();
	AIPathStyle reference;
	bool foundReference = false;
	for (ai::int32 index = 0; index < selected.Count(); ++index) {
		short type = kUnknownArt;
		if (sAIArt->GetArtType(selected[index], &type) == kNoErr && type == kPathArt) {
			AIBoolean advanced = false;
			if (sAIPathStyle->GetPathStyle(selected[index], &reference, &advanced) == kNoErr) {
				foundReference = true;
				break;
			}
		}
	}
	if (!foundReference) return kNoErr;

	MatchingArt paths(kPathArt);
	if (paths.Error()) return paths.Error();
	for (ai::int32 index = 0; index < paths.Count(); ++index) {
		ASErr error = sAIArt->SetArtUserAttr(
			paths[index],
			kArtSelected | kArtFullySelected,
			StylesMatch(paths[index], reference)
				? kArtSelected | kArtFullySelected
				: 0);
		if (error) return error;
	}
	return kNoErr;
}

ASErr RepairSelectedPaths()
{
	SelectedArt selection;
	if (selection.Error()) return selection.Error();
	for (ai::int32 index = selection.Count() - 1; index >= 0; --index) {
		AIArtHandle art = selection[index];
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type) != kNoErr || type != kPathArt) continue;
		ai::int16 count = 0;
		ASErr error = sAIPath->GetPathSegmentCount(art, &count);
		if (error) return error;
		if (count < 2) {
			error = sAIArt->DisposeArt(art);
			if (error) return error;
			continue;
		}

		std::vector<AIPathSegment> source(static_cast<size_t>(count));
		error = sAIPath->GetPathSegments(art, 0, count, source.data());
		if (error) return error;
		std::vector<AIPathSegment> repaired;
		for (const AIPathSegment& segment : source) {
			if (repaired.empty() || Distance(repaired.back().p, segment.p) > 0.01) {
				repaired.push_back(segment);
			}
		}
		AIBoolean closed = false;
		sAIPath->GetPathClosed(art, &closed);
		const size_t minimum = closed ? 3 : 2;
		if (repaired.size() < minimum || repaired.size() == source.size()) continue;
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
	}
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

ASErr VectorSuitePlugin::ExecuteModuleCommand(VSModuleID module)
{
	switch (module) {
		case kVSSmartFind:
			return SelectSimilarPaths();
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
			return ExecuteModuleCommand(module);
		default:
			break;
	}

	if (ModuleNeedsSelection(module)) {
		SelectedArt selection;
		if (selection.Error()) return selection.Error();
		if (selection.Count() == 0) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Vector Suite: seleziona almeno un oggetto prima di usare questo modulo."));
			return kNoErr;
		}
	}
	return sAIAnnotator->SetAnnotatorActive(fAnnotatorHandle, true);
}

ASErr VectorSuitePlugin::ContinueModuleGesture(AIToolMessage* message, int toolIndex)
{
	if (toolIndex < 0 || toolIndex >= kVSToolCount) return kNoErr;
	const VSModuleID module = kVSTools[toolIndex].module;
	fEndPoint = message->cursor;

	if (module == kVSFluidSketch || module == kVSInkStudio) {
		if (fGesturePoints.empty() || Distance(fGesturePoints.back(), fEndPoint) >= 1.5) {
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
		case kVSPrecisionPen:
			return DrawPrecisionLine(fStartingPoint, fEndPoint);
		case kVSFluidSketch:
			return DrawSmoothGesture(fGesturePoints, 1);
		case kVSInkStudio:
			return DrawSmoothGesture(fGesturePoints, 3.5);
		case kVSWidthStudio:
			return AdjustStrokeWidth(-dy);
		case kVSPathStudio:
			return SimplifySelectedPaths(Clamp(std::abs(dx) / 20.0, 0.25, 12));
		case kVSGeometryLab:
			return DrawGeometryConstruction(fStartingPoint, fEndPoint);
		case kVSCollisionAlign:
			return MoveSelection(dx, dy);
		case kVSMirrorStudio:
			return MirrorSelection(fStartingPoint, fEndPoint);
		case kVSShapeReform:
			return ReformNearestPoint(fEndPoint);
		case kVSLiveStyle:
			return AdjustOpacity(dx);
		case kVSColorLab:
			return AdjustMonochrome(dx);
		case kVSTextureLab:
			return DrawTexture(fStartingPoint, fEndPoint);
		case kVSStippleLab:
			return DrawStipple(fStartingPoint, fEndPoint);
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
