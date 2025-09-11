#!/bin/bash

# Profiling helper script for scaler benchmarks

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../build"
BENCHMARK_BUILD_DIR="${BUILD_DIR}/benchmark"
BENCHMARK_EXEC="${BUILD_DIR}/bin/benchmark_scalers"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

function print_usage {
    echo "Usage: $0 [command] [options]"
    echo ""
    echo "Commands:"
    echo "  gprof              Run gprof profiling"
    echo "  cachegrind         Run valgrind cachegrind (cache profiling)"
    echo "  callgrind          Run valgrind callgrind (call graph profiling)"
    echo "  memcheck           Run valgrind memcheck (memory leak detection)"
    echo "  massif             Run valgrind massif (heap profiling)"
    echo "  perf-stat          Run perf stat (performance counters)"
    echo "  perf-record        Run perf record (sampling profiler)"
    echo "  all                Run all profiling tools"
    echo "  clean              Clean profiling outputs"
    echo ""
    echo "Options:"
    echo "  --quick            Use quick benchmark mode"
    echo "  --verbose          Verbose output"
    echo "  --filter ALGO      Profile only specified algorithm (e.g., HQ3x)"
    echo "  --help             Show this help"
}

function check_tool {
    if ! command -v $1 &> /dev/null; then
        echo -e "${RED}Error: $1 is not installed${NC}"
        echo "Please install $1 to use this profiling option"
        return 1
    fi
    return 0
}

function build_benchmark {
    echo -e "${GREEN}Building benchmark with profiling support...${NC}"
    
    local CMAKE_FLAGS=""
    if [[ "$1" == "gprof" ]]; then
        CMAKE_FLAGS="-DSCALER_ENABLE_PROFILING=ON"
    elif [[ "$1" == "valgrind" ]]; then
        CMAKE_FLAGS="-DSCALER_ENABLE_VALGRIND=ON"
    fi
    
    cmake -B "${BUILD_DIR}" -DSCALER_BUILD_TEST=ON ${CMAKE_FLAGS} ..
    cmake --build "${BUILD_DIR}" --target benchmark_scalers
}

function run_gprof {
    echo -e "${YELLOW}Running gprof profiling...${NC}"
    check_tool gprof || return 1
    
    build_benchmark gprof
    
    cd "${BENCHMARK_BUILD_DIR}"
    rm -f gmon.out
    
    echo "Running benchmark..."
    ${BENCHMARK_EXEC} $BENCH_ARGS
    
    echo "Generating report..."
    gprof ${BENCHMARK_EXEC} gmon.out > gprof_report.txt
    
    echo -e "${GREEN}Report saved to ${BENCHMARK_BUILD_DIR}/gprof_report.txt${NC}"
    echo ""
    echo "Top functions by time:"
    head -n 30 gprof_report.txt | grep -A 20 "time   seconds"
}

function run_cachegrind {
    echo -e "${YELLOW}Running cachegrind (cache profiling)...${NC}"
    check_tool valgrind || return 1
    
    build_benchmark valgrind
    
    cd "${BENCHMARK_BUILD_DIR}"
    rm -f cachegrind.out.*
    
    valgrind --tool=cachegrind \
             --cachegrind-out-file=cachegrind.out \
             ${BENCHMARK_EXEC} $BENCH_ARGS
    
    echo -e "${GREEN}Generating annotated source...${NC}"
    cg_annotate cachegrind.out > cachegrind_report.txt
    
    echo -e "${GREEN}Report saved to ${BENCHMARK_BUILD_DIR}/cachegrind_report.txt${NC}"
    echo ""
    echo "Cache summary:"
    grep -A 10 "I   refs:" cachegrind_report.txt
}

function run_callgrind {
    echo -e "${YELLOW}Running callgrind (call graph profiling)...${NC}"
    check_tool valgrind || return 1
    
    build_benchmark valgrind
    
    cd "${BENCHMARK_BUILD_DIR}"
    rm -f callgrind.out.*
    
    valgrind --tool=callgrind \
             --callgrind-out-file=callgrind.out \
             --collect-jumps=yes \
             --collect-systime=yes \
             ${BENCHMARK_EXEC} $BENCH_ARGS
    
    callgrind_annotate callgrind.out > callgrind_report.txt
    
    echo -e "${GREEN}Report saved to ${BENCHMARK_BUILD_DIR}/callgrind_report.txt${NC}"
    echo -e "${GREEN}View with: kcachegrind ${BENCHMARK_BUILD_DIR}/callgrind.out${NC}"
}

