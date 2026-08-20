#!/usr/bin/env bash

# Ultimate Profiling script for SysPulse
# Generates comprehensive flame graphs and reports across 5 phases
# Fully unattended: automatically bypasses the "Press Enter" prompt

set -e

PROJECT_NAME="SysPulse_profiling"
PROFILING_DIR="build"
RESULTS_DIR="profiling/perf_results"
FLAMEGRAPH_DIR="flamegraphs"
EXECUTABLE="$PROFILING_DIR/$PROJECT_NAME"

# Colors
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'

echo -e "${BLUE}=== SysPulse Ultimate Profiling Suite (Unattended) ===${NC}"

if [ ! -f "$EXECUTABLE" ]; then
    echo -e "${RED}Error: Executable not found at $EXECUTABLE${NC}"
    exit 1
fi

mkdir -p "$RESULTS_DIR" "$FLAMEGRAPH_DIR"
cd "$RESULTS_DIR"

command -v perf >/dev/null 2>&1 || { echo -e "${RED}Error: perf not installed${NC}"; exit 1; }
command -v flamegraph.pl >/dev/null 2>&1 || { echo -e "${RED}Error: flamegraph.pl not found${NC}"; exit 1; }
command -v stackcollapse-perf.pl >/dev/null 2>&1 || { echo -e "${RED}Error: stackcollapse-perf.pl not found${NC}"; exit 1; }

# Helper function to safely run perf and automatically bypass the "Press Enter" prompt.
# The (sleep 2; echo) subshell waits 2 seconds for the app to start, then sends a newline to stdin.
safe_record() {
    if ! (sleep 2; echo) | perf record "$@" >/dev/null 2>&1; then
        echo -e "${YELLOW}⚠ Event not supported on this CPU/VM. Skipping phase.${NC}"
        return 1
    fi
    return 0
}

# ==========================================
# PHASE 1: CORE PROFILE (Single Run)
# ==========================================
echo -e "\n${BLUE}--- Phase 1: Core Profile (CPU, Cache, Branches) ---${NC}"
if safe_record -e cycles,cache-misses,branch-misses -F 499 -g --call-graph fp --mmap-pages=128 -o core.data ../../"$EXECUTABLE" "$@"; then
    perf script -i core.data | awk '
        /^[^ ]/ && /cpu-cycles/ { f="cpu.txt" }
        /^[^ ]/ && /cycles/ && !/cpu-cycles/ { f="cpu.txt" }
        /^[^ ]/ && /cache-misses/ { f="mem.txt" }
        /^[^ ]/ && /branch-misses/ { f="branch.txt" }
        { if (f) print >> f }
    '
    stackcollapse-perf.pl < cpu.txt | flamegraph.pl --title="CPU Cycles" --colors=hot > cpu_flamegraph.svg
    stackcollapse-perf.pl < mem.txt | flamegraph.pl --title="Cache Misses" --colors=mem > mem_flamegraph.svg
    stackcollapse-perf.pl < branch.txt | flamegraph.pl --title="Branch Mispredictions" --colors=red > branch_flamegraph.svg
    echo -e "${GREEN}✓ Phase 1 Complete${NC}"
fi

# ==========================================
# PHASE 2: DEEP HARDWARE (IPC, TLB, L1 Cache)
# ==========================================
echo -e "\n${BLUE}--- Phase 2: Deep Hardware (IPC, TLB, L1) ---${NC}"
if safe_record -e instructions,cycles,dTLB-load-misses,L1-dcache-load-misses -F 499 -g --call-graph fp --mmap-pages=128 -o deep.data ../../"$EXECUTABLE" "$@"; then
    perf script -i deep.data | awk '
        /^[^ ]/ && /instructions/ { f="inst.txt" }
        /^[^ ]/ && /cpu-cycles/ { f="cyc.txt" }
        /^[^ ]/ && /cycles/ && !/cpu-cycles/ { f="cyc.txt" }
        /^[^ ]/ && /dTLB-load-misses/ { f="tlb.txt" }
        /^[^ ]/ && /L1-dcache-load-misses/ { f="l1.txt" }
        { if (f) print >> f }
    '
    
    stackcollapse-perf.pl < inst.txt > inst.folded
    stackcollapse-perf.pl < cyc.txt > cyc.folded
    flamegraph.pl --title="Instructions Executed" --colors=blue inst.folded > ipc_inst_flamegraph.svg
    flamegraph.pl --title="CPU Cycles (for IPC comparison)" --colors=hot cyc.folded > ipc_cyc_flamegraph.svg
    
    [ -s tlb.txt ] && stackcollapse-perf.pl < tlb.txt | flamegraph.pl --title="TLB Load Misses" --colors=orange > tlb_flamegraph.svg
    [ -s l1.txt ] && stackcollapse-perf.pl < l1.txt | flamegraph.pl --title="L1 Data Cache Misses" --colors=mem > l1_flamegraph.svg
    echo -e "${GREEN}✓ Phase 2 Complete${NC}"
