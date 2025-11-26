#!/bin/bash
set -e

# Directory setup
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/.."
BUILD_DIR="$PROJECT_ROOT/build/tests"

# 1. Build the test executable
echo "Building LfoTests..."
mkdir -p "$BUILD_DIR"
cmake -S "$PROJECT_ROOT" -B "$PROJECT_ROOT/build" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$PROJECT_ROOT/build" --target LfoTests --parallel

# 2. Run the tests (generates CSV files)
echo "Running LfoTests..."
# Check if executable exists where we expect it
if [ -f "$PROJECT_ROOT/build/tests/LfoTests" ]; then
    EXE_PATH="$PROJECT_ROOT/build/tests/LfoTests"
elif [ -f "$PROJECT_ROOT/build/tests/Debug/LfoTests" ]; then
    EXE_PATH="$PROJECT_ROOT/build/tests/Debug/LfoTests"
elif [ -f "$PROJECT_ROOT/build/bin/LfoTests" ]; then
    EXE_PATH="$PROJECT_ROOT/build/bin/LfoTests"
else
    echo "Could not find LfoTests executable!"
    exit 1
fi

# Run from project root so relative paths to tests/output work
cd "$PROJECT_ROOT"
"$EXE_PATH"

# 3. Plot the results and generate report
echo "Generating report..."
if command -v python3 &> /dev/null; then
    PYTHON_CMD=python3
else
    PYTHON_CMD=python
fi

$PYTHON_CMD tests/scripts/plot_lfo_tests.py

echo "Done! Report is available in tests/report/index.html"
# Open report on macOS
if [[ "$OSTYPE" == "darwin"* ]]; then
    open tests/report/index.html
fi
