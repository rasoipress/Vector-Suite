#include "../native/VectorSuiteNative/Source/VectorSuiteProjectionMath.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

constexpr double kTolerance = 1e-9;

void ExpectNear(double actual, double expected, const char* message)
{
	if (std::abs(actual - expected) <= kTolerance) return;
	std::cerr << message << ": expected " << expected << ", received "
		<< actual << '\n';
	std::exit(EXIT_FAILURE);
}

void Expect(bool condition, const char* message)
{
	if (condition) return;
	std::cerr << message << '\n';
	std::exit(EXIT_FAILURE);
}

void TestIsometricAxes()
{
	const VSProjectionLinearMatrix right =
		VSMakeProjectionLinearMatrix(30.0, 30.0, kVSProjectionTargetRight);
	ExpectNear(right.a, std::sqrt(3.0) * 0.5, "right plane x axis");
	ExpectNear(right.b, 0.5, "right plane x rise");
	ExpectNear(right.c, 0.0, "right plane vertical x");
	ExpectNear(right.d, 1.0, "right plane vertical y");

	const VSProjectionLinearMatrix left =
		VSMakeProjectionLinearMatrix(30.0, 30.0, kVSProjectionTargetLeft);
	ExpectNear(left.a, -std::sqrt(3.0) * 0.5, "left plane x axis");
	ExpectNear(left.b, 0.5, "left plane x rise");
	ExpectNear(left.c, 0.0, "left plane vertical x");
	ExpectNear(left.d, 1.0, "left plane vertical y");
}

void TestProjectionAxisVectors()
{
	const VSProjectionPoint x =
		VSMakeProjectionAxisVector(30.0, 30.0, kVSProjectionAxisX);
	const VSProjectionPoint z =
		VSMakeProjectionAxisVector(30.0, 30.0, kVSProjectionAxisZ);
	const VSProjectionPoint y =
		VSMakeProjectionAxisVector(30.0, 30.0, kVSProjectionAxisY);
	ExpectNear(x.x, std::sqrt(3.0) * 0.5, "axis X horizontal");
	ExpectNear(x.y, 0.5, "axis X vertical");
	ExpectNear(z.x, -std::sqrt(3.0) * 0.5, "axis Z horizontal");
	ExpectNear(z.y, 0.5, "axis Z vertical");
	ExpectNear(y.x, 0.0, "axis Y horizontal");
	ExpectNear(y.y, 1.0, "axis Y vertical");
}

void TestRoundTripEveryPlane()
{
	for (int target = 0; target < kVSProjectionTargetCount; ++target) {
		const VSProjectionLinearMatrix projected = VSMakeProjectionLinearMatrix(
			17.0,
			41.0,
			static_cast<VSProjectionTarget>(target));
		VSProjectionLinearMatrix inverse = {};
		Expect(VSInvertProjectionLinearMatrix(projected, &inverse),
			"projection matrix must be invertible");

		const VSProjectionPoint input = {13.25, -8.5};
		const VSProjectionPoint intermediate =
			VSTransformProjectionPoint(projected, input);
		const VSProjectionPoint result =
			VSTransformProjectionPoint(inverse, intermediate);
		ExpectNear(result.x, input.x, "round-trip x");
		ExpectNear(result.y, input.y, "round-trip y");
	}
}

void TestReferencePointRemainsFixed()
{
	const VSProjectionLinearMatrix linear =
		VSMakeProjectionLinearMatrix(30.0, 30.0, kVSProjectionTargetTopAlongRight);
	const VSProjectionAffineMatrix affine =
		VSMakeCenteredProjectionMatrix(linear, {120.0, 75.0});
	const VSProjectionPoint anchor =
		VSTransformProjectionPoint(affine, {120.0, 75.0});
	ExpectNear(anchor.x, 120.0, "reference x");
	ExpectNear(anchor.y, 75.0, "reference y");
}

