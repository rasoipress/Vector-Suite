#include "IllustratorSDK.h"
#include "VectorSuitePlugin.h"
#include "VectorSuiteProjection.h"
#include "VectorSuiteProjectionMath.h"
#include "VectorSuiteSelection.h"
#include "SDKErrors.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

// --- Impostazioni condivise di Projection Studio ----------------------------
//
// L'implementazione sta qui e non in un file a sé per non dover toccare né il
// progetto Xcode né l'elenco dei sorgenti: è un blocco di stato piccolo e
// autosufficiente, e vive accanto al codice che lo consuma.

namespace {

VSProjectionSettings gProjection = {
	30.0, 30.0, kVSPlaneTop, 1, 40.0, kVSProjectionAxisY,
	100.0, 100.0, 15.0, 10.0
};

double ClampAngle(double degrees)
{
	return std::max(0.0, std::min(89.0, degrees));
}

} // namespace

extern "C" VSProjectionSettings VSProjectionDefaults(void)
{
	VSProjectionSettings settings;
	settings.leftAngle = 30.0;
	settings.rightAngle = 30.0;
	settings.plane = kVSPlaneTop;
	settings.snapLine = 1;
	settings.moveDistance = 40.0;
	settings.moveAxis = kVSProjectionAxisY;
	settings.scaleU = 100.0;
	settings.scaleV = 100.0;
	settings.rotation = 15.0;
	settings.shear = 10.0;
	return settings;
}

extern "C" VSProjectionSettings VSProjectionGet(void)
{
	return gProjection;
}

extern "C" void VSProjectionSet(const VSProjectionSettings* settings)
{
	if (!settings) return;
	gProjection.leftAngle = ClampAngle(settings->leftAngle);
	gProjection.rightAngle = ClampAngle(settings->rightAngle);
	if (gProjection.leftAngle + gProjection.rightAngle < 1.0) gProjection.leftAngle = 1.0;
	gProjection.plane = std::max(0, std::min(2, settings->plane));
	gProjection.snapLine = settings->snapLine ? 1 : 0;
	gProjection.moveDistance = std::max(-2000.0, std::min(2000.0,
		settings->moveDistance));
	gProjection.moveAxis = std::max(0, std::min(
		static_cast<int>(kVSProjectionAxisCount) - 1,
		settings->moveAxis));
	gProjection.scaleU = std::max(1.0, std::min(1000.0, settings->scaleU));
	gProjection.scaleV = std::max(1.0, std::min(1000.0, settings->scaleV));
	gProjection.rotation = std::max(-360.0, std::min(360.0, settings->rotation));
	gProjection.shear = std::max(-80.0, std::min(80.0, settings->shear));
}

namespace {

const AIReal kBezierCircle = 0.5522847498307936;
const AIReal kDegreesToRadians = 0.017453292519943295;

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

ASErr CreatePath(
	const AIPathSegment* segments,
	ai::int16 count,
	AIBoolean closed,
	AIArtHandle* created = nullptr)
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

	error = ApplyVectorSuiteStyle(path);
	if (!error && created) *created = path;
	return error;
}

/// I due assi del piano attivo, come versori.
///
/// L'asse destro sale verso destra dell'angolo scelto, il sinistro verso
/// sinistra del proprio, il terzo è la verticale. Il piano decide quale coppia
/// forma la faccia su cui si disegna.
void PlaneAxes(AIRealPoint& first, AIRealPoint& second)
{
	const VSProjectionSettings settings = VSProjectionGet();
	const AIReal leftRadians = settings.leftAngle * kDegreesToRadians;
	const AIReal rightRadians = settings.rightAngle * kDegreesToRadians;

	const AIRealPoint right = {std::cos(rightRadians), std::sin(rightRadians)};
	const AIRealPoint left = {-std::cos(leftRadians), std::sin(leftRadians)};
	const AIRealPoint up = {0, 1};

	switch (settings.plane) {
	case kVSPlaneLeft:
		first = left;
		second = up;
		break;
	case kVSPlaneRight:
		first = right;
		second = up;
		break;
	default:
		first = right;
		second = left;
		break;
	}
}

