/// \copyright Copyright 2018 Apex.AI, Inc.
/// All rights reserved.
/// \file
/// \brief This file defines the core algorithms for square root covariance filtering.
#ifndef KALMAN_FILTER__SRCF_CORE_HPP_
#define KALMAN_FILTER__SRCF_CORE_HPP_

#include <kalman_filter/visibility_control.hpp>
#include <Eigen/Core>

#include <cmath>

namespace autoware
{
/// \brief Functionality relating to prediction, including state
///        estimation, kinematic prediction, maneuver-based prediction, map-aware prediction etc...
namespace prediction
{
/// \brief Contains the kalman filter package and related classes and functionality
namespace kalman_filter
{
/// \brief Type for matrix indexing
using index_t = Eigen::Index;

/// arbitrary small constant: 1.0E-6F
const float FEPS = 0.000001F;

/// \brief This class is an implementation of the carlson-schmidt temporal and observation update
///        for the square root covariance filter.
/// \tparam NumStates dimensionality of state space
/// \tparam ProcessNoiseDim dimensionality of process noise space
template<int NumStates, int ProcessNoiseDim>
class SrcfCore
{
public:
  using state_vec_float_t = Eigen::Matrix<float, NumStates, 1>;
  using square_mat_float_t = Eigen::Matrix<float, NumStates, NumStates>;

  using state_vec_double_t = Eigen::Matrix<double, NumStates, 1>;
  using square_mat_double_t = Eigen::Matrix<double, NumStates, NumStates>;

  /// \brief Applies the right triangularization operation on a single row, e.g.
  ///        implicitly applying the givens rotation on row vectors [f, g], where
  ///        f is an element in A(row, row), and g is an element in B(row, col_start:col_end).
  ///        This implementation assumes that rows are zeroed from 0 to N, and exploits this
  ///        assumption of zero structure above row to do less work for each implicit matrix
  ///        multiplication to zero a given element of B(row, col_start:col_end).
  ///        This is equivalent to applying a couple of right triangularization operations:
  ///        [A' | B'] <-- [A | B] T1 ... Tn
  /// \tparam ColN1 Number of cols in A
  /// \tparam ColN2 Number of cols in B
  /// \param[inout] A The matrix for whom the row'th column is pivoted
  /// \param[inout] B The matrix for whom B(row, col_start:col_end) is getting zero'd. This matrix
  ///                 can also be A, in which case col_start must be > row
  /// \param[in] row The row of B to be zero'd
  /// \param[in] col_start The starting point in the given row of B to be zero'd
  /// \param[in] col_end The ending point in the given row of B to be zero'd
  template<int ColN1, int ColN2>
  static void zero_row(
    Eigen::Matrix<float, NumStates, ColN1> & A,
    Eigen::Matrix<float, NumStates, ColN2> & B,
    const index_t row,
    const index_t col_start,
    const index_t col_end)
  {
    zero_row_common(A, B, row, col_start, col_end);
  }

  template<int ColN1, int ColN2>
  static void zero_row(
    Eigen::Matrix<double, NumStates, ColN1> & A,
    Eigen::Matrix<double, NumStates, ColN2> & B,
    const index_t row,
    const index_t col_start,
    const index_t col_end)
  {
    zero_row_common(A, B, row, col_start, col_end);
  }

  template<int ColN1, int ColN2, typename ValueType>
  static void zero_row_common(
    Eigen::Matrix<ValueType, NumStates, ColN1> & A,
    Eigen::Matrix<ValueType, NumStates, ColN2> & B,
    const index_t row,
    const index_t col_start,
    const index_t col_end)
  {
    // TODO(cvasfi): Use the proper Eigen API (eg. JacobiRotation) for matrix decompositions

    for (index_t j = col_start; j < col_end; ++j) {
      const ValueType g = B(row, j);
      // TODO(c.ho) benchmark branchi-ness
      if (fabsf(g) <= FEPS) {
        // c = 1, s = 0
        // Do nothing, identity rotation
      } else if (fabsf(A(row, row)) <= FEPS) {
        // c = 0, s = sign(g)
        for (index_t k = row; k < NumStates; ++k) {
          const ValueType tau = std::copysign(A(k, row), g);
          A(k, row) = std::copysign(B(k, j), g);
          B(k, j) = tau;
        }
      } else {
        const ValueType f = A(row, row);
        const ValueType ir = 1.0F / std::copysign(sqrtf((f * f) + (g * g)), f);
        const ValueType s = g * ir;
        const ValueType c = fabsf(f * ir);
        // TODO(c.ho) benchmark BLAS
        // probably slightly more efficient to do it this way than ~6 ish BLAS ops
        for (index_t k = row; k < NumStates; ++k) {
          const ValueType tau = (c * B(k, j)) - (s * A(k, row));
          A(k, row) = (s * B(k, j)) + (c * A(k, row));
          B(k, j) = tau;
        }
      }
    }
  }

  /// \brief Triangularize the fat partitioned matrix, [A | B] s.t.:
  ///        [A' | 0] = [A | B] * T, where A' is lower triangular.
  ///        This is equivalent to applying the Schmidt-Givens temporal update, and in general
  ///        Provides a scheme for calculating A'*A'^T = A*A^T + B*B^T in place, e.g. the
  ///        sum of two Cholesky factors.
  /// \tparam NCols2 The number of columns in the fat portion of the partitioned matrix
  /// \param[inout] A The matrix that will get lower triangularized
  /// \param[inout] B The remainder matrix that will be zero'd during triangularization
  /// \param[in] b_rank The number of columns to zero out in B
  template<int NCols2>
  static void right_lower_triangularize_matrices(
    square_mat_float_t & A,
    Eigen::Matrix<float, NumStates, NCols2> & B,
    const index_t b_rank = NCols2)
  {
    right_lower_triangularize_matrices_common(A, B, b_rank);
  }

