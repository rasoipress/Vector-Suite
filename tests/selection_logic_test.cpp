#include "../native/VectorSuiteNative/Source/VectorSuiteSelection.h"

#include <cassert>
#include <iostream>

int main()
{
	assert(VSIsWholeObjectSelectionRoot(true, false));
	assert(!VSIsWholeObjectSelectionRoot(false, false));
	assert(!VSIsWholeObjectSelectionRoot(true, true));
	assert(!VSIsWholeObjectSelectionRoot(false, true));
	std::cout << "selection logic tests passed\n";
	return 0;
}