/// Scompone lo spostamento del puntatore nelle due componenti del piano.
///
/// Risolve il sistema first·a + second·b = delta. Il determinante si annulla
/// solo se i due assi sono paralleli, che con gli angoli limitati a 1°–89° non
/// può accadere: il controllo resta come rete di sicurezza.
void ResolvePlane(
	const AIRealPoint& start,
	const AIRealPoint& end,
	AIReal& along,
	AIReal& across)
{
	AIRealPoint first, second;
	PlaneAxes(first, second);

	const AIReal dx = end.h - start.h;
	const AIReal dy = end.v - start.v;
	const AIReal determinant = first.h * second.v - first.v * second.h;
	if (std::abs(determinant) < 1e-9) {
		along = 0;
		across = 0;
		return;
	}
	along = (dx * second.v - dy * second.h) / determinant;
	across = (first.h * dy - first.v * dx) / determinant;
}

class ProjectionSelection
{
public:
	ProjectionSelection() : fArt(nullptr), fCount(0), fError(kNoErr)
	{
		fError = sAIMatchingArt->GetSelectedArt(&fArt, &fCount);
		if (fError) {
			fArt = nullptr;
			fCount = 0;
		}
	}

	~ProjectionSelection()
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

bool ProjectionHasFullySelectedAncestor(AIArtHandle art)
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

bool ProjectionIsFullySelectedRoot(AIArtHandle art)
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
		ProjectionHasFullySelectedAncestor(art));
}

ASErr CollectProjectionPaths(
	AIArtHandle art,
	std::vector<AIArtHandle>& paths)
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
		error = CollectProjectionPaths(child, paths);
		if (error) return error;
		AIArtHandle sibling = nullptr;
		error = sAIArt->GetArtSibling(child, &sibling);
		if (error) return error;
		child = sibling;
	}
	return kNoErr;
}

struct ProjectionPathSnapshot {
	std::vector<AIPathSegment> segments;
	AIPathStyle style;
	bool hasStyle;
};

const char* kProjectionTargetKey = "studio.vectorsuite.projection.target";
const char* kProjectionLeftAngleKey = "studio.vectorsuite.projection.leftAngle";
const char* kProjectionRightAngleKey = "studio.vectorsuite.projection.rightAngle";
const char* kProjectionReferenceXKey = "studio.vectorsuite.projection.referenceX";
const char* kProjectionReferenceYKey = "studio.vectorsuite.projection.referenceY";

struct ProjectionMetadata {
	VSProjectionTarget target;
	double leftAngle;
	double rightAngle;
	VSProjectionPoint reference;
};

bool ReadProjectionMetadata(AIArtHandle art, ProjectionMetadata& metadata)
{
	if (!sAIDictionary || !sAIArt->HasDictionary(art)) return false;
	AIDictionaryRef dictionary = nullptr;
	if (sAIArt->GetDictionary(art, &dictionary) != kNoErr || !dictionary) {
		return false;
	}
	ai::int32 target = 0;
	AIReal left = 0;
	AIReal right = 0;
	AIReal referenceX = 0;
	AIReal referenceY = 0;
	const bool valid =
		sAIDictionary->GetIntegerEntry(
			dictionary,
			sAIDictionary->Key(kProjectionTargetKey),
			&target) == kNoErr &&
		sAIDictionary->GetRealEntry(
			dictionary,
			sAIDictionary->Key(kProjectionLeftAngleKey),
			&left) == kNoErr &&
		sAIDictionary->GetRealEntry(
			dictionary,
			sAIDictionary->Key(kProjectionRightAngleKey),
			&right) == kNoErr &&
		sAIDictionary->GetRealEntry(
			dictionary,
			sAIDictionary->Key(kProjectionReferenceXKey),
			&referenceX) == kNoErr &&
		sAIDictionary->GetRealEntry(
			dictionary,
			sAIDictionary->Key(kProjectionReferenceYKey),
			&referenceY) == kNoErr &&
		target >= 0 && target < kVSProjectionTargetCount;
	sAIDictionary->Release(dictionary);
	if (!valid) return false;
	metadata.target = static_cast<VSProjectionTarget>(target);
	metadata.leftAngle = left;
	metadata.rightAngle = right;
	metadata.reference = {referenceX, referenceY};
	return true;
}