  template<int NCols2>
  static void right_lower_triangularize_matrices(
    square_mat_double_t & A,
    Eigen::Matrix<double, NumStates, NCols2> & B,
    const index_t b_rank = NCols2)
  {
    right_lower_triangularize_matrices_common(A, B, b_rank);
  }

  template<int NCols2, typename ValueType, typename SquareMatType>
  static void right_lower_triangularize_matrices_common(
    SquareMatType & A,
    Eigen::Matrix<ValueType, NumStates, NCols2> & B,
    const index_t b_rank = NCols2)
  {
    // For each row in fat partitioned matrix, [A | B]
    for (index_t i = index_t{}; i < NumStates; ++i) {
      zero_row(A, B, i, index_t(), b_rank);
      zero_row(A, A, i, i + 1, NumStates);
    }
  }

  /// \brief do the Carlson first-order observation update, assumes uncorrelated measurement noise
  /// \param[in] z a scalar measurement (i.e. z(0))
  /// \param[in] r the associated scalar measurement noise with the scalar measurement
  ///              (i.e. R(0, 0))
  /// \param[in] h The associated row vector in the observation matrix, (i.e. H.row(0)), assumed
  ///              to have a stride of 1 (i.e. packed array, matrix in row-major format)
  /// \param[inout] C cholesky factor of the state covariance matrix, assumed to be,
  ///                 lower-triangular. Gets updated
  /// \param[inout] x the state vector, gets updated
  /// \return log-likelihood of this scalar observation
  /// \throw std::domain_error if r is not positive
  template<typename Derived, typename ValueType, typename SquareMatType, typename StateVecType>
  ValueType scalar_update_common(
    const ValueType z,
    const ValueType r,
    const Eigen::MatrixBase<Derived> & h,
    SquareMatType & C,
    StateVecType & x,
    StateVecType & k,
    StateVecType & tau)
  {
    // This implementation is adapted from the lower triangle update in
    // "Estimation with Applications to Tracking and Navigation" Bar-shalom & Li,
    // Ch 7.4: pg314-315
    // Where the implementation is modified by applying the diagonal scaling in-place,
    // i.e. P = LDL^T = SS^T <--> S = LD^(1/2)
    // all operations are guanteed to be in domain if r is positive
    if (r <= 0.0F) {
      throw std::domain_error("ScrfCore: measurement noise variance must be positive");
    }
    ValueType alpha = r;
    k.setZero();
    for (index_t idx = NumStates - 1; idx < NumStates && idx >= 0; --idx) {
      const index_t jdx = NumStates - idx;
      Eigen::Block<SquareMatType> col = C.block(idx, idx, jdx, 1);
      // f_j = C_j^T * h^T, C_j = j'th col of C
      const ValueType sigma =
        (h.block(0, idx, 1, jdx) * col)(0, 0);
      // beta = alpha_{j-1}
      const ValueType beta = alpha;
      // alpha_j = alpha_{j-1} + (f_j^2)
      alpha += (sigma * sigma);
      tau.block(0, 0, jdx, 1) = col;
      const ValueType zetap = -sigma / beta;  // beta is positive if R is positive
      // Update covariance column C_j = -K_{j-1} * (f_j / beta) + C_j
      col += zetap * k.block(idx, 0, jdx, 1);
      const ValueType etap = sqrtf(beta / alpha);  // alpha positive if beta is
      // scale by diagonal factor C_j = C_j * sqrt(beta / alpha)
      col *= etap;
      // accumulate unscaled kalman gain: K = K + f_j * C_{j, -}
      k.block(idx, 0, jdx, 1) += sigma * tau.block(0, 0, jdx, 1);
    }
    const ValueType del = z - (h * x)(0, 0);

    // alpha established to be positive
    if (alpha <= 0.0F) {
      // if code is written properly, this branch should never hit!
      throw std::runtime_error("ScrfCore: invariant broken, kalman gain must be positive");
    }
    // Update state vector by error * unscaled kalman_gain
    const ValueType del_alpha = del / alpha;
    x += del_alpha * k;

    // constant for computation (logf isn't constexpr due to errno)
    constexpr ValueType logf2pi = 1.83787706641F;
    // alpha = r + h^T * P * h = s = scalar component of innovation covariance (variance)
    // use gaussian likelihood
    return -0.5F * (logf2pi + logf(alpha) + (del * del_alpha));
  }

  template<typename Derived>
  float scalar_update(
    const float z,
    const float r,
    const Eigen::MatrixBase<Derived> & h,
    square_mat_float_t & C,
    state_vec_float_t & x)
  {
    return scalar_update_common(z, r, h, C, x, m_k_float, m_tau_float);
  }

  template<typename Derived>
  double scalar_update(
    const double z,
    const double r,
    const Eigen::MatrixBase<Derived> & h,
    square_mat_double_t & C,
    state_vec_double_t & x)
  {
    return scalar_update_common(z, r, h, C, x, m_k_double, m_tau_double);
  }

private:
  state_vec_float_t m_tau_float;
  state_vec_float_t m_k_float;

  state_vec_double_t m_tau_double;
  state_vec_double_t m_k_double;

  static_assert(NumStates > 0U, "must have positive number of states");
  static_assert(ProcessNoiseDim > 0U, "must have positive number of process noise dimensions");
};  // class SrcfCore
}  // namespace kalman_filter
}  // namespace prediction
}  // namespace autoware

#endif  // KALMAN_FILTER__SRCF_CORE_HPP_
