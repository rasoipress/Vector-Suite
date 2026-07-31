#include "IllustratorSDK.h"
#include "VectorSuitePlugin.h"
#include "SDKErrors.h"

#include <algorithm>
#include <cmath>

namespace {

const AIReal kCos30 = 0.8660254037844386;
const AIReal kSin30 = 0.5;
const AIReal kBezierCircle = 0.5522847498307936;

AIRealPoint Add(const AIRealPoint& a, const AIRealPoint& b)
{
	AIRealPoint result = {a.h + b.h, a.v + b.v};
	return result;
}

AIRealPoint Scale(const AIRealPoint& point, AIReal amount)
{
	AIRealPoint result = {point.h * amount, point.v * amount};
	return result;
}

AIReal Dot(const AIRealPoint& a, const AIRealPoint& b)
{
	return a.h * b.h + a.v * b.v;
}

void SetCorner(AIPathSegment& segment, const AIRealPoint& point)
{
	segment.p = point;
	segment.in = point;
	segment.out = point;
	segment.corner = true;
}

ASErr ApplyVectorSuiteStyle(AIArtHandle path)
{
	AIPathStyle style;
	AIPathStyleMap styleMap;
	AIDictionaryRef advancedStroke = NULL;
	ASErr error = sAIPathStyle->GetCurrentPathStyle(
		&style,
		&styleMap,
		&advancedStroke,
		nullptr);
	if (error)
		return error;

	style.fillPaint = false;
	style.strokePaint = true;
	style.stroke.color.kind = kGrayColor;
	style.stroke.color.c.g.gray = kAIRealOne;
	if (style.stroke.width <= 0)
		style.stroke.width = kAIRealOne;
	return sAIPathStyle->SetPathStyle(path, &style);
}

ASErr CreatePath(const AIPathSegment* segments, ai::int16 count, AIBoolean closed)
{
	AIArtHandle path = NULL;
	ASErr error = sAIArt->NewArt(kPathArt, kPlaceAboveAll, NULL, &path);
	if (error)
		return error;

	error = sAIPath->SetPathSegmentCount(path, count);
	if (error)
		return error;

	error = sAIPath->SetPathSegments(path, 0, count, segments);
	if (error)
		return error;

	error = sAIPath->SetPathClosed(path, closed);
	if (error)
		return error;

	return ApplyVectorSuiteStyle(path);
}

void ResolveTopPlane(
	const AIRealPoint& start,
	const AIRealPoint& end,
	AIReal& axisX,
	AIReal& axisZ)
{
	const AIReal dx = end.h - start.h;
	const AIReal dy = end.v - start.v;
	axisX = dx / (2 * kCos30) + dy / (2 * kSin30);
	axisZ = -dx / (2 * kCos30) + dy / (2 * kSin30);
}

} // namespace