ASErr WriteProjectionMetadata(
	AIArtHandle art,
	const ProjectionMetadata& metadata)
{
	if (!sAIDictionary) return kCantHappenErr;
	AIDictionaryRef dictionary = nullptr;
	ASErr error = sAIArt->GetDictionary(art, &dictionary);
	if (error || !dictionary) return error ? error : kCantHappenErr;
	error = sAIDictionary->SetIntegerEntry(
		dictionary,
		sAIDictionary->Key(kProjectionTargetKey),
		metadata.target);
	if (!error) {
		error = sAIDictionary->SetRealEntry(
			dictionary,
			sAIDictionary->Key(kProjectionLeftAngleKey),
			metadata.leftAngle);
	}
	if (!error) {
		error = sAIDictionary->SetRealEntry(
			dictionary,
			sAIDictionary->Key(kProjectionRightAngleKey),
			metadata.rightAngle);
	}
	if (!error) {
		error = sAIDictionary->SetRealEntry(
			dictionary,
			sAIDictionary->Key(kProjectionReferenceXKey),
			metadata.reference.x);
	}
	if (!error) {
		error = sAIDictionary->SetRealEntry(
			dictionary,
			sAIDictionary->Key(kProjectionReferenceYKey),
			metadata.reference.y);
	}
	sAIDictionary->Release(dictionary);
	return error;
}

void ClearProjectionMetadata(AIArtHandle art)
{
	if (!sAIDictionary || !sAIArt->HasDictionary(art)) return;
	AIDictionaryRef dictionary = nullptr;
	if (sAIArt->GetDictionary(art, &dictionary) != kNoErr || !dictionary) return;
	for (const char* key : {
		kProjectionTargetKey,
		kProjectionLeftAngleKey,
		kProjectionRightAngleKey,
		kProjectionReferenceXKey,
		kProjectionReferenceYKey}) {
		const AIDictKey dictionaryKey = sAIDictionary->Key(key);
		if (sAIDictionary->IsKnown(dictionary, dictionaryKey)) {
			sAIDictionary->DeleteEntry(dictionary, dictionaryKey);
		}
	}
	sAIDictionary->Release(dictionary);
}

} // namespace