fi

# ==========================================
# PHASE 3: CONCURRENCY / OFF-CPU
# ==========================================
echo -e "\n${BLUE}--- Phase 3: Concurrency (Context Switches) ---${NC}"
if safe_record -e context-switches -F 499 -g --call-graph fp --mmap-pages=128 -o offcpu.data ../../"$EXECUTABLE" "$@"; then
    perf script -i offcpu.data | awk '/^[^ ]/ && /context-switches/ { f="ctx.txt" } { if (f) print >> f }'
    if [ -s ctx.txt ]; then
        stackcollapse-perf.pl < ctx.txt | flamegraph.pl --title="Context Switches (Off-CPU)" --colors=green > offcpu_flamegraph.svg
    fi
    echo -e "${GREEN}✓ Phase 3 Complete${NC}"
fi

# ==========================================
# PHASE 4: MEMORY TOPOLOGY (NUMA)
# ==========================================
echo -e "\n${BLUE}--- Phase 4: Memory Topology (NUMA/Latency) ---${NC}"
# Applied the auto-enter trick here as well
if ! (sleep 2; echo) | perf mem record -o numa.data -- ../../"$EXECUTABLE" "$@" >/dev/null 2>&1; then
    echo -e "${YELLOW}⚠ perf mem not supported. Skipping phase.${NC}"
else
    perf mem report -i numa.data --stdio --sort=local_weight,mem,sym,dso > numa_report.txt
    echo -e "${GREEN}✓ Phase 4 Complete (See numa_report.txt)${NC}"
fi

# ==========================================
# PHASE 5: SIMD & VECTORIZATION SUMMARY
# ==========================================
echo -e "\n${BLUE}--- Phase 5: SIMD / Vectorization Summary ---${NC}"
echo "Running perf stat to check for AVX/SIMD utilization..."
# Applied the auto-enter trick here as well
(sleep 2; echo) | perf stat -e instructions,fp_arith_inst_retired.256b_packed_double,fp_arith_inst_retired.128b_packed_double,fp_arith_inst_retired.scalar_double \
    -o simd_summary.txt -- ../../"$EXECUTABLE" "$@" >/dev/null 2>&1 || \
echo -e "${YELLOW}⚠ Specific SIMD counters not supported on this CPU. Check simd_summary.txt for fallback.${NC}"
echo -e "${GREEN}✓ Phase 5 Complete${NC}"

# ==========================================
# FINALIZE
# ==========================================
echo -e "\n${BLUE}=== Finalizing and Copying Files ===${NC}"
cd ../../ 

# Copy all generated SVGs and reports
for file in "$RESULTS_DIR"/*.svg "$RESULTS_DIR"/*_report.txt "$RESULTS_DIR"/simd_summary.txt; do
    if [ -f "$file" ]; then
        cp "$file" "$FLAMEGRAPH_DIR"/
    fi
done

echo -e "${GREEN}All files copied to: $(pwd)/flamegraphs${NC}"
echo -e "\n${BLUE}=== Profiling Suite Complete ===${NC}"
echo -e "${YELLOW}Generated Insights:${NC}"
echo "  • Core:       cpu_flamegraph.svg, mem_flamegraph.svg, branch_flamegraph.svg"
echo "  • Deep HW:    ipc_inst_flamegraph.svg, tlb_flamegraph.svg, l1_flamegraph.svg"
echo "  • Off-CPU:    offcpu_flamegraph.svg (Context switches/waiting)"
echo "  • Topology:   numa_report.txt (Local vs Remote memory access)"
echo "  • SIMD:       simd_summary.txt (Vectorized instruction counts)"
echo -e "${GREEN}Done! You can now walk away.${NC}"