// Copyright (C) 2019-2023 Rhys Mainwaring
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// This code modified from the gazebo LiftDrag plugin
/*
* Copyright (C) 2012 Open Source Robotics Foundation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*/

#include "asv/sim/LiftDragModel.hh"

#include <string>

#include <gz/math/Pose3.hh>
#include <gz/math/Vector3.hh>

#include "asv/sim/Utilities.hh"

namespace asv
{
class LiftDragModelPrivate
{
  /// \brief Fluid density
  public: double fluidDensity = 1.2;

  /// \brief True if the foil is symmetric about its chord.
  public: bool radialSymmetry = true;

  /// \brief Foil forward direction (body frame), usually parallel
  /// to the foil chord.
  public: gz::math::Vector3d forward = gz::math::Vector3d(1, 0, 0);

  /// \brief Foil upward direction (body frame), usually perpendicular
  /// to the foil chord in the direction of positive lift for the foil
  /// in its intended configuration.
  public: gz::math::Vector3d upward = gz::math::Vector3d(0, 0, 1);

  /// \brief Foil area
  public: double area = 1.0;

  /// \brief Angle of attack at zero lift.
  public: double alpha0 = 0.0;

  /// \brief Slope of lift coefficient before stall.
  public: double cla = 2.0 * GZ_PI;

  /// \brief Angle of attack at stall.
  public: double alphaStall = 1.0 / 2.0 / GZ_PI;

  /// \brief Slope of lift coefficient after stall.
  public: double claStall = -(2 * GZ_PI) / (GZ_PI * GZ_PI - 1.0);

  /// \brief Slope of drag coefficient.
  public: double cda = 2.0 / GZ_PI;

  /// \brief Slope of skin friction coefficient.
  public: double cf = 0.0;