ASErr VectorSuitePlugin::TransformProjectionSelection(
	int target,
	AIBoolean inverse,
	AIBoolean copy)
{
	if (target < 0 || target >= kVSProjectionTargetCount) return kBadParameterErr;

	ProjectionSelection selection;
	if (selection.Error()) return selection.Error();
	std::vector<AIArtHandle> roots;
	AIRealRect selectionBounds = {};
	bool hasBounds = false;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		AIArtHandle art = selection[index];
		if (!ProjectionIsFullySelectedRoot(art)) continue;
		AIRealRect bounds;
		ASErr error = sAIArt->GetArtBounds(art, &bounds);
		if (error) return error;
		if (!hasBounds) {
			selectionBounds = bounds;
			hasBounds = true;
		}
		else {
			sAIRealMath->AIRealRectUnion(
				&selectionBounds,
				&bounds,
				&selectionBounds);
		}
		roots.push_back(art);
	}
	if (roots.empty() || !hasBounds) {
		if (sAIUser) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Vector Suite: seleziona almeno un oggetto da trasformare."));
		}
		return kNoErr;
	}

	const VSProjectionSettings settings = VSProjectionGet();
	const VSProjectionPoint defaultReference = {
		(selectionBounds.left + selectionBounds.right) * 0.5,
		(selectionBounds.top + selectionBounds.bottom) * 0.5
	};
	const ai::int32 flags =
		kTransformObjects |
		kTransformEntireArtStyle |
		kTransformChildren |
		kTransformLinkedMasks |
		kTransformNotifyPluginGroups;

	for (AIArtHandle source : roots) {
		ProjectionMetadata metadata = {
			static_cast<VSProjectionTarget>(target),
			settings.leftAngle,
			settings.rightAngle,
			defaultReference
		};
		if (inverse) ReadProjectionMetadata(source, metadata);
		VSProjectionLinearMatrix linear = VSMakeProjectionLinearMatrix(
			metadata.leftAngle,
			metadata.rightAngle,
			metadata.target);
		if (inverse) {
			VSProjectionLinearMatrix inverted = {};
			if (!VSInvertProjectionLinearMatrix(linear, &inverted)) {
				return kBadParameterErr;
			}
			linear = inverted;
		}
		const VSProjectionAffineMatrix projected =
			VSMakeCenteredProjectionMatrix(linear, metadata.reference);
		AIRealMatrix matrix = {
			projected.a,
			projected.b,
			projected.c,
			projected.d,
			projected.tx,
			projected.ty
		};
		const AIReal lineScale = std::sqrt(
			std::abs(VSProjectionDeterminant(linear)));
		AIArtHandle transformed = source;
		if (copy) {
			ASErr error = sAIArt->DuplicateArt(
				source,
				kPlaceAbove,
				source,
				&transformed);
			if (error) return error;
			error = sAIArt->SetArtUserAttr(
				source,
				kArtSelected | kArtFullySelected,
				0);
			if (error) return error;
			error = sAIArt->SetArtUserAttr(
				transformed,
				kArtSelected | kArtFullySelected,
				kArtSelected | kArtFullySelected);
			if (error) return error;
		}
		ASErr error = sAITransformArt->TransformArt(
			transformed,
			&matrix,
			lineScale,
			flags);
		if (error) return error;
		if (inverse) {
			ClearProjectionMetadata(transformed);
		}
		else {
			error = WriteProjectionMetadata(transformed, metadata);
			if (error) return error;
		}
	}
	return kNoErr;
}

