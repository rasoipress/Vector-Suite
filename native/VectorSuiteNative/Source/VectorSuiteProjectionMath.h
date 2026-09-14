#ifndef VECTOR_SUITE_PROJECTION_MATH_H
#define VECTOR_SUITE_PROJECTION_MATH_H

#include <algorithm>
#include <cmath>

enum VSProjectionTarget {
	kVSProjectionTargetTopAlongRight = 0,
	kVSProjectionTargetTopAlongLeft,
	kVSProjectionTargetLeft,
	kVSProjectionTargetRight,
	kVSProjectionTargetCount
};

enum VSProjectionAxis {
	kVSProjectionAxisX = 0,
	kVSProjectionAxisZ,
	kVSProjectionAxisY,
	kVSProjectionAxisCount
};

struct VSProjectionPoint {
	double x;
	double y;
};

// x' = a*x + c*y, y' = b*x + d*y
struct VSProjectionLinearMatrix {
	double a;
	double b;
	double c;
	double d;
};

struct VSProjectionAffineMatrix {
	double a;
	double b;
	double c;
	double d;
	double tx;
	double ty;
};

struct VSProjectionMeasurement {
	double width;
	double height;
	double diagonal;
};

inline VSProjectionPoint VSMakeProjectionAxisVector(
	double leftAngle,
	double rightAngle,
	VSProjectionAxis axis)
{
	constexpr double kDegreesToRadians =
		0.017453292519943295769236907684886;
	if (axis == kVSProjectionAxisZ) {
		const double angle = leftAngle * kDegreesToRadians;
		return {-std::cos(angle), std::sin(angle)};
	}
	if (axis == kVSProjectionAxisY) return {0.0, 1.0};
	const double angle = rightAngle * kDegreesToRadians;
	return {std::cos(angle), std::sin(angle)};
}

inline VSProjectionLinearMatrix VSMakeProjectionLinearMatrix(
	double leftAngle,
	double rightAngle,
	VSProjectionTarget target)
{
	const VSProjectionPoint rightAxis = VSMakeProjectionAxisVector(
		leftAngle, rightAngle, kVSProjectionAxisX);
	const VSProjectionPoint leftAxis = VSMakeProjectionAxisVector(
		leftAngle, rightAngle, kVSProjectionAxisZ);
	const VSProjectionPoint verticalAxis = VSMakeProjectionAxisVector(
		leftAngle, rightAngle, kVSProjectionAxisY);

	VSProjectionPoint horizontal = rightAxis;
	VSProjectionPoint vertical = leftAxis;
	switch (target) {
		case kVSProjectionTargetTopAlongLeft:
			horizontal = leftAxis;
			vertical = rightAxis;
			break;
		case kVSProjectionTargetLeft:
			horizontal = leftAxis;
			vertical = verticalAxis;
			break;
		case kVSProjectionTargetRight:
			horizontal = rightAxis;
			vertical = verticalAxis;
			break;
		default:
			break;
	}

	return {
		horizontal.x,
		horizontal.y,
		vertical.x,
		vertical.y
	};
}

inline double VSProjectionDeterminant(const VSProjectionLinearMatrix& matrix)
{
	return matrix.a * matrix.d - matrix.b * matrix.c;
}

inline bool VSInvertProjectionLinearMatrix(
	const VSProjectionLinearMatrix& matrix,
	VSProjectionLinearMatrix* inverse)
{
	if (!inverse) return false;
	const double determinant = VSProjectionDeterminant(matrix);
	if (std::abs(determinant) < 1e-12) return false;
	*inverse = {
		matrix.d / determinant,
		-matrix.b / determinant,
		-matrix.c / determinant,
		matrix.a / determinant
	};
	return true;
}

inline VSProjectionAffineMatrix VSMakeCenteredProjectionMatrix(
	const VSProjectionLinearMatrix& linear,
	const VSProjectionPoint& reference)
{
	return {
		linear.a,
		linear.b,
		linear.c,
		linear.d,
		reference.x - linear.a * reference.x - linear.c * reference.y,
		reference.y - linear.b * reference.x - linear.d * reference.y
	};
}

inline VSProjectionPoint VSTransformProjectionPoint(
	const VSProjectionLinearMatrix& matrix,
	const VSProjectionPoint& point)
{
	return {
		matrix.a * point.x + matrix.c * point.y,
		matrix.b * point.x + matrix.d * point.y
	};
}

inline VSProjectionPoint VSTransformProjectionPoint(
	const VSProjectionAffineMatrix& matrix,
	const VSProjectionPoint& point)
{
	return {
		matrix.a * point.x + matrix.c * point.y + matrix.tx,
		matrix.b * point.x + matrix.d * point.y + matrix.ty
	};
}

