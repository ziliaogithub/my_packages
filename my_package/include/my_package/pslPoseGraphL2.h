#ifndef _PSLPOSEGRAPH_H_
#define _PSLPOSEGRAPH_H_

#include <string>
#include <fstream>
#include <sstream>
#include <iostream> 
#include <vector>
using namespace std;

#include "Eigen/Core"
#include "Eigen/Geometry"

#include "ceres/ceres.h"
#include "ceres/rotation.h"
#include "ceres/local_parameterization.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//3D pose by Eigen 
struct Pose3d
{
  Eigen::Vector3d p;
  Eigen::Quaterniond q;

  Pose3d()
  {
      q.setIdentity();
      p.fill(0.);
  };

  Pose3d(const Pose3d &_p):
        p(_p.p), q(_p.q)
  {};

  Pose3d(const Eigen::Vector3d &_p, const Eigen::Quaterniond &_q):
        p(_p), q(_q)
  {};

  Pose3d(const Eigen::Vector3d &t, const Eigen::Matrix3d &R): 
        p(t), q(Eigen::Quaterniond(R))
  {};

  Pose3d inverse() const
  {
    return Pose3d(q.conjugate()*(-1.*p), q.conjugate());
  }

  Pose3d operator *(const Pose3d& other) const 
  {
    Pose3d ret;
    ret.q = q*other.q;
    ret.p = (q*other.p)+p;
    return ret;
  }

  Eigen::Vector3d map (const Eigen::Vector3d& xyz) const 
  {
    return (q*xyz) + p;
  }

  inline const Eigen::Vector3d& translation() const {return p;}

  inline Eigen::Vector3d& translation() {return p;}

  inline const Eigen::Quaterniond& rotation() const {return q;}

  inline Eigen::Quaterniond& rotation() {return q;}

  friend ostream& operator<<(ostream& os, const Pose3d& ret) { 
	  os << "p=[" << ret.p.x() << "," << ret.p.y() << "," << ret.p.z() << "]," 
	     << "q=[" << ret.q.x() << "," << ret.q.y() << "," << ret.q.z() << "," << ret.q.w() << "]";  
      return os;  
  } 
};
typedef struct Pose3d Pose3d;

typedef vector<Pose3d, Eigen::aligned_allocator<Pose3d>> VectorofPoses;
typedef vector<Eigen::Vector3d, Eigen::aligned_allocator<Eigen::Vector3d>> VectorofNormalVectors;

// The constraint between two vertices in the pose graph. The constraint is the
// transformation from vertex id_begin to vertex id_end.
struct Constraint3d {
  int id_begin;
  int id_end;

  // The transformation that represents the pose of the end frame E w.r.t. the
  // begin frame B. In other words, it transforms a vector in the E frame to
  // the B frame.
  Pose3d t_be;

  // The inverse of the covariance matrix for the measurement. The order of the
  // entries are x, y, z, delta orientation.
  Eigen::Matrix<double, 7, 7> information;

  // L2, ground surface normal vector
  Eigen::Vector3d nvec_a_, nvec_b_;

  Constraint3d() 
  {
	  id_begin = -1;
	  id_end = -1;
    t_be.q.setIdentity();
    t_be.p.fill(0.);
	  information.setIdentity();
    nvec_a_.fill(0.);
    nvec_b_.fill(0.);
  };

  Constraint3d(int ib, int ie, Pose3d t_be_, Eigen::Matrix<double, 7, 7> inf):
      id_begin(ib), id_end(ie), t_be(t_be_), information(inf)
  {
    nvec_a_.fill(0.);
    nvec_b_.fill(0.);
  };

  Constraint3d(int ib, int ie, Pose3d t_be_):
      id_begin(ib), id_end(ie), t_be(t_be_)
  {
    information.setIdentity();
    nvec_a_.fill(0.);
    nvec_b_.fill(0.);
  };

  Constraint3d(int ib, int ie, Pose3d t_be_, Eigen::Vector3d nv_a, Eigen::Vector3d nv_b):
      id_begin(ib), id_end(ie), t_be(t_be_), nvec_a_(nv_a), nvec_b_(nv_b)
  {
      information.setIdentity();
  };
};

typedef vector<Constraint3d, Eigen::aligned_allocator<Constraint3d>> VectorOfConstraints;

class PoseGraph3dErrorTerm 
{
 public:
  PoseGraph3dErrorTerm(const Pose3d& t_ab_measured,
                       const Eigen::Matrix<double, 7, 7>& sqrt_information,
                       const Eigen::Vector3d& nv_a,
                       const Eigen::Vector3d& nv_b,
                       const double lambda)
    : t_ab_measured_(t_ab_measured)
    , sqrt_information_(sqrt_information)
    , nvec_a_(nv_a.normalized())
    , nvec_b_(nv_b.normalized())
    , lambda_(lambda)
  {};