ASErr VectorSuitePlugin::MoveOrExtrudeProjectionSelection(
	int axis,
	AIReal distance,
	AIBoolean extrude,
	AIBoolean copy)
{
	if (axis < 0 || axis >= kVSProjectionAxisCount) return kBadParameterErr;
	ProjectionSelection selection;
	if (selection.Error()) return selection.Error();
	std::vector<AIArtHandle> roots;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		AIArtHandle art = selection[index];
		if (ProjectionIsFullySelectedRoot(art)) roots.push_back(art);
	}
	if (roots.empty()) {
		if (sAIUser) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Vector Suite: seleziona almeno un oggetto da spostare."));
		}
		return kNoErr;
	}

	const VSProjectionSettings settings = VSProjectionGet();
	const VSProjectionPoint unit = VSMakeProjectionAxisVector(
		settings.leftAngle,
		settings.rightAngle,
		static_cast<VSProjectionAxis>(axis));
	const AIRealPoint offset = {
		unit.x * distance,
		unit.y * distance
	};
	AIRealMatrix matrix;
	sAIRealMath->AIRealMatrixSetTranslate(&matrix, offset.h, offset.v);
	const ai::int32 flags =
		kTransformObjects |
		kTransformEntireArtStyle |
		kTransformChildren |
		kTransformLinkedMasks |
		kTransformNotifyPluginGroups;

	for (AIArtHandle source : roots) {
		std::vector<ProjectionPathSnapshot> pathSnapshots;
		if (extrude) {
			std::vector<AIArtHandle> sourcePaths;
			ASErr error = CollectProjectionPaths(source, sourcePaths);
			if (error) return error;
			for (AIArtHandle path : sourcePaths) {
				ai::int16 segmentCount = 0;
				error = sAIPath->GetPathSegmentCount(path, &segmentCount);
				if (error) return error;
				if (segmentCount <= 0) continue;

				ProjectionPathSnapshot snapshot = {};
				snapshot.segments.resize(static_cast<size_t>(segmentCount));
				error = sAIPath->GetPathSegments(
					path,
					0,
					segmentCount,
					snapshot.segments.data());
				if (error) return error;
				AIBoolean advanced = false;
				snapshot.hasStyle = sAIPathStyle->GetPathStyle(
					path,
					&snapshot.style,
					&advanced) == kNoErr;
				if (snapshot.hasStyle) {
					if (!snapshot.style.strokePaint && snapshot.style.fillPaint) {
						snapshot.style.stroke.color = snapshot.style.fill.color;
					}
					snapshot.style.fillPaint = false;
					snapshot.style.strokePaint = true;
					if (snapshot.style.stroke.width <= 0) {
						snapshot.style.stroke.width = 1.0;
					}
				}
				pathSnapshots.push_back(std::move(snapshot));
			}
		}

		AIArtHandle transformed = source;
		if (copy || extrude) {
			ASErr error = sAIArt->DuplicateArt(
				source,
				kPlaceAbove,
				source,
				&transformed);
			if (error) return error;
			error = sAIArt->SetArtUserAttr(
				source,
				kArtSelected | kArtFullySelected,
				0);
			if (error) return error;
			error = sAIArt->SetArtUserAttr(
				transformed,
				kArtSelected | kArtFullySelected,
				kArtSelected | kArtFullySelected);
			if (error) return error;
		}
		ASErr error = sAITransformArt->TransformArt(
			transformed,
			&matrix,
			1.0,
			flags);
		if (error) return error;

		if (!extrude) continue;
		for (const ProjectionPathSnapshot& snapshot : pathSnapshots) {
			for (const AIPathSegment& segment : snapshot.segments) {
				AIPathSegment connector[2];
				SetCorner(connector[0], segment.p);
				SetCorner(connector[1], Add(segment.p, offset));
				AIArtHandle connectorArt = nullptr;
				error = CreatePath(connector, 2, false, &connectorArt);
				if (error) return error;
				if (snapshot.hasStyle) {
					error = sAIPathStyle->SetPathStyle(
						connectorArt,
						&snapshot.style);
					if (error) return error;
				}
			}
		}
	}
	return kNoErr;
}