// Base del piano di lavoro: U e V sono le colonne della matrice. I valori di
// plane corrispondono a VSProjectionPlane senza introdurre dipendenze dal
// pannello o dall'SDK Illustrator in questo header matematico testabile.
inline VSProjectionLinearMatrix VSMakeProjectionPlaneBasis(
	double leftAngle,
	double rightAngle,
	int plane)
{
	const VSProjectionPoint x = VSMakeProjectionAxisVector(
		leftAngle, rightAngle, kVSProjectionAxisX);
	const VSProjectionPoint z = VSMakeProjectionAxisVector(
		leftAngle, rightAngle, kVSProjectionAxisZ);
	const VSProjectionPoint y = VSMakeProjectionAxisVector(
		leftAngle, rightAngle, kVSProjectionAxisY);
	if (plane == 1) return {z.x, z.y, y.x, y.y};
	if (plane == 2) return {x.x, x.y, y.x, y.y};
	return {x.x, x.y, z.x, z.y};
}

// lhs * rhs: applica prima rhs e poi lhs.
inline VSProjectionLinearMatrix VSMultiplyProjectionLinearMatrices(
	const VSProjectionLinearMatrix& lhs,
	const VSProjectionLinearMatrix& rhs)
{
	return {
		lhs.a * rhs.a + lhs.c * rhs.b,
		lhs.b * rhs.a + lhs.d * rhs.b,
		lhs.a * rhs.c + lhs.c * rhs.d,
		lhs.b * rhs.c + lhs.d * rhs.d
	};
}

// Trasformazione espressa negli assi U/V del piano e riportata nelle
// coordinate dell'artwork: B * R * H * S * B^-1.
inline VSProjectionLinearMatrix VSMakeProjectionPlaneTransform(
	double leftAngle,
	double rightAngle,
	int plane,
	double scaleU,
	double scaleV,
	double rotationDegrees,
	double shearDegrees)
{
	const VSProjectionLinearMatrix basis = VSMakeProjectionPlaneBasis(
		leftAngle, rightAngle, plane);
	VSProjectionLinearMatrix inverseBasis = {};
	if (!VSInvertProjectionLinearMatrix(basis, &inverseBasis)) {
		return {1.0, 0.0, 0.0, 1.0};
	}
	constexpr double kDegreesToRadians =
		0.017453292519943295769236907684886;
	const double radians = rotationDegrees * kDegreesToRadians;
	const double shear = std::tan(shearDegrees * kDegreesToRadians);
	const VSProjectionLinearMatrix scale = {scaleU, 0.0, 0.0, scaleV};
	const VSProjectionLinearMatrix skew = {1.0, 0.0, shear, 1.0};
	const VSProjectionLinearMatrix rotate = {
		std::cos(radians),
		std::sin(radians),
		-std::sin(radians),
		std::cos(radians)
	};
	const VSProjectionLinearMatrix local = VSMultiplyProjectionLinearMatrices(
		rotate,
		VSMultiplyProjectionLinearMatrices(skew, scale));
	return VSMultiplyProjectionLinearMatrices(
		basis,
		VSMultiplyProjectionLinearMatrices(local, inverseBasis));
}

inline VSProjectionMeasurement VSMeasureProjectionRect(
	double leftAngle,
	double rightAngle,
	int plane,
	double firstX,
	double firstY,
	double secondX,
	double secondY)
{
	const VSProjectionLinearMatrix basis = VSMakeProjectionPlaneBasis(
		leftAngle, rightAngle, plane);
	VSProjectionLinearMatrix inverse = {};
	if (!VSInvertProjectionLinearMatrix(basis, &inverse)) {
		return {0.0, 0.0, 0.0};
	}
	const double minimumX = std::min(firstX, secondX);
	const double maximumX = std::max(firstX, secondX);
	const double minimumY = std::min(firstY, secondY);
	const double maximumY = std::max(firstY, secondY);
	const VSProjectionPoint corners[] = {
		{minimumX, minimumY},
		{maximumX, minimumY},
		{maximumX, maximumY},
		{minimumX, maximumY}
	};
	VSProjectionPoint local = VSTransformProjectionPoint(inverse, corners[0]);
	double localMinX = local.x;
	double localMaxX = local.x;
	double localMinY = local.y;
	double localMaxY = local.y;
	for (int index = 1; index < 4; ++index) {
		local = VSTransformProjectionPoint(inverse, corners[index]);
		localMinX = std::min(localMinX, local.x);
		localMaxX = std::max(localMaxX, local.x);
		localMinY = std::min(localMinY, local.y);
		localMaxY = std::max(localMaxY, local.y);
	}
	const double width = localMaxX - localMinX;
	const double height = localMaxY - localMinY;
	return {width, height, std::sqrt(width * width + height * height)};
}

#endif
