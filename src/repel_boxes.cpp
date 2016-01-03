#include <Rcpp.h>
using namespace Rcpp;

// //' Find the intersections between a line and a rectangle.
// //' @param p1 A point c(x, y)
// //' @param p2 A point c(x, y)
// //' @param b A rectangle c(x1, y1, x2, y2)
// // [[Rcpp::export]]
// NumericVector intersect_line_rectangle(
//     NumericVector p1, NumericVector p2, NumericVector b
// ) {
//   double slope = (p2[1] - p1[1]) / (p2[0] - p1[0]);
//   double intercept = p2[1] - p2[0] * slope;
//   NumericMatrix retval(4, 2);
//
//   double x, y;
//
//   x = b[0];
//   y = slope * x + intercept;
//   if (b[1] <= y && y <= b[3]) {
//     retval(0, _) = NumericVector::create(x, y);
//   }
//
//   x = b[2];
//   y = slope * x + intercept;
//   if (b[1] <= y && y <= b[3]) {
//     retval(1, _) = NumericVector::create(x, y);
//   }
//
//   y = b[1];
//   x = (y - intercept) / slope;
//   if (b[0] <= y && y <= b[2]) {
//     retval(2, _) = NumericVector::create(x, y);
//   }
//
//   y = b[3];
//   x = (y - intercept) / slope;
//   if (b[0] <= y && y <= b[2]) {
//     retval(3, _) = NumericVector::create(x, y);
//   }
//
//   int i = 0;
//   int imin = 0;
//   double d = euclid(retval(i, _), p1);
//   double dmin = d;
//   for (i = 1; i < 4; i++) {
//     d = euclid(retval(i, _), p1);
//     if (d < dmin) {
//       d = dmin;
//       imin = i;
//     }
//   }
//
//   return retval(imin, _);
// }

// [[Rcpp::export]]
double euclid(NumericVector p1, NumericVector p2) {
  return sqrt(pow(p2[1] - p1[1], 2) + pow(p2[0] - p1[0], 2));
}

// [[Rcpp::export]]
NumericVector put_within_bounds(
    NumericVector b, NumericVector xlim, NumericVector ylim
) {
  double d;
  if (b[0] < xlim[0]) {
    d = fabs(b[0] - xlim[0]);
    b[0] += d;
    b[2] += d;
  } else if (b[2] > xlim[1]) {
    d = fabs(b[2] - xlim[1]);
    b[0] -= d;
    b[2] -= d;
  }
  if (b[1] < ylim[0]) {
    d = fabs(b[1] - ylim[0]);
    b[1] += d;
    b[3] += d;
  } else if (b[3] > ylim[1]) {
    d = fabs(b[3] - ylim[1]);
    b[1] -= d;
    b[3] -= d;
  }
  return b;
}

// [[Rcpp::export]]
NumericVector centroid(NumericVector b) {
  return NumericVector::create((b[0] + b[2]) / 2, (b[1] + b[3]) / 2);
}

//' Test if a box overlaps another box.
// [[Rcpp::export]]
bool overlaps(NumericVector a, NumericVector b) {
  return
    b[0] <= a[2] &&
    b[1] <= a[3] &&
    b[2] >= a[0] &&
    b[3] >= a[1];
}

//' Test if a point is within the boundaries of a box.
// [[Rcpp::export]]
bool point_within_box(NumericVector p, NumericVector b) {
  return
    p[0] >= b[0] &&
    p[0] <= b[2] &&
    p[1] >= b[1] &&
    p[1] <= b[3];
}

//' Compute the force upon point a from point b.
// [[Rcpp::export]]
NumericVector compute_force(
    NumericVector a, NumericVector b, double force = 0.000001
) {
  a += rnorm(2, 0, force);
  double d = std::max(euclid(a, b), 0.01);
  NumericVector v = (a - b) / d;
  return force * v / pow(d, 2);
}

//' Adjust the layout of a list of potentially overlapping boxes.
// [[Rcpp::export]]
DataFrame repel_boxes(
    NumericMatrix boxes, NumericVector xlim, NumericVector ylim,
    double force = 1e-6, int maxiter = 1e4
) {
  int n = boxes.nrow();
  int iter = 0;
  bool any_overlaps = true;

  if (NumericVector::is_na(force)) {
    force = 1e-6;
  }

  // height over width
  NumericVector ratios(n);
  NumericVector b(4);
  for (int i = 0; i < n; i++) {
    b = boxes(i, _);
    ratios[i] = (b[3] - b[1]) / (b[2] - b[0]);
  }
  NumericMatrix original_centroids(n, 2);
  for (int i = 0; i < n; i++) {
    original_centroids(i, _) = centroid(boxes(i, _));
  }

  double mx = 0, my = 0;
  NumericVector f(2);
  NumericVector ci(2);
  NumericVector cj(2);

  while (any_overlaps && iter < maxiter) {
    iter += 1;
    any_overlaps = false;

    for (int i = 0; i < n; i++) {
      f[0] = 0;
      f[1] = 0;

      ci = centroid(boxes(i, _));

      for (int j = 0; j < n; j++) {

        cj = centroid(boxes(j, _));

        if (i == j) {
          // Repel the box from its own centroid.
          if (point_within_box(original_centroids(i, _), boxes(i, _))) {
            any_overlaps = true;
            f = f + compute_force(ci, original_centroids(i, _), force);
          }
        } else {
          // Repel the box from overlapping boxes.
          if (overlaps(boxes(i, _), boxes(j, _))) {
            any_overlaps = true;
            f = f + compute_force(ci, cj, force);
          }
          // Repel the box from overlapping centroids.
          if (point_within_box(original_centroids(j, _), boxes(i, _))) {
            any_overlaps = true;
            f = f + compute_force(ci, original_centroids(j, _), force);
          }
        }
      }

      // Pull toward the label's point.
      if (!any_overlaps) {
        f = f + compute_force(original_centroids(i, _), ci, force);
      }

      f[0] = f[0] * ratios[i];

      if (f[0] > mx) mx = f[0];
      if (f[1] > my) my = f[1];
      b = boxes(i, _);
      boxes(i, _) = NumericVector::create(
        b[0] + f[0], b[1] + f[1], b[2] + f[0], b[3] + f[1]
      );
      boxes(i, _) = put_within_bounds(boxes(i, _), xlim, ylim);
    }
  }

  Rcout << "iter " << iter << std::endl;
  Rcout << "max force x " << mx << std::endl;
  Rcout << "max force y " << my << std::endl;

  NumericVector xs(n);
  NumericVector ys(n);

  for (int i = 0; i < n; i++) {
    b = boxes(i, _);
    xs[i] = (b[0] + b[2]) / 2;
    ys[i] = (b[1] + b[3]) / 2;
  }

  return Rcpp::DataFrame::create(
    Rcpp::Named("x") = xs,
    Rcpp::Named("y") = ys
  );
}

// You can include R code blocks in C++ files processed with sourceCpp
// (useful for testing and development). The R code will be automatically
// run after the compilation.
//

/*** R
# timesTwo(42)
*/