function run_memcheck {
    echo -e "${YELLOW}Running memcheck (memory leak detection)...${NC}"
    check_tool valgrind || return 1
    
    build_benchmark valgrind
    
    cd "${BENCHMARK_BUILD_DIR}"
    
    valgrind --tool=memcheck \
             --leak-check=full \
             --show-leak-kinds=all \
             --track-origins=yes \
             --verbose \
             --log-file=memcheck.log \
             ${BENCHMARK_EXEC} $BENCH_ARGS
    
    echo -e "${GREEN}Report saved to ${BENCHMARK_BUILD_DIR}/memcheck.log${NC}"
    echo ""
    echo "Leak summary:"
    grep -A 10 "LEAK SUMMARY:" memcheck.log || echo "No leaks found!"
}

function run_massif {
    echo -e "${YELLOW}Running massif (heap profiling)...${NC}"
    check_tool valgrind || return 1
    
    build_benchmark valgrind
    
    cd "${BENCHMARK_BUILD_DIR}"
    rm -f massif.out.*
    
    valgrind --tool=massif \
             --massif-out-file=massif.out \
             --time-unit=B \
             --detailed-freq=1 \
             --max-snapshots=100 \
             ${BENCHMARK_EXEC} $BENCH_ARGS
    
    ms_print massif.out > massif_report.txt
    
    echo -e "${GREEN}Report saved to ${BENCHMARK_BUILD_DIR}/massif_report.txt${NC}"
    echo ""
    echo "Peak memory usage:"
    grep "mem_heap_B" massif_report.txt | head -5
}

function run_perf_stat {
    echo -e "${YELLOW}Running perf stat (performance counters)...${NC}"
    check_tool perf || return 1
    
    build_benchmark
    
    cd "${BENCHMARK_BUILD_DIR}"
    
    perf stat -d \
         -e cycles,instructions,cache-references,cache-misses,branches,branch-misses \
         -o perf_stat.txt \
         ${BENCHMARK_EXEC} $BENCH_ARGS
    
    echo -e "${GREEN}Report saved to ${BENCHMARK_BUILD_DIR}/perf_stat.txt${NC}"
    cat perf_stat.txt
}

function run_perf_record {
    echo -e "${YELLOW}Running perf record (sampling profiler)...${NC}"
    check_tool perf || return 1
    
    build_benchmark
    
    cd "${BENCHMARK_BUILD_DIR}"
    
    perf record -g --call-graph=dwarf \
         -o perf.data \
         ${BENCHMARK_EXEC} $BENCH_ARGS
    
    perf report --stdio > perf_report.txt
    
    echo -e "${GREEN}Report saved to ${BENCHMARK_BUILD_DIR}/perf_report.txt${NC}"
    echo -e "${GREEN}Interactive view: perf report -i ${BENCHMARK_BUILD_DIR}/perf.data${NC}"
    echo ""
    echo "Top functions:"
    head -30 perf_report.txt | grep -A 20 "Overhead"
}

function clean_outputs {
    echo -e "${YELLOW}Cleaning profiling outputs...${NC}"
    cd "${BENCHMARK_BUILD_DIR}"
    rm -f gmon.out gprof_report.txt
    rm -f cachegrind.out* cachegrind_report.txt
    rm -f callgrind.out* callgrind_report.txt
    rm -f memcheck.log
    rm -f massif.out* massif_report.txt
    rm -f perf.data* perf_stat.txt perf_report.txt
    echo -e "${GREEN}Cleaned all profiling outputs${NC}"
}

# Parse arguments
COMMAND=""
BENCH_ARGS=""
VERBOSE=false
FILTER_ALGO=""

while [[ $# -gt 0 ]]; do
    case $1 in
        gprof|cachegrind|callgrind|memcheck|massif|perf-stat|perf-record|all|clean)
            COMMAND=$1
            shift
            ;;
        --quick)
            BENCH_ARGS="$BENCH_ARGS -q"
            shift
            ;;
        --verbose)
            BENCH_ARGS="$BENCH_ARGS -v"
            VERBOSE=true
            shift
            ;;
        --filter)
            if [[ $# -gt 1 ]]; then
                FILTER_ALGO=$2
                BENCH_ARGS="$BENCH_ARGS --filter $2"
                shift 2
            else
                echo -e "${RED}--filter requires an algorithm name${NC}"
                exit 1
            fi
            ;;
        --help|-h)
            print_usage
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            print_usage
            exit 1
            ;;
    esac
done

if [ -z "$COMMAND" ]; then
    print_usage
    exit 1
fi

# Execute command
case $COMMAND in
    gprof)
        run_gprof
        ;;
    cachegrind)
        run_cachegrind
        ;;
    callgrind)
        run_callgrind
        ;;
    memcheck)
        run_memcheck
        ;;
    massif)
        run_massif
        ;;
    perf-stat)
        run_perf_stat
        ;;
    perf-record)
        run_perf_record
        ;;
    all)
        run_gprof
        echo ""
        run_cachegrind
        echo ""
        run_callgrind
        echo ""
        run_memcheck
        echo ""
        run_massif
        ;;
    clean)
        clean_outputs
        ;;
    *)
        echo -e "${RED}Unknown command: $COMMAND${NC}"
        print_usage
        exit 1
        ;;
esac