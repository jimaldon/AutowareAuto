// Copyright 2017-2019 Apex.AI, Inc.
// Co-developed by Tier IV, Inc. and Apex.AI, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/// \file
/// \brief This file defines the computational core of the ray ground filter algorithm

#ifndef RAY_GROUND_CLASSIFIER__RAY_GROUND_POINT_CLASSIFIER_HPP_
#define RAY_GROUND_CLASSIFIER__RAY_GROUND_POINT_CLASSIFIER_HPP_

#include <cmath>
#include <vector>

#include "lidar_utils/lidar_types.hpp"
#include "ray_ground_classifier/visibility_control.hpp"

namespace autoware
{
/// \brief Perception related algorithms and functionality, such as those
///        acting on 3D lidar data, camera data, radar, or ultrasonic information
namespace perception
{
/// \brief Classifiers and operations that act to reduce or organize data. Currently
///        this namespace is strictly for point cloud filters, e.g. voxelgrid and ground filtering,
///        but in the future it may include filtering for images and other functionality
namespace filters
{
using autoware::common::lidar_utils::PI;
using autoware::common::lidar_utils::TAU;
using autoware::common::lidar_utils::PointXYZIF;

/// \brief Resources for the ray ground filter algorithm used for
///        ground filtering in point clouds
namespace ray_ground_classifier
{
template<typename T>
inline T clamp(const T val, const T min, const T max)
{
  return (val < min) ? min : ((val > max) ? max : val);
}

/// \brief th_rad - phi_rad, normalized to +/- pi
/// \param[in] th_rad the reference angle
/// \param[in] phi_rad the test angle
/// \return angle from reference angle to test angle
inline float angle_distance_rad(const float th_rad, const float phi_rad)
{
  return fmodf((th_rad - phi_rad) + PI + TAU, TAU) - PI;
}

inline double deg2rad(double degrees)
{
  return degrees * 4.0 * atan(1.0) / 180.0;
}

/// \brief A struct that holds configuration parameters for the ground filter
class RAY_GROUND_CLASSIFIER_PUBLIC Config
{
public:
  /// \brief Constructor
  /// \param[in] sensor_height_m How high the sensor is off the ground
  /// \param[in] max_local_slope_deg Maximum permissible slope for two ground points within
  ///                                reclass_threshold
  /// \param[in] max_global_slope_deg Maximum permissible slope from base footprint of sensor
  /// \param[in] nonground_retro_thresh_deg How steep consecutive points need to be to retroactively
  ///                                       annotate a point as nonground
  /// \param[in] min_height_thresh_m Local height threshold can be no less than this
  /// \param[in] max_global_height_thresh_m Global height threshold can be no more than this
  /// \param[in] max_last_local_ground_thresh_m Saturation threshold for locality wrt last ground
  ///                                           point (for classifying as ground from nonground)
  /// \param[in] max_provisional_ground_distance_m Max radial distance until provisional ground is
  ///                                              not influenced by next points
  /// \param[in] min_height_m Points below this height in point cloud's frame will not be considered
  ///                         for ground filtering
  /// \param[in] max_height_m Points above this height in point cloud's frame will not be considered
  ///                         for ground filtering
  /// throw std::runtime_error If any of the configuration parameters are inconsistent (e.g. angles
  ///                          are outside of (0, 90), min_* > max_*, etc.)
  Config(
    const float sensor_height_m,
    const float max_local_slope_deg,
    const float max_global_slope_deg,
    const float nonground_retro_thresh_deg,
    const float min_height_thresh_m,
    const float max_global_height_thresh_m,
    const float max_last_local_ground_thresh_m,
    const float max_provisional_ground_distance_m,
    const float min_height_m,
    const float max_height_m);
  /// \brief Get height of sensor off of ground, in meters
  /// \return The value in meters
  float get_sensor_height() const;
  /// \brief Get z value of the ground in sensor frame, -sensor_height
  /// \return The value in meters
  float get_ground_z() const;
  /// \brief Get maximum allowed slope between two nearby points that are both ground
  /// \return The value as a nondimensionalized ratio, rise / run
  float get_max_local_slope() const;
  /// \brief Get maximum allowed slope between a ground point  and the sensor
  /// \return The value as a nondimensionalized ratio, rise / run
  float get_max_global_slope() const;
  /// \brief Get minimum slope at which vertical structure is assumed
  /// \return The value as a nondimensionalized ratio, rise / run
  float get_nonground_retro_thresh() const;
  /// \brief Get minimum value for local height threshold
  /// \return The value in meters
  float get_min_height_thresh() const;
  /// \brief Get maximum value for global height threshold
  /// \return The value in meters
  float get_max_global_height_thresh() const;
  /// \brief Get maximum value for local heigh threshold
  /// \return The value in meters
  float get_max_last_local_ground_thresh() const;
  /// \brief Get maximum influence distance for provisional ground point label
  /// \return The value in meters
  float get_max_provisional_ground_distance() const;
  /// \brief Get minimum height (in sensor frame) of points to classify, points lower than this are
  ///        ignored
  /// \return The value in meters
  float get_min_height() const;
  /// \brief Get maximum height (in sensor frame) of points to classify, points higher than this are
  ///        ignored
  /// \return The value in meters
  float get_max_height() const;

private:
  const float m_ground_z_m;
  const float m_max_local_slope;
  const float m_max_global_slope;
  const float m_nonground_retro_thresh;
  const float m_min_height_thresh_m;
  const float m_max_global_height_thresh_m;
  const float m_max_last_local_ground_thresh_m;
  const float m_max_provisional_ground_distance_m;
  const float m_min_height_m;
  const float m_max_height_m;
};  // class Config

/// \brief This is a simplified point view of a ray. The only information needed is height and
///        projected radial distance from the sensor
class RAY_GROUND_CLASSIFIER_PUBLIC PointXYZIFR
{
  friend bool operator<(const PointXYZIFR & lhs, const PointXYZIFR & rhs) noexcept;

public:
  /// \brief Default constructor
  PointXYZIFR() = default;
  /// \brief Conversion constructor
  /// \param[in] pt The point to convert into a 2D view
  explicit PointXYZIFR(const PointXYZIF & pt);
  /// \brief Getter for radius
  /// \return The projected radial distance
  float get_r() const;
  /// \brief Getter for height
  /// \return The height of the point
  float get_z() const;