  template <typename T>
  bool operator()(const T* const p_a_ptr, const T* const q_a_ptr,
                  const T* const p_b_ptr, const T* const q_b_ptr,
                  T* residuals_ptr) const 
  {
    Eigen::Map<const Eigen::Matrix<T, 3, 1> > p_a(p_a_ptr);
    Eigen::Map<const Eigen::Quaternion<T> > q_a(q_a_ptr);

    Eigen::Map<const Eigen::Matrix<T, 3, 1> > p_b(p_b_ptr);
    Eigen::Map<const Eigen::Quaternion<T> > q_b(q_b_ptr);

	  // Compute inversion of b (T)
    Eigen::Quaternion<T> q_b_inverse = q_b.conjugate();
    Eigen::Matrix<T, 3, 1> p_b_inverse = q_b_inverse*(-p_b);

    // Compute the relative rotation between the two frames.
    Eigen::Quaternion<T> q_ab_estimated = q_b_inverse * q_a;

    // Represent the displacement between the two frames in the A frame.
    Eigen::Matrix<T, 3, 1> p_ab_estimated = q_b_inverse * p_a + p_b_inverse;

    // Compute the error between the two orientation estimates.
    Eigen::Quaternion<T> delta_q =
        t_ab_measured_.q.template cast<T>() * q_ab_estimated.conjugate();

    // Compute the residuals.
    // [ position         ]   [ delta_p          ]
    // [ orientation (3x1)] = [ 2 * delta_q(0:2) ]
    Eigen::Map<Eigen::Matrix<T, 7, 1> > residuals(residuals_ptr);
    residuals.template block<3, 1>(0, 0) =
        p_ab_estimated - t_ab_measured_.p.template cast<T>();
    residuals.template block<3, 1>(3, 0) = T(2.0) * delta_q.vec();

    // Add the regu term
    // Eigen::Vector3d nvec_a_xz(nvec_a_[0], 0., nvec_a_[2]);
    // nvec_a_xz.normalize();
    // Eigen::Vector3d nvec_b_xz(nvec_b_[0], 0., nvec_b_[2]);
    // nvec_b_xz.normalize();
    // Eigen::Matrix<T, 3, 1> n_a_global = (q_a.toRotationMatrix() * nvec_a_xz.template cast<T>()).normalized();
    // Eigen::Matrix<T, 3, 1> n_b_global = (q_b.toRotationMatrix() * nvec_b_xz.template cast<T>()).normalized();
    Eigen::Matrix<T, 3, 1> n_a_global = (q_a.toRotationMatrix() * nvec_a_.template cast<T>()).normalized();
    Eigen::Matrix<T, 3, 1> n_b_global = (q_b.toRotationMatrix() * nvec_b_.template cast<T>()).normalized();
    // std::cout << "Vec" << k << ": [" << n_a_global[0] << ",,," << n_a_global[1] << ",,," << n_a_global[2] << "]" << std::endl;

    residuals[6] = sqrt(lambda_ * (1.0 - n_a_global[0] * n_b_global[0] - n_a_global[1] * n_b_global[1] - n_a_global[2] * n_b_global[2]));

    // Scale the residuals by the measurement uncertainty.
    residuals.applyOnTheLeft(sqrt_information_.template cast<T>());

    return true;
  }

  static ceres::CostFunction* Create(const Pose3d& t_ab_measured,
                                     const Eigen::Matrix<double, 7, 7>& sqrt_information,
                                     const Eigen::Vector3d nv_a,
                                     const Eigen::Vector3d nv_b,
                                     const double lambda)
  {
    return new ceres::AutoDiffCostFunction<PoseGraph3dErrorTerm, 7, 3, 4, 3, 4>(
              new PoseGraph3dErrorTerm(t_ab_measured, sqrt_information, nv_a, nv_b, lambda));
  }

 private:
  // The measurement for the position of B relative to A in the A frame.
  const Pose3d t_ab_measured_;
  // The square root of the measurement information matrix.
  const Eigen::Matrix<double, 7, 7> sqrt_information_;
  // Norm vec
  const Eigen::Vector3d nvec_a_, nvec_b_;
  // L2 strength
  const double lambda_;
};

bool optimizeEssentialGraphWithL2(const VectorofPoses &NonCorrectedSim3, 
                                  const VectorofNormalVectors &GroundNormalVector3,
                                  const double regularization_strength,
                                  Pose3d endCorrectedPose,
      							              VectorofPoses &CorrectedSim3);

#endif
