/*
 * Copyright 2023 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "src/base/SkQuads.h"

#include "include/private/base/SkFloatingPoint.h"

#include <cmath>

// Solve 0 = M * x + B. If M is 0, there are no solutions, unless B is also 0,
// in which case there are infinite solutions, so we just return 1 of them.
static int solve_linear(const double M, const double B, double solution[2]) {
    if (sk_double_nearly_zero(M)) {
        solution[0] = 0;
        if (sk_double_nearly_zero(B)) {
            return 1;
        }
        return 0;
    }
    solution[0] = -B / M;
    if (!std::isfinite(solution[0])) {
        return 0;
    }
    return 1;
}

// When the A coefficient of a quadratic is close to 0, there can be floating point error
// that arises from computing a very large root. In those cases, we would rather be
// precise about the one smaller root, so we have this arbitrary cutoff for when A is
// really small or small compared to B.
static bool close_to_linear(double A, double B) {
    if (sk_double_nearly_zero(B)) {
        return sk_double_nearly_zero(A);
    }
    // This is a different threshold (tighter) than the close_to_a_quadratic in SkCubics.cpp
    // because the SkQuads::RootsReal gives better answers for longer as A/B -> 0.
    return std::abs(A / B) < 1.0e-16;
}

double SkQuads::Discriminant(const double a, const double b, const double c) {
    const double b2 = b * b;
    const double ac = a * c;

    // Calculate the rough discriminate which may suffer from a loss in precision due to b2 and
    // ac being too close.
    const double roughDiscriminant = b2 - ac;

    // Check if b2 and ac were too close, and caused catastrophic cancellation. The
    // roughDiscriminant should be good enough most of the time. If b2 and ac are very close to
    // each other, then roughDiscriminant will be very small with most of the bits canceled
    // making much smaller than roundOffCheck.
    const double roundOffCheck = b2 + ac;
    if (3 * std::abs(roughDiscriminant) >= roundOffCheck) {
        return roughDiscriminant;
    }

    // Use the extra internal precision afforded by fma to calculate the rounding error for
    // b^2 and ac.
    const double b2RoundingError = std::fma(b, b, -b2);
    const double acRoundingError = std::fma(a, c, -ac);

    // Add the total rounding error back into the discriminant guess.
    const double discriminant = (b2 - ac) + (b2RoundingError - acRoundingError);
    return discriminant;
}

int SkQuads::RootsReal(const double A, const double B, const double C, double solution[2]) {
    if (close_to_linear(A, B)) {
        return solve_linear(B, C, solution);
    }
    // If A is zero (e.g. B was nan and thus close_to_linear was false), we will
    // temporarily have infinities rolling about, but will catch that when checking
    // p2 - q.
    const double p = sk_ieee_double_divide(B, 2 * A);
    const double q = sk_ieee_double_divide(C, A);
    /* normal form: x^2 + px + q = 0 */
    const double p2 = p * p;
    if (!std::isfinite(p2 - q) ||
        (!sk_double_nearly_zero(p2 - q) && p2 < q)) {
        return 0;
    }
    double sqrt_D = 0;
    if (p2 > q) {
        sqrt_D = sqrt(p2 - q);
    }
    solution[0] = sqrt_D - p;
    solution[1] = -sqrt_D - p;
    if (sk_double_nearly_zero(sqrt_D) ||
        sk_doubles_nearly_equal_ulps(solution[0], solution[1])) {
        return 1;
    }
    return 2;
}
