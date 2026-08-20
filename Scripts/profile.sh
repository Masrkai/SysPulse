#!/usr/bin/env bash

# Profiling script for SysPulse
# Generates 3 focused flame graphs: CPU, Memory, and Function Call patterns

set -e

PROJECT_NAME="SysPulse_profiling"
PROFILING_DIR="profiling"
RESULTS_DIR="profiling/perf_results"
FLAMEGRAPH_DIR="flamegraphs"
EXECUTABLE="$PROFILING_DIR/$PROJECT_NAME"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== SysPulse Profiling Suite ===${NC}"

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
    echo -e "${RED}Error: Executable not found at $EXECUTABLE${NC}"
    echo -e "${YELLOW}Run 'make build-profiling' first${NC}"
    exit 1
fi

# Create results and flamegraph directories
mkdir -p "$RESULTS_DIR"
mkdir -p "$FLAMEGRAPH_DIR"
cd "$RESULTS_DIR"

# Check for required tools
command -v perf >/dev/null 2>&1 || { echo -e "${RED}Error: perf not installed${NC}"; exit 1; }
command -v flamegraph.pl >/dev/null 2>&1 || { echo -e "${RED}Error: flamegraph.pl not found in PATH${NC}"; exit 1; }
command -v stackcollapse-perf.pl >/dev/null 2>&1 || { echo -e "${RED}Error: stackcollapse-perf.pl not found in PATH${NC}"; exit 1; }

echo -e "${GREEN}Starting profiling session...${NC}"

# 1. CPU Performance Profiling
echo -e "${YELLOW}[1/3] CPU Performance Analysis${NC}"
echo "Recording CPU events with call stacks..."

# Reduced frequency and using frame pointers for reliability
perf record -F 499 -g --call-graph fp --mmap-pages=128 -o cpu_profile.data ../../"$EXECUTABLE" "$@"

# Check if recording was successful
if [ $? -ne 0 ]; then
    echo -e "${RED}CPU profiling failed, trying fallback method...${NC}"
    perf record -F 99 -g -o cpu_profile.data ../../"$EXECUTABLE" "$@"
fi

# Generate folded stack traces for flamegraph
perf script -i cpu_profile.data | stackcollapse-perf.pl > cpu_profile.folded
flamegraph.pl cpu_profile.folded > cpu_flamegraph.svg

# Verify output
if [ -s cpu_flamegraph.svg ]; then
    echo -e "${GREEN}✓ CPU flame graph generated: cpu_flamegraph.svg${NC}"
else
    echo -e "${RED}✗ CPU flame graph generation failed${NC}"
fi

# 2. Memory Allocation Profiling  
echo -e "${YELLOW}[2/3] Memory Allocation Analysis${NC}"
echo "Recording memory allocation events..."

# Record memory events with lower frequency to reduce overhead
perf record -e cache-misses,cache-references,page-faults -F 199 \
    -g --call-graph fp --mmap-pages=128 -o memory_profile.data ../../"$EXECUTABLE" "$@"

# Check if recording was successful  
if [ $? -ne 0 ]; then
    echo -e "${YELLOW}Full memory profiling failed, trying cache-only...${NC}"
    perf record -e cache-misses,cache-references -F 99 \
        -g --call-graph fp -o memory_profile.data ../../"$EXECUTABLE" "$@"
fi

perf script -i memory_profile.data | stackcollapse-perf.pl > memory_profile.folded
flamegraph.pl --title="Memory Events" --colors=mem memory_profile.folded > memory_flamegraph.svg

if [ -s memory_flamegraph.svg ]; then
    echo -e "${GREEN}✓ Memory flame graph generated: memory_flamegraph.svg${NC}"
else
    echo -e "${RED}✗ Memory flame graph generation failed${NC}"
fi

# 3. Function Call Frequency Analysis
echo -e "${YELLOW}[3/3] Function Call Frequency Analysis${NC}"
echo "Recording high-frequency samples for call patterns..."

# Moderate frequency sampling for detailed function call analysis
perf record -F 999 -g --call-graph fp --mmap-pages=128 -o calls_profile.data ../../"$EXECUTABLE" "$@"

# Check if recording was successful
if [ $? -ne 0 ]; then
    echo -e "${YELLOW}High-frequency profiling failed, using lower frequency...${NC}"
    perf record -F 299 -g --call-graph fp -o calls_profile.data ../../"$EXECUTABLE" "$@"
fi

perf script -i calls_profile.data | stackcollapse-perf.pl > calls_profile.folded
flamegraph.pl --title="Function Call Frequency" --width=1400 --colors=hot calls_profile.folded > calls_flamegraph.svg

if [ -s calls_flamegraph.svg ]; then
    echo -e "${GREEN}✓ Function calls flame graph generated: calls_flamegraph.svg${NC}"
else
    echo -e "${RED}✗ Function calls flame graph generation failed${NC}"
fi

# Copy SVG files to flamegraph directory
echo -e "${YELLOW}Copying flame graphs to flamegraphs directory...${NC}"
cd ../../  # Go back to project root

# Copy all generated SVG files to flamegraph directory
for svg_file in "$RESULTS_DIR"/*.svg; do
    if [ -f "$svg_file" ]; then
        cp "$svg_file" "$FLAMEGRAPH_DIR"/
        echo -e "${GREEN}✓ Copied $(basename "$svg_file") to flamegraphs/${NC}"
    fi
done

# Go back to results directory for the rest of the script
cd "$RESULTS_DIR"

# Generate summary report
echo -e "${BLUE}=== Profiling Complete ===${NC}"
echo -e "${GREEN}Results saved in: $(pwd)${NC}"
echo -e "${GREEN}Flame graphs copied to: $(pwd)/../../flamegraphs${NC}"
echo ""
echo -e "${YELLOW}Generated Files:${NC}"
echo "  • cpu_flamegraph.svg     - CPU hotspots and execution time"
echo "  • memory_flamegraph.svg  - Memory access patterns and cache performance"  
echo "  • calls_flamegraph.svg   - Function call frequency and relationships"
echo ""
echo -e "${YELLOW}Raw perf data files:${NC}"
echo "  • cpu_profile.data       - Raw CPU profiling data"
echo "  • memory_profile.data    - Raw memory profiling data"
echo "  • calls_profile.data     - Raw call profiling data"
echo ""
echo -e "${BLUE}To view flame graphs, open the .svg files in the flamegraphs/ directory${NC}"

# Optional: Generate perf report summaries
echo ""
echo -e "${YELLOW}Generating text reports...${NC}"
perf report -i cpu_profile.data --stdio > cpu_report.txt
perf report -i memory_profile.data --stdio > memory_report.txt
perf report -i calls_profile.data --stdio > calls_report.txt
echo -e "${GREEN}✓ Text reports generated${NC}"

echo ""
echo -e "${GREEN}Profiling session completed successfully!${NC}"
echo -e "${BLUE}Flame graphs are available in both:${NC}"
echo -e "  • ${RESULTS_DIR}/ (original location)"
echo -e "  • flamegraphs/ (convenient access)"