ASErr VectorSuitePlugin::TransformProjectionPlaneSelection(
	int operation,
	AIBoolean copy)
{
	if (operation != kVSPanelProjectionScale &&
		operation != kVSPanelProjectionRotate &&
		operation != kVSPanelProjectionShear) {
		return kBadParameterErr;
	}

	ProjectionSelection selection;
	if (selection.Error()) return selection.Error();
	std::vector<AIArtHandle> roots;
	AIRealRect selectionBounds = {};
	bool hasBounds = false;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		AIArtHandle art = selection[index];
		if (!ProjectionIsFullySelectedRoot(art)) continue;
		AIRealRect bounds = {};
		ASErr error = sAIArt->GetArtBounds(art, &bounds);
		if (error) return error;
		if (!hasBounds) {
			selectionBounds = bounds;
			hasBounds = true;
		}
		else {
			sAIRealMath->AIRealRectUnion(
				&selectionBounds,
				&bounds,
				&selectionBounds);
		}
		roots.push_back(art);
	}
	if (roots.empty() || !hasBounds) {
		if (sAIUser) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Vector Suite: seleziona almeno un oggetto da trasformare."));
		}
		return kNoErr;
	}

	const VSProjectionSettings settings = VSProjectionGet();
	double scaleU = 1.0;
	double scaleV = 1.0;
	double rotation = 0.0;
	double shear = 0.0;
	if (operation == kVSPanelProjectionScale) {
		scaleU = settings.scaleU / 100.0;
		scaleV = settings.scaleV / 100.0;
	}
	else if (operation == kVSPanelProjectionRotate) {
		rotation = settings.rotation;
	}
	else {
		shear = settings.shear;
	}
	const VSProjectionLinearMatrix linear = VSMakeProjectionPlaneTransform(
		settings.leftAngle,
		settings.rightAngle,
		settings.plane,
		scaleU,
		scaleV,
		rotation,
		shear);
	const VSProjectionPoint reference = {
		(selectionBounds.left + selectionBounds.right) * 0.5,
		(selectionBounds.top + selectionBounds.bottom) * 0.5
	};
	const VSProjectionAffineMatrix transformedMatrix =
		VSMakeCenteredProjectionMatrix(linear, reference);
	AIRealMatrix matrix = {
		transformedMatrix.a,
		transformedMatrix.b,
		transformedMatrix.c,
		transformedMatrix.d,
		transformedMatrix.tx,
		transformedMatrix.ty
	};
	const AIReal lineScale = std::sqrt(
		std::abs(VSProjectionDeterminant(linear)));
	const ai::int32 flags =
		kTransformObjects |
		kTransformEntireArtStyle |
		kTransformChildren |
		kTransformLinkedMasks |
		kTransformNotifyPluginGroups;

	for (AIArtHandle source : roots) {
		AIArtHandle transformed = source;
		if (copy) {
			ASErr error = sAIArt->DuplicateArt(
				source,
				kPlaceAbove,
				source,
				&transformed);
			if (error) return error;
			error = sAIArt->SetArtUserAttr(
				source,
				kArtSelected | kArtFullySelected,
				0);
			if (error) return error;
			error = sAIArt->SetArtUserAttr(
				transformed,
				kArtSelected | kArtFullySelected,
				kArtSelected | kArtFullySelected);
			if (error) return error;
		}
		ASErr error = sAITransformArt->TransformArt(
			transformed,
			&matrix,
			lineScale,
			flags);
		if (error) return error;
	}
	return kNoErr;
}

ASErr VectorSuitePlugin::MeasureProjectionSelection()
{
	ProjectionSelection selection;
	if (selection.Error()) return selection.Error();
	AIRealRect selectionBounds = {};
	bool hasBounds = false;
	for (ai::int32 index = 0; index < selection.Count(); ++index) {
		AIArtHandle art = selection[index];
		if (!ProjectionIsFullySelectedRoot(art)) continue;
		AIRealRect bounds = {};
		ASErr error = sAIArt->GetArtBounds(art, &bounds);
		if (error) return error;
		if (!hasBounds) {
			selectionBounds = bounds;
			hasBounds = true;
		}
		else {
			sAIRealMath->AIRealRectUnion(
				&selectionBounds,
				&bounds,
				&selectionBounds);
		}
	}
	if (!hasBounds) {
		if (sAIUser) {
			sAIUser->MessageAlert(ai::UnicodeString(
				"Vector Suite: seleziona almeno un oggetto da misurare."));
		}
		return kNoErr;
	}

	const VSProjectionSettings settings = VSProjectionGet();
	const VSProjectionMeasurement measurement = VSMeasureProjectionRect(
		settings.leftAngle,
		settings.rightAngle,
		settings.plane,
		selectionBounds.left,
		selectionBounds.bottom,
		selectionBounds.right,
		selectionBounds.top);
	char message[256] = {};
	std::snprintf(
		message,
		sizeof(message),
		"Vector Suite — Misura sul piano\nU: %.2f pt\nV: %.2f pt\nDiagonale: %.2f pt",
		measurement.width,
		measurement.height,
		measurement.diagonal);
	if (sAIUser) sAIUser->MessageAlert(ai::UnicodeString(message));
	return kNoErr;
}