  /// \brief Get address-of core point
  /// \return Pointer to internally stored point
  const PointXYZIF * get_point_pointer() const;

private:
  // This could instead be a pointer; I'm pretty sure ownership would work out, but I'm
  // uncomfortable doing it that way (12 vs 22 bytes)
  PointXYZIF m_point;
  float m_r_xy;
};  // class PointXYZIFR

/// \brief Comparison operator for default sorting
/// \param[in] lhs Left hand side of comparison
/// \param[in] rhs Right hand side of comparison
/// \return True if lhs < rhs: if lhs.r < rhs.r, if nearly same radius then lhs.z < rhs.z
inline bool operator<(const PointXYZIFR & lhs, const PointXYZIFR & rhs) noexcept
{
  return (fabsf(lhs.m_r_xy - rhs.m_r_xy) > autoware::common::lidar_utils::FEPS) ?
         (lhs.m_r_xy < rhs.m_r_xy) : (lhs.m_point.z < rhs.m_point.z);
}

using Ray = std::vector<PointXYZIFR>;

/// \brief Simple stateful implementation of ray ground filter:
///        https://github.com/CPFL/Autoware/blob/develop/ros/src/sensing/filters/packages/
///        points_preprocessor/nodes/ray_ground_ray_ground_filter/ray_ground_filter.cpp\#L187
class RAY_GROUND_CLASSIFIER_PUBLIC RayGroundPointClassifier
{
public:
  /// \brief Return codes from is_ground()
  enum class PointLabel : int8_t
  {
    /// \brief Point is ground
    GROUND = 0,
    /// \brief Point is nonground
    NONGROUND = 1,
    /// \brief Last point was nonground. This one is maybe ground. Label is
    /// solidified if terminal or next label is ground, otherwise nonground
    PROVISIONAL_GROUND = -1,
    /// \brief point is so vertical wrt last that last point is also nonground
    RETRO_NONGROUND = 2,
    /// \brief last was provisional ground, but this point is distant
    NONLOCAL_NONGROUND = 3
  };

  /// \brief Constructor
  /// \param[in] config The configuration struct for the filter
  /// \throw std::runtime_error if the configuration is invalid
  explicit RayGroundPointClassifier(const Config & config);

  /// \brief Reinitializes state of filter, should be run before scanning through a ray
  void reset();

  /// \brief Decides if point is ground or not based on locality to last point, max local and
  ///        global slopes, dependent on and updates state
  /// \param[in] pt The point to be classified as ground or not ground
  /// \return PointLabel::GROUND if the point is classified as ground
  ///         PointLabel::NONGROUND if the point is classified as not ground
  ///         PointLabel::RETRO_NONGROUND if the point is classified as not ground and
  ///           the point is so vertical that the last point should also be ground
  /// \throw std::runtime_error if points are not received in order of increasing radius
  PointLabel is_ground(const PointXYZIFR & pt);

  /// \brief Whether a points label is abstractly ground or nonground
  /// \param[in] label the label to check whether or not its ground
  /// \return True if ground or provisionally ground, false if nonground or retro nonground
  static bool label_is_ground(const PointLabel label);

private:
  /// 'state' member variables
  float m_prev_radius_m;
  float m_prev_height_m;
  float m_prev_ground_radius_m;
  float m_prev_ground_height_m;
  bool m_last_was_ground;
  /// complete config struct
  const Config m_config;
};  // RayGroundPointClassifier
}  // namespace ray_ground_classifier
}  // namespace filters
}  // namespace perception
}  // namespace autoware

#endif  // RAY_GROUND_CLASSIFIER__RAY_GROUND_POINT_CLASSIFIER_HPP_