void TestCenteredRoundTripWithStoredReference()
{
	VSProjectionLinearMatrix projected =
		VSMakeProjectionLinearMatrix(7.0, 42.0, kVSProjectionTargetTopAlongLeft);
	VSProjectionLinearMatrix unprojected = {};
	Expect(VSInvertProjectionLinearMatrix(projected, &unprojected),
		"stored projection must be invertible");
	const VSProjectionPoint reference = {245.0, -31.0};
	const VSProjectionAffineMatrix project =
		VSMakeCenteredProjectionMatrix(projected, reference);
	const VSProjectionAffineMatrix unproject =
		VSMakeCenteredProjectionMatrix(unprojected, reference);
	const VSProjectionPoint input = {311.5, 82.25};
	const VSProjectionPoint intermediate =
		VSTransformProjectionPoint(project, input);
	const VSProjectionPoint result =
		VSTransformProjectionPoint(unproject, intermediate);
	ExpectNear(result.x, input.x, "stored-reference round-trip x");
	ExpectNear(result.y, input.y, "stored-reference round-trip y");
}

void TestPlaneScaleUsesLocalAxes()
{
	const VSProjectionLinearMatrix basis =
		VSMakeProjectionPlaneBasis(30.0, 30.0, 0);
	const VSProjectionPoint sourceLocal = {10.0, 5.0};
	const VSProjectionPoint sourceArtwork =
		VSTransformProjectionPoint(basis, sourceLocal);
	const VSProjectionLinearMatrix transform = VSMakeProjectionPlaneTransform(
		30.0, 30.0, 0, 2.0, 0.5, 0.0, 0.0);
	const VSProjectionPoint transformedArtwork =
		VSTransformProjectionPoint(transform, sourceArtwork);
	VSProjectionLinearMatrix inverseBasis = {};
	Expect(VSInvertProjectionLinearMatrix(basis, &inverseBasis),
		"top plane basis must be invertible");
	const VSProjectionPoint transformedLocal =
		VSTransformProjectionPoint(inverseBasis, transformedArtwork);
	ExpectNear(transformedLocal.x, 20.0, "local U scale");
	ExpectNear(transformedLocal.y, 2.5, "local V scale");
}

void TestPlaneRotateAndShear()
{
	const VSProjectionLinearMatrix rotate = VSMakeProjectionPlaneTransform(
		30.0, 30.0, 2, 1.0, 1.0, 90.0, 0.0);
	const VSProjectionLinearMatrix shear = VSMakeProjectionPlaneTransform(
		30.0, 30.0, 2, 1.0, 1.0, 0.0, 45.0);
	const VSProjectionLinearMatrix basis =
		VSMakeProjectionPlaneBasis(30.0, 30.0, 2);
	VSProjectionLinearMatrix inverseBasis = {};
	Expect(VSInvertProjectionLinearMatrix(basis, &inverseBasis),
		"right plane basis must be invertible");

	const VSProjectionPoint sourceArtwork =
		VSTransformProjectionPoint(basis, {2.0, 3.0});
	const VSProjectionPoint rotatedLocal = VSTransformProjectionPoint(
		inverseBasis,
		VSTransformProjectionPoint(rotate, sourceArtwork));
	ExpectNear(rotatedLocal.x, -3.0, "local rotation U");
	ExpectNear(rotatedLocal.y, 2.0, "local rotation V");

	const VSProjectionPoint shearedLocal = VSTransformProjectionPoint(
		inverseBasis,
		VSTransformProjectionPoint(shear, sourceArtwork));
	ExpectNear(shearedLocal.x, 5.0, "local shear U");
	ExpectNear(shearedLocal.y, 3.0, "local shear V");
}

void TestPlaneMeasurement()
{
	const VSProjectionMeasurement measurement = VSMeasureProjectionRect(
		0.0, 0.0, 2, -10.0, -5.0, 30.0, 15.0);
	ExpectNear(measurement.width, 40.0, "measurement width");
	ExpectNear(measurement.height, 20.0, "measurement height");
	ExpectNear(measurement.diagonal, std::sqrt(2000.0), "measurement diagonal");
}

} // namespace

int main()
{
	TestIsometricAxes();
	TestProjectionAxisVectors();
	TestRoundTripEveryPlane();
	TestReferencePointRemainsFixed();
	TestCenteredRoundTripWithStoredReference();
	TestPlaneScaleUsesLocalAxes();
	TestPlaneRotateAndShear();
	TestPlaneMeasurement();
	std::cout << "Projection math: projections, plane transforms and measurement verified.\n";
	return EXIT_SUCCESS;
}
