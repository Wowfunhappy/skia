/*
 * Copyright 2023 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SkCubics_DEFINED
#define SkCubics_DEFINED

/**
 * Utilities for dealing with cubic formulas with one variable:
 *   f(t) = A*t^3 + B*t^2 + C*t + d
 */
class SkCubics {
public:
    /**
     * Puts up to 3 real solutions to the equation
     *   A*t^3 + B*t^2 + C*t + d = 0
     * in the provided array.
     */
    static int RootsReal(double A, double B, double C, double D,
                         double solution[3]);

    /**
     * Puts up to 3 real solutions to the equation
     *   A*t^3 + B*t^2 + C*t + d = 0
     * in the provided array, with the constraint that t is in the range [0.0, 1.0].
     */
    static int RootsValidT(double A, double B, double C, double D,
                           double solution[3]);


    /**
     * Evaluates the a cubic function with the 4 provided coefficients and the
     * provided variable.
     */
    static double EvalAt(double coefficients[4], double t) {
        return coefficients[0] * t * t * t +
               coefficients[1] * t * t +
               coefficients[2] * t +
               coefficients[3];
    }
};

#endif
