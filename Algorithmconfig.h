#ifndef ALGORITHM_CONFIG_H
#define ALGORITHM_CONFIG_H

#include <string>

/**
 * @file AlgorithmConfig.h
 * @brief Configuration parsed from a registers input file.
 *
 * Stores the number of available physical registers and the algorithm
 * variant to use for register allocation.
 *
 * Supported algorithm variants:
 *  - "basic"        : greedy graph-coloring, no fallback (T2.1)
 *  - "spilling,K"   : basic + up to K web spills (T2.2)
 *  - "splitting,K"  : basic + up to K web splits (T2.3)
 *  - "free"         : custom algorithm (T2.4)
 */
struct AlgorithmConfig {
    /** @brief Number of physical registers available (r0 … r_{N-1}). */
    int numRegisters = 0;

    /**
     * @brief Algorithm variant identifier.
     *
     * One of: "basic", "spilling", "splitting", "free".
     */
    std::string algorithm = "basic";

    /**
     * @brief Numeric parameter for spilling/splitting variants.
     *
     * Maximum number of webs that may be spilled or split.
     * Ignored for "basic" and "free".
     */
    int algorithmParam = 0;
};

#endif // ALGORITHM_CONFIG_H