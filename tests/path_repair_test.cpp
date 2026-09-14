#include "../native/VectorSuiteNative/Source/VectorSuitePathRepair.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void ExpectIndices(
	const std::vector<size_t>& actual,
	const std::vector<size_t>& expected,
	const char* message)
{
	if (actual == expected) return;
	std::cerr << message << ": expected";
	for (size_t value : expected) std::cerr << ' ' << value;
	std::cerr << ", received";
	for (size_t value : actual) std::cerr << ' ' << value;
	std::cerr << '\n';
	std::exit(EXIT_FAILURE);
}

void TestOpenPathRemovesDuplicatesAndCollinearAnchors()
{
	const std::vector<VSRepairAnchor> anchors = {
		{0.0, 0.0},
		{0.01, 0.0},
		{5.0, 0.0},
		{10.0, 0.0}
	};
	ExpectIndices(
		VSComputeRepairKeepIndices(anchors, {}, false, 0.05, true, true),
		{0, 3},
		"open repair");
}

void TestClosedPathRemovesRepeatedClosingAnchor()
{
	const std::vector<VSRepairAnchor> anchors = {
		{0.0, 0.0},
		{10.0, 0.0},
		{10.0, 10.0},
		{0.0, 10.0},
		{0.0, 0.01}
	};
	ExpectIndices(
		VSComputeRepairKeepIndices(anchors, {}, true, 0.05, true, false),
		{0, 1, 2, 3},
		"closed duplicate repair");
}

void TestCurvedAnchorIsProtected()
{
	const std::vector<VSRepairAnchor> anchors = {
		{0.0, 0.0},
		{5.0, 0.0},
		{10.0, 0.0}
	};
	const std::vector<bool> protectedAnchors = {false, true, false};
	ExpectIndices(
		VSComputeRepairKeepIndices(
			anchors, protectedAnchors, false, 0.05, true, true),
		{0, 1, 2},
		"curved anchor protection");
}

void TestClosedPathNeverFallsBelowThreeAnchors()
{
	const std::vector<VSRepairAnchor> anchors = {
		{0.0, 0.0},
		{5.0, 0.0},
		{10.0, 0.0}
	};
	ExpectIndices(
		VSComputeRepairKeepIndices(anchors, {}, true, 0.05, true, true),
		{0, 1, 2},
		"closed minimum");
}

} // namespace

int main()
{
	TestOpenPathRemovesDuplicatesAndCollinearAnchors();
	TestClosedPathRemovesRepeatedClosingAnchor();
	TestCurvedAnchorIsProtected();
	TestClosedPathNeverFallsBelowThreeAnchors();
	std::cout << "Path repair: duplicates, collinear anchors and safeguards verified.\n";
	return EXIT_SUCCESS;
}
