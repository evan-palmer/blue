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

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "mavros_msgs/msg/override_rc_in.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"
#include "std_srvs/srv/set_bool.hpp"

namespace blue::control
{

class Controller : public rclcpp::Node
{
public:
  virtual ~Controller() = default;  // NOLINT
  Controller(const std::string & node_name, const rclcpp::NodeOptions & options);

  [[nodiscard]] bool running() const;

protected:
  virtual mavros_msgs::msg::OverrideRCIn update();

private:
  void runControlLoopCb();
  void startControlCb(
    const std::shared_ptr<std_srvs::srv::SetBool::Request> & request,
    const std::shared_ptr<std_srvs::srv::SetBool::Response> & response);
  void setOdomPoseCb(geometry_msgs::msg::PoseStamped::ConstSharedPtr pose);

  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr pose_sub_;
  rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;
  rclcpp::Publisher<mavros_msgs::msg::OverrideRCIn>::SharedPtr rc_override_pub_;
  rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr start_control_;
  rclcpp::TimerBase::SharedPtr timer_;

  // BlueROV2 state; these are fused to create the public odometry msg in the desired frame
  geometry_msgs::msg::PoseStamped odom_pose_;
  geometry_msgs::msg::TwistStamped odom_twist_;
  bool running_;
};

}  // namespace blue::control
