// Copyright 2023, Evan Palmer
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "blue_dynamics/hydrodynamics.hpp"

namespace blue::dynamics
{

[[nodiscard]] Eigen::Matrix3d createSkewSymmetricMatrix(double a1, double a2, double a3)
{
  Eigen::Matrix3d mat;
  mat << 0, -a3, a2, a3, 0, -a1, -a2, a1, 0;

  return mat;
}

[[nodiscard]] Eigen::Matrix3d createSkewSymmetricMatrix(const Eigen::Vector3d & vec)
{
  Eigen::Matrix3d mat;
  mat << 0, -vec(3), vec(2), vec(3), 0, -vec(1), -vec(2), vec(1), 0;

  return mat;
}

Inertia::Inertia(
  double mass, const Eigen::Vector3d & inertia_tensor_coeff,
  const Eigen::VectorXd & added_mass_coeff)
{
  Eigen::MatrixXd rigid_body = Eigen::MatrixXd::Zero(6, 6);

  rigid_body.topLeftCorner(3, 3) = mass * Eigen::MatrixXd::Identity(3, 3);
  rigid_body.bottomRightCorner(3, 3) = inertia_tensor_coeff.asDiagonal().toDenseMatrix();

  Eigen::MatrixXd added_mass = -added_mass_coeff.asDiagonal().toDenseMatrix();

  inertia_matrix_ = rigid_body + added_mass;
}

[[nodiscard]] Eigen::MatrixXd Inertia::getInertia() const { return inertia_matrix_; }

Coriolis::Coriolis(
  double mass, const Eigen::Vector3d & inertia_tensor_coeff,
  const Eigen::VectorXd & added_mass_coeff)
: mass_(mass),
  moments_(inertia_tensor_coeff.asDiagonal().toDenseMatrix()),
  added_mass_coeff_(std::move(added_mass_coeff))
{
}

[[nodiscard]] Eigen::MatrixXd Coriolis::calculateCoriolis(const Eigen::VectorXd & velocity) const
{
  // The rigid body Coriolis matrix only needs the angular velocity
  const Eigen::Vector3d angular_velocity = velocity.bottomRows(3);

  return calculateRigidBodyCoriolis(angular_velocity) + calculateAddedCoriolis(velocity);
}

[[nodiscard]] Eigen::MatrixXd Coriolis::calculateCoriolisDot(const Eigen::VectorXd & accel) const
{
  // The operations used to calculate the time derivative of the Coriolis matrix are the same as
  // that for the velocity (except with acceleration now), so we just wrap the original function to
  // improve readability & reduce code complexity
  return calculateCoriolis(accel);
}

[[nodiscard]] Eigen::MatrixXd Coriolis::calculateRigidBodyCoriolis(
  const Eigen::Vector3d & angular_velocity) const
{
  Eigen::MatrixXd rigid = Eigen::MatrixXd::Zero(6, 6);

  const Eigen::Vector3d moments_v2 = moments_ * angular_velocity;

  rigid.topLeftCorner(3, 3) = mass_ * createSkewSymmetricMatrix(angular_velocity);
  rigid.bottomRightCorner(3, 3) = -createSkewSymmetricMatrix(moments_v2);

  return rigid;
}

[[nodiscard]] Eigen::MatrixXd Coriolis::calculateAddedCoriolis(
  const Eigen::VectorXd & velocity) const
{
  Eigen::MatrixXd added = Eigen::MatrixXd::Zero(6, 6);

  Eigen::Matrix3d linear_vel = createSkewSymmetricMatrix(
    added_mass_coeff_(0) * velocity(0), added_mass_coeff_(1) * velocity(1),
    added_mass_coeff_(2) * velocity(2));

  Eigen::Matrix3d angular_vel = createSkewSymmetricMatrix(
    added_mass_coeff_(3) * velocity(3), added_mass_coeff_(4) * velocity(4),
    added_mass_coeff_(5) * velocity(5));

  added.topRightCorner(3, 3) = linear_vel;
  added.bottomLeftCorner(3, 3) = linear_vel;
  added.bottomRightCorner(3, 3) = angular_vel;

  return added;
}

Damping::Damping(
  const Eigen::VectorXd & linear_damping_coeff, const Eigen::VectorXd & quadratic_damping_coeff)
: linear_damping_(-linear_damping_coeff.asDiagonal().toDenseMatrix()),
  quadratic_damping_coeff_(std::move(quadratic_damping_coeff))
{
}

[[nodiscard]] Eigen::MatrixXd Damping::calculateDamping(const Eigen::VectorXd & velocity) const
{
  return linear_damping_ + calculateNonlinearDamping(velocity);
}

[[nodiscard]] Eigen::MatrixXd Damping::calculateDampingDot(const Eigen::VectorXd & accel) const
{
  // The time derivative of the damping matrix uses the same operations as that used for the
  // non-derivative version, so we just wrap the original function to improve readability & reduce
  // code complexity
  return calculateDamping(accel);
}

[[nodiscard]] Eigen::MatrixXd Damping::calculateNonlinearDamping(
  const Eigen::VectorXd & velocity) const
{
  return -(quadratic_damping_coeff_.asDiagonal().toDenseMatrix() * velocity.cwiseAbs())
            .asDiagonal()
            .toDenseMatrix();
}

RestoringForces::RestoringForces(
  double weight, double buoyancy, const Eigen::Vector3d & center_of_buoyancy,
  const Eigen::Vector3d & center_of_gravity)
: weight_(weight),
  buoyancy_(buoyancy),
  center_of_buoyancy_(center_of_buoyancy),
  center_of_gravity_(center_of_gravity)
{
}

[[nodiscard]] Eigen::VectorXd RestoringForces::calculateRestoringForces(
  const Eigen::Matrix3d & rotation) const
{
  const Eigen::Vector3d fg(0, 0, weight_);
  const Eigen::Vector3d fb(0, 0, -buoyancy_);

  Eigen::VectorXd g_rb(6);

  g_rb.topRows(3) = rotation * (fg + fb);
  g_rb.bottomRows(3) =
    center_of_gravity_.cross(rotation * fg) + center_of_buoyancy_.cross(rotation * fb);

  g_rb *= -1;

  return g_rb;
}

[[nodiscard]] Eigen::VectorXd RestoringForces::calculateRestoringForcesDot(
  const Eigen::Matrix3d & rotation, const Eigen::Vector3d & angular_velocity) const
{
  const Eigen::Matrix3d skew = createSkewSymmetricMatrix(rotation * angular_velocity);
  Eigen::Matrix3d skew_rotated = -rotation.transpose() * skew;

  return calculateRestoringForces(skew_rotated);
}

CurrentEffects::CurrentEffects(const Eigen::VectorXd & current_velocity)
: current_velocity_(std::move(current_velocity))
{
}

[[nodiscard]] Eigen::VectorXd CurrentEffects::calculateCurrentEffects(
  const Eigen::Matrix3d & rotation) const
{
  return rotation * current_velocity_;
}

HydrodynamicParameters::HydrodynamicParameters(
  const Inertia & inertia, const Coriolis & coriolis, const Damping & damping,
  const RestoringForces & restoring_forces, const CurrentEffects & current_effects)
: inertia(inertia),
  coriolis(coriolis),
  damping(damping),
  restoring_forces(restoring_forces),
  current_effects(current_effects)
{
}

}  // namespace blue::dynamics