ASErr VectorSuitePlugin::CreateProjectionArt(AIToolMessage* message)
{
	sAIUndo->UndoChanges();
	fEndPoint = message->cursor;

	ASErr error = InvalidateRect(oldAnnotatorRect);
	if (error)
		return error;

	const AIRealPoint axisX = {kCos30, kSin30};
	const AIRealPoint axisZ = {-kCos30, kSin30};
	const AIRealPoint axisY = {0, 1};

	if (message->tool == fToolHandle[kVSToolProjectionLine])
	{
		const AIRealPoint delta = {
			fEndPoint.h - fStartingPoint.h,
			fEndPoint.v - fStartingPoint.v
		};
		const AIRealPoint axes[3] = {axisX, axisZ, axisY};
		AIReal bestProjection = Dot(delta, axes[0]);
		int bestAxis = 0;
		for (int index = 1; index < 3; ++index)
		{
			const AIReal projection = Dot(delta, axes[index]);
			if (std::abs(projection) > std::abs(bestProjection))
			{
				bestProjection = projection;
				bestAxis = index;
			}
		}

		fEndPoint = Add(fStartingPoint, Scale(axes[bestAxis], bestProjection));
		AIPathSegment segments[2];
		SetCorner(segments[0], fStartingPoint);
		SetCorner(segments[1], fEndPoint);
		return CreatePath(segments, 2, false);
	}

	AIReal extentX = 0;
	AIReal extentZ = 0;
	ResolveTopPlane(fStartingPoint, fEndPoint, extentX, extentZ);
	if (message->event->modifiers & aiEventModifiers_shiftKey)
	{
		const AIReal extent = std::max(std::abs(extentX), std::abs(extentZ));
		extentX = std::copysign(extent, extentX == 0 ? 1 : extentX);
		extentZ = std::copysign(extent, extentZ == 0 ? 1 : extentZ);
	}

	const AIRealPoint projectedX = Scale(axisX, extentX);
	const AIRealPoint projectedZ = Scale(axisZ, extentZ);

	if (message->tool == fToolHandle[kVSToolProjectionRectangle])
	{
		AIPathSegment segments[4];
		SetCorner(segments[0], fStartingPoint);
		SetCorner(segments[1], Add(fStartingPoint, projectedX));
		SetCorner(segments[2], Add(Add(fStartingPoint, projectedX), projectedZ));
		SetCorner(segments[3], Add(fStartingPoint, projectedZ));
		return CreatePath(segments, 4, true);
	}

	if (message->tool == fToolHandle[kVSToolProjectionEllipse])
	{
		const AIRealPoint halfX = Scale(projectedX, 0.5);
		const AIRealPoint halfZ = Scale(projectedZ, 0.5);
		const AIRealPoint center = Add(Add(fStartingPoint, halfX), halfZ);
		const AIRealPoint tangentX = Scale(halfX, kBezierCircle);
		const AIRealPoint tangentZ = Scale(halfZ, kBezierCircle);

		AIPathSegment segments[4];
		segments[0].p = Add(center, halfX);
		segments[0].in = Add(segments[0].p, Scale(tangentZ, -1));
		segments[0].out = Add(segments[0].p, tangentZ);
		segments[0].corner = false;

		segments[1].p = Add(center, halfZ);
		segments[1].in = Add(segments[1].p, tangentX);
		segments[1].out = Add(segments[1].p, Scale(tangentX, -1));
		segments[1].corner = false;

		segments[2].p = Add(center, Scale(halfX, -1));
		segments[2].in = Add(segments[2].p, tangentZ);
		segments[2].out = Add(segments[2].p, Scale(tangentZ, -1));
		segments[2].corner = false;

		segments[3].p = Add(center, Scale(halfZ, -1));
		segments[3].in = Add(segments[3].p, Scale(tangentX, -1));
		segments[3].out = Add(segments[3].p, tangentX);
		segments[3].corner = false;
		return CreatePath(segments, 4, true);
	}

	const AIReal dx = fEndPoint.h - fStartingPoint.h;
	const AIReal dy = fEndPoint.v - fStartingPoint.v;
	const AIReal size = std::max<AIReal>(8, std::sqrt(dx * dx + dy * dy) * 0.5);
	const AIRealPoint x = Scale(axisX, size);
	const AIRealPoint z = Scale(axisZ, size);
	const AIRealPoint y = Scale(axisY, size);
	const AIRealPoint origin = fStartingPoint;
	const AIRealPoint pointX = Add(origin, x);
	const AIRealPoint pointZ = Add(origin, z);
	const AIRealPoint pointY = Add(origin, y);
	const AIRealPoint pointXY = Add(pointX, y);
	const AIRealPoint pointZY = Add(pointZ, y);
	const AIRealPoint pointXZY = Add(Add(pointX, z), y);

	AIPathSegment top[4];
	SetCorner(top[0], pointY);
	SetCorner(top[1], pointXY);
	SetCorner(top[2], pointXZY);
	SetCorner(top[3], pointZY);
	error = CreatePath(top, 4, true);
	if (error)
		return error;

	AIPathSegment right[4];
	SetCorner(right[0], origin);
	SetCorner(right[1], pointX);
	SetCorner(right[2], pointXY);
	SetCorner(right[3], pointY);
	error = CreatePath(right, 4, true);
	if (error)
		return error;

	AIPathSegment left[4];
	SetCorner(left[0], origin);
	SetCorner(left[1], pointZ);
	SetCorner(left[2], pointZY);
	SetCorner(left[3], pointY);
	return CreatePath(left, 4, true);
}
