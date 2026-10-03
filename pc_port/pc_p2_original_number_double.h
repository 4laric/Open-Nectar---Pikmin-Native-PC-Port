#pragma once
namespace p2originalnumber { namespace doubleMath {
// Exact finite binary64 a*b+c, one nearest/even rounding. Integer arithmetic
// avoids dependence on host FMA implementations. IEEE nearest/gradual underflow
// required; failures preserve output. No original FPSCR or hardware grant.
bool sourceFma(double a,double b,double c,double& output) noexcept;
} }
