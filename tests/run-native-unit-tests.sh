#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
PROJECTION_BINARY=$(mktemp "${TMPDIR:-/tmp}/vector-suite-projection-tests.XXXXXX")
REPAIR_BINARY=$(mktemp "${TMPDIR:-/tmp}/vector-suite-repair-tests.XXXXXX")
SMART_FIND_BINARY=$(mktemp "${TMPDIR:-/tmp}/vector-suite-smart-find-tests.XXXXXX")
TRANSFORM_BINARY=$(mktemp "${TMPDIR:-/tmp}/vector-suite-transform-tests.XXXXXX")
STYLE_BINARY=$(mktemp "${TMPDIR:-/tmp}/vector-suite-style-tests.XXXXXX")
WORKFLOW_BINARY=$(mktemp "${TMPDIR:-/tmp}/vector-suite-workflow-tests.XXXXXX")
DRAWING_BINARY=$(mktemp "${TMPDIR:-/tmp}/vector-suite-drawing-tests.XXXXXX")
SELECTION_BINARY=$(mktemp "${TMPDIR:-/tmp}/vector-suite-selection-tests.XXXXXX")
SNAP_BINARY=$(mktemp "${TMPDIR:-/tmp}/vector-suite-snap-tests.XXXXXX")
trap 'rm -f "$PROJECTION_BINARY" "$REPAIR_BINARY" "$SMART_FIND_BINARY" "$TRANSFORM_BINARY" "$STYLE_BINARY" "$WORKFLOW_BINARY" "$DRAWING_BINARY" "$SELECTION_BINARY" "$SNAP_BINARY"' EXIT HUP INT TERM

clang++ -std=c++17 -Wall -Wextra -Werror "$PROJECT_ROOT/tests/snap_math_test.cpp" -o "$SNAP_BINARY"
"$SNAP_BINARY"

clang++ \
  -std=c++17 \
  -Wall \
	-Wextra \
	-Werror \
	"$PROJECT_ROOT/tests/projection_math_test.cpp" \
	-o "$PROJECTION_BINARY"

clang++ \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  "$PROJECT_ROOT/tests/path_repair_test.cpp" \
  -o "$REPAIR_BINARY"

clang++ \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  "$PROJECT_ROOT/tests/smart_find_test.cpp" \
  -o "$SMART_FIND_BINARY"

clang++ \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  "$PROJECT_ROOT/tests/transform_math_test.cpp" \
  -o "$TRANSFORM_BINARY"

clang++ \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  "$PROJECT_ROOT/tests/style_math_test.cpp" \
  -o "$STYLE_BINARY"

clang++ \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  "$PROJECT_ROOT/tests/workflow_test.cpp" \
  -o "$WORKFLOW_BINARY"

clang++ \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
	"$PROJECT_ROOT/tests/drawing_math_test.cpp" \
	-o "$DRAWING_BINARY"

clang++ \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
	"$PROJECT_ROOT/tests/selection_logic_test.cpp" \
	-o "$SELECTION_BINARY"

"$PROJECTION_BINARY"
"$REPAIR_BINARY"
"$SMART_FIND_BINARY"
"$TRANSFORM_BINARY"
"$STYLE_BINARY"
"$WORKFLOW_BINARY"
"$DRAWING_BINARY"
"$SELECTION_BINARY"
