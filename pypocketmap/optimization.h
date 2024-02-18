#ifndef PYPOCKETMAP_OPTIMIZATION_H_
#define PYPOCKETMAP_OPTIMIZATION_H_

#if defined(__has_builtin)
#define ABSL_HAVE_BUILTIN(x) __has_builtin(x)
#else
#define ABSL_HAVE_BUILTIN(x) 0
#endif

#if defined(__GNUC__) && !defined(__clang__)
// GCC
#define ABSL_NUMERIC_INTERNAL_HAVE_BUILTIN_OR_GCC(x) 1
#elif defined(__has_builtin)
#define ABSL_NUMERIC_INTERNAL_HAVE_BUILTIN_OR_GCC(x) __has_builtin(x)
#else
#define ABSL_NUMERIC_INTERNAL_HAVE_BUILTIN_OR_GCC(x) 0
#endif

// ABSL_PREDICT_TRUE, ABSL_PREDICT_FALSE
//
// Enables the compiler to prioritize compilation using static analysis for
// likely paths within a boolean branch.
//
// Example:
//
//   if (ABSL_PREDICT_TRUE(expression)) {
//     return result;                        // Faster if more likely
//   } else {
//     return 0;
//   }
//
// Compilers can use the information that a certain branch is not likely to be
// taken (for instance, a CHECK failure) to optimize for the common case in
// the absence of better information (ie. compiling gcc with `-fprofile-arcs`).
//
// Recommendation: Modern CPUs dynamically predict branch execution paths,
// typically with accuracy greater than 97%. As a result, annotating every
// branch in a codebase is likely counterproductive; however, annotating
// specific branches that are both hot and consistently mispredicted is likely
// to yield performance improvements.
#if ABSL_HAVE_BUILTIN(__builtin_expect) || \
    (defined(__GNUC__) && !defined(__clang__))
#define ABSL_PREDICT_FALSE(x) (__builtin_expect(false || (x), false))
#define ABSL_PREDICT_TRUE(x) (__builtin_expect(false || (x), true))
#else
#define ABSL_PREDICT_FALSE(x) (x)
#define ABSL_PREDICT_TRUE(x) (x)
#endif

#endif