  /// \brief radius around stall region giving a softer stall
  public: double rStall = 0.0;
};

/////////////////////////////////////////////////
LiftDragModel::~LiftDragModel() = default;

/////////////////////////////////////////////////
LiftDragModel::LiftDragModel()
    : data(std::make_unique<LiftDragModelPrivate>())
{
}

/////////////////////////////////////////////////
LiftDragModel::LiftDragModel(std::unique_ptr<LiftDragModelPrivate> &_data)
    : data(std::move(_data))
{
}

/////////////////////////////////////////////////
LiftDragModel* LiftDragModel::Create(
    const std::shared_ptr<const sdf::Element> &_sdf)
{
  std::unique_ptr<LiftDragModelPrivate> data(
      std::make_unique<LiftDragModelPrivate>());

  // Parameters
  asv::LoadParam(_sdf, "fluid_density", data->fluidDensity,
      data->fluidDensity);
  asv::LoadParam(_sdf, "radial_symmetry", data->radialSymmetry,
      data->radialSymmetry);
  asv::LoadParam(_sdf, "forward", data->forward, data->forward);
  asv::LoadParam(_sdf, "upward", data->upward, data->upward);
  asv::LoadParam(_sdf, "area", data->area, data->area);
  asv::LoadParam(_sdf, "a0", data->alpha0, data->alpha0);
  asv::LoadParam(_sdf, "alpha_stall", data->alphaStall, data->alphaStall);
  asv::LoadParam(_sdf, "cla", data->cla, data->cla);
  asv::LoadParam(_sdf, "cla_stall", data->claStall, data->claStall);
  asv::LoadParam(_sdf, "cda", data->cda, data->cda);
  asv::LoadParam(_sdf, "cf", data->cf, data->cf);
  asv::LoadParam(_sdf, "r_stall", data->rStall, data->rStall);

  // Only support radially symmetric lift-drag coefficients at present
  if (!data->radialSymmetry)
  {
    gzerr << "LiftDragModel only supports radially symmetric foils\n";
    return nullptr;
  }

  if ( data->rStall >= data->alphaStall){
    gzerr << "r_stall must be less than alpha_stall\n";
    return nullptr;
  }
  // avoid div epsilon on stall radius
  if ( (data->rStall > 0.0) && (data->rStall < 0.01) ){
    gzerr << "non zero r_stall must be greater equal 0.01\n";
    return nullptr;
  }

  // Normalise
  data->forward.Normalize();
  data->upward.Normalize();

  return new LiftDragModel(data);
}

/////////////////////////////////////////////////
void LiftDragModel::Compute(
  const gz::math::Vector3d &_velU,
  const gz::math::Pose3d &_bodyPose,
  gz::math::Vector3d &_lift,
  gz::math::Vector3d &_drag) const
{
  double alpha, u, cl, cd;
  this->Compute(_velU, _bodyPose, _lift, _drag, alpha, u, cl, cd);
}

/////////////////////////////////////////////////
void LiftDragModel::Compute(
  const gz::math::Vector3d &_velU,
  const gz::math::Pose3d &_bodyPose,
  gz::math::Vector3d &_lift,
  gz::math::Vector3d &_drag,
  double &_alpha,
  double &_u,
  double &_cl,
  double &_cd) const
{
  // Unit free stream velocity (world frame).
  auto velUnit = _velU;
  velUnit.Normalize();

  // Avoid division by zero issues.
  if (_velU.Length() <= 0.01)
  {
    _lift = gz::math::Vector3d::Zero;
    _drag = gz::math::Vector3d::Zero;
    return;
  }

  // Rotate forward and upward vectors into the world frame.
  auto forwardI = _bodyPose.Rot().RotateVector(this->data->forward);
  auto upwardI = _bodyPose.Rot().RotateVector(this->data->upward);

  // The span vector is normal to lift-drag-plane (world frame)
  auto spanI = forwardI.Cross(upwardI).Normalize();

  // Compute the angle of attack, alpha:
  // This is the angle between the free stream velocity
  // projected into the lift-drag plane and the forward vector
  auto velLD = _velU - _velU.Dot(spanI) * spanI;

  // Get direction of drag
  auto dragUnit = velLD;
  dragUnit.Normalize();

  // Get direction of lift
  auto liftUnit = dragUnit.Cross(spanI);
  liftUnit.Normalize();

  // Compute angle of attack.
  double sgnAlpha =  forwardI.Dot(liftUnit) < 0 ? -1.0 : 1.0;
  double cosAlpha = -forwardI.Dot(dragUnit);
  // lift-drag coefficients assume alpha > 0 if foil is symmetric
  double alpha = acos(cosAlpha);

  // Compute dynamic pressure.
  double u = velLD.Length();
  double q = 0.5 * this->data->fluidDensity * u * u;

  // Compute lift coefficient and set sign.
  double cl = this->LiftCoefficient(alpha) * sgnAlpha;

  // Compute lift force.
  _lift = cl * q * this->data->area * liftUnit;

  // Compute drag coefficient due to lift.
  double cd = this->DragCoefficient(alpha);

  // Compute chordwise velocity.
  double uf = u * cosAlpha;

  // Compute chordwise dynamic pressure.
  double qf = 0.5 * this->data->fluidDensity * uf * uf ;

  // Read drag coefficient due to skin friction.
  double cf = this->data->cf ;

  // Compute total drag force from vortex and skin friction drag
  _drag = ((cd * q) + (cf * qf)) * this->data->area * dragUnit;

  // Outputs
  _alpha = alpha;
  _u = u;
  _cl = cl;
  _cd = cd;

  // DEBUG
#if 0
  gzmsg << "velU:         " << _velU << "\n";
  gzmsg << "velUnit:      " << velUnit << "\n";
  gzmsg << "body_pos:     " << _bodyPose.Pos() << "\n";
  gzmsg << "body_rot:     " << _bodyPose.Rot().Euler() << "\n";
  gzmsg << "forward:      " << this->data->forward << "\n";
  gzmsg << "upward:       " << this->data->upward << "\n";
  gzmsg << "forwardI:     " << forwardI << "\n";
  gzmsg << "upwardI:      " << upwardI << "\n";
  gzmsg << "spanI:        " << spanI << "\n";
  gzmsg << "velLD:        " << velLD << "\n";
  gzmsg << "dragUnit:     " << dragUnit << "\n";
  gzmsg << "liftUnit:     " << liftUnit << "\n";
  gzmsg << "alpha:        " << alpha << "\n";
  gzmsg << "u:            " << _u << "\n";
  gzmsg << "cl:           " << _cl << "\n";
  gzmsg << "cd:           " << _cd << "\n";
  gzmsg << "lift:         " << _lift << "\n";
  gzmsg << "drag:         " << _drag << "\n\n";
#endif
}

/////////////////////////////////////////////////
/// Lift is piecewise linear and symmetric about alpha = PI/2
double LiftDragModel::LiftCoefficient(double _alpha) const
{
  double alpha0     = this->data->alpha0;
  double cla        = this->data->cla;
  double alphaStall = this->data->alphaStall;
  double claStall   = this->data->claStall;
  double rStall     = this->data->rStall;

  if ( rStall > 0.0){

// The angle of the straight slope  slope in the lift region
   double const straightLiftSlopeAngle = atan2(cla,1.0);

   // The angle of the straight slope in the stall region
   double const straightStallSlopeAngle = atan2(claStall,1.0);

   // angle between lift and stall straight slope lines
   double const betweenLiftStallSlopesAngle
      = straightStallSlopeAngle + (GZ_PI - straightLiftSlopeAngle);
   // distance from the stall point at corner between lift and stall lines
   double const StallPointToR_StallCenter = rStall / sin(betweenLiftStallSlopesAngle/2.0);

   // Angle of center of stall circle with StallPoint
   double const StallCircleAngle = betweenLiftStallSlopesAngle/2.0 + straightLiftSlopeAngle;

   /// return cl on straight part of lift curve as a function of angle of attack
   /// @param[in] alpha angle of attack of the foil in radians
   /// @return cl at alpha
   auto clStraightLift = [=] (double alpha){
       return cla * (alpha - alpha0);
   };
   //using vect = quan::two_d::vect<double>;
   using vect = gz::math::Vector2d;
   // center of stall r circle rel to stall point
   auto const rawCircleCenter = vect{cos(StallCircleAngle), sin(StallCircleAngle)} * -StallPointToR_StallCenter;

// point where stall would occur if stall radius was 0
   auto const rawStallPoint = vect{alphaStall,clStraightLift(alphaStall)};

// The center of the stall radius circle on the graph
   auto const stall_R_CircleCenter = rawCircleCenter + rawStallPoint;

   // xaxis maximum angle to pick off cl (y) on straight lift line
   double const maxStraightAlpha = stall_R_CircleCenter.X() - rStall * sin(straightLiftSlopeAngle);

   // xaxis minimum angle  to pick off cl (y) on straight stall line
   double const minStraightStallAlpha = stall_R_CircleCenter.X()- rStall * sin(straightStallSlopeAngle);

   // return cl on straight part of stall curve
   auto clStraightStall = [=](double alpha){
      return  claStall * (alpha - alphaStall) + clStraightLift(alphaStall);
   };

   // cl on stall_circle curve around stall region
   auto clStall_R =[=] (double alpha) {
      return stall_R_CircleCenter.Y() + sin(acos((stall_R_CircleCenter.X() - alpha)/rStall)) * rStall;
   };

   // return cl for alpha
   auto fn_cl = [=]( double alpha){

      double const r = ( alpha <= maxStraightAlpha)
      ? clStraightLift(alpha)
      : ( alpha >= minStraightStallAlpha )
         ? clStraightStall(alpha)
         : clStall_R(alpha);

      return std::max(r,0.0);
    };

     double cl = 0.0;
     if (_alpha < GZ_PI/2.0)
     {
       cl = fn_cl(_alpha);
     }
     else
     {
       cl = -fn_cl(GZ_PI - _alpha);
     }

     return cl;

  }else{

     auto f1 = [=](auto _x)
     {
       return cla * (_x - alpha0);
     };

     auto f2 = [=](auto _x)
     {
       if (_x < alphaStall)
         return f1(_x);
       else
         return claStall * (_x - alphaStall) + f1(alphaStall);
     };

     double cl = 0.0;
     if (_alpha < GZ_PI/2.0)
     {
       cl = f2(_alpha);
     }
     else
     {
       cl = -f2(GZ_PI - _alpha);
     }

     return cl;
  }
}

/////////////////////////////////////////////////
/// Drag is piecewise linear and symmetric about alpha = PI/2
double LiftDragModel::DragCoefficient(double _alpha) const
{
  double cda = this->data->cda;

  auto f1 = [=](auto _x)
  {
    return cda * _x;
  };

  double cd = 0.0;
  if (_alpha < GZ_PI/2.0)
  {
    cd = f1(_alpha);
  }
  else
  {
    cd = f1(GZ_PI - _alpha);
  }

  return cd;
}

}  // namespace asv