ASErr VectorSuitePlugin::CreateProjectionGuideGrid()
{
	AIRealRect bounds = {};
	ASErr error = sAIDocumentView->GetDocumentViewBounds(nullptr, &bounds);
	if (error) return error;
	AIArtHandle group = nullptr;
	error = sAIArt->NewArt(kGroupArt, kPlaceAboveAll, nullptr, &group);
	if (error) return error;
	error = sAIArt->SetArtName(group, ai::UnicodeString("Vector Suite · Griglia assonometrica"));
	if (error) return error;
	const AIRealPoint center = {(bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2};
	const AIReal extent = std::max<AIReal>(100, std::hypot(bounds.right - bounds.left, bounds.top - bounds.bottom));
	const AIReal step = std::max<AIReal>(20, std::ceil(extent / 100 / 20) * 20);
	const VSProjectionSettings settings = VSProjectionGet();
	for (int axis = 0; axis < kVSProjectionAxisCount; ++axis) {
		const VSProjectionPoint direction = VSMakeProjectionAxisVector(settings.leftAngle, settings.rightAngle, static_cast<VSProjectionAxis>(axis));
		const AIRealPoint normal = {-direction.y, direction.x};
		const int count = static_cast<int>(std::ceil(extent / step));
		for (int index = -count; index <= count; ++index) {
			AIArtHandle path = nullptr;
			error = sAIArt->NewArt(kPathArt, kPlaceInsideOnTop, group, &path);
			if (error) return error;
			AIPathSegment segments[2] = {};
			for (int end = 0; end < 2; ++end) {
				const AIReal distance = end ? extent : -extent;
				const AIRealPoint point = {center.h + normal.h * index * step + direction.x * distance, center.v + normal.v * index * step + direction.y * distance};
				segments[end].p = segments[end].in = segments[end].out = point;
				segments[end].corner = true;
			}
			error = sAIPath->SetPathSegmentCount(path, 2);
			if (!error) error = sAIPath->SetPathSegments(path, 0, 2, segments);
			if (!error) error = sAIPath->SetPathGuide(path, true);
			if (error) return error;
		}
	}
	return kNoErr;
}

ASErr VectorSuitePlugin::CreateProjectionArt(AIToolMessage* message)
{
	sAIUndo->UndoChanges();
	fEndPoint = message->cursor;
	SnapCursor(message, fEndPoint);

	ASErr error = InvalidateRect(oldAnnotatorRect);
	if (error)
		return error;

	const VSProjectionSettings projection = VSProjectionGet();
	const AIReal leftRadians = projection.leftAngle * kDegreesToRadians;
	const AIReal rightRadians = projection.rightAngle * kDegreesToRadians;
	const AIRealPoint axisX = {std::cos(rightRadians), std::sin(rightRadians)};
	const AIRealPoint axisZ = {-std::cos(leftRadians), std::sin(leftRadians)};
	const AIRealPoint axisY = {0, 1};

	if (message->tool == fToolHandle[kVSToolProjectionLine])
	{
		const AIRealPoint delta = {
			fEndPoint.h - fStartingPoint.h,
			fEndPoint.v - fStartingPoint.v
		};

		// Senza aggancio la linea segue il puntatore: serve per le quote e per
		// i raccordi che non stanno su nessuno dei tre assi.
		if (!projection.snapLine)
		{
			AIPathSegment free[2];
			SetCorner(free[0], fStartingPoint);
			SetCorner(free[1], fEndPoint);
			return CreatePath(free, 2, false);
		}

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

	AIRealPoint planeFirst, planeSecond;
	PlaneAxes(planeFirst, planeSecond);

	AIReal extentX = 0;
	AIReal extentZ = 0;
	ResolvePlane(fStartingPoint, fEndPoint, extentX, extentZ);
	if (message->event->modifiers & aiEventModifiers_shiftKey)
	{
		const AIReal extent = std::max(std::abs(extentX), std::abs(extentZ));
		extentX = std::copysign(extent, extentX == 0 ? 1 : extentX);
		extentZ = std::copysign(extent, extentZ == 0 ? 1 : extentZ);
	}

	const AIRealPoint projectedX = Scale(planeFirst, extentX);
	const AIRealPoint projectedZ = Scale(planeSecond, extentZ);

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
