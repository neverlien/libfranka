#include <cmath>
#include <iostream>
#include <vector>

#include <franka/exception.h>
#include <franka/robot.h>

/**
 * @example joint_point_to_point_motion.cpp
 * An example showing how to generate a joint pose motion to a goal position.
 *
 * @warning Before executing this example, make sure there is enough space in front of the robot.
 */

inline int sgn(double x) {
  if (x == 0)
    return 0;
  else
    return (x > 0) ? 1 : -1;
}
void add(const std::array<double, 7>& a,
         const std::array<double, 7>& b,
         std::array<double, 7>* out) {
  for (uint i = 0; i < a.size(); i++) {
    (*out)[i] = a[i] + b[i];
  }
}
void sub(const std::array<double, 7>& a,
         const std::array<double, 7>& b,
         std::array<double, 7>* out) {
  for (uint i = 0; i < a.size(); i++) {
    (*out)[i] = a[i] - b[i];
  }
}
void max_vector(const std::array<double, 7>& a, double* out) {
  (*out) = a[0];
  for (uint i = 0; i < a.size(); i++) {
    if (a[i] > (*out)) {
      (*out) = a[i];
    }
  }
}

void calculation_of_desired_values(double t,
                                   const std::array<double, 7>& delta_q,
                                   const std::array<double, 7>& dq_max,
                                   const std::array<double, 7>& t_1,
                                   const std::array<double, 7>& t_2,
                                   const std::array<double, 7>& t_f,
                                   const std::array<double, 7>& q_1,
                                   double delta_q_motion_finished,
                                   std::array<double, 7>* delta_q_d,
                                   bool* motion_finished_flag) {
  std::array<int, 7> sign_delta_q;
  std::array<double, 7> t_d;
  std::array<double, 7> delta_t_2;
  std::array<bool, 7> joint_motion_finished = {false, false, false, false, false, false, false};
  sub(t_2, t_1, &t_d);
  sub(t_f, t_2, &delta_t_2);

  for (uint joint_index = 0; joint_index < 7; joint_index++) {
    sign_delta_q[joint_index] = sgn(delta_q[joint_index]);
    if (std::abs(delta_q[joint_index]) < delta_q_motion_finished) {  // not moving joint
      (*delta_q_d)[joint_index] = 0;
      joint_motion_finished[joint_index] = true;
    } else {  // Moving joints

      if (t < t_1[joint_index])  // Acceleration phase
      {
        (*delta_q_d)[joint_index] = -1.0 / std::pow(t_1[joint_index], 3) * dq_max[joint_index] *
                                    sign_delta_q[joint_index] * (0.5 * t - t_1[joint_index]) *
                                    std::pow(t, 3);

      } else if (t >= t_1[joint_index] && t < t_2[joint_index]) {  // Constant velocity phase

        (*delta_q_d)[joint_index] =
            q_1[joint_index] +
            (t - t_1[joint_index]) * dq_max[joint_index] * sign_delta_q[joint_index];

      } else if (t >= t_2[joint_index] && t < t_f[joint_index]) {  // Deceleration phase

        (*delta_q_d)[joint_index] =
            delta_q[joint_index] +
            0.5 * (1 / std::pow(delta_t_2[joint_index], 3) *
                       (t - t_1[joint_index] - 2 * delta_t_2[joint_index] - t_d[joint_index]) *
                       std::pow((t - t_1[joint_index] - t_d[joint_index]), 3) +
                   (2 * t - 2 * t_1[joint_index] - delta_t_2[joint_index] - 2 * t_d[joint_index])) *
                dq_max[joint_index] * sign_delta_q[joint_index];
      } else {  // End of Trajectory

        (*delta_q_d)[joint_index] = delta_q[joint_index];
        joint_motion_finished[joint_index] = true;
      }
    }
  }
  *motion_finished_flag =
      (joint_motion_finished[0] && joint_motion_finished[1] && joint_motion_finished[2] &&
       joint_motion_finished[3] && joint_motion_finished[4] && joint_motion_finished[5] &&
       joint_motion_finished[6]);
}
int main(int argc, char** argv) {
  if (argc != 10) {
    std::cerr << "Usage: ./generate_joint_pose_motion <robot-hostname> <goal_position> <speed factor(between zero and 1)>"
              << std::endl;
    return -1;
  }

  try {
    franka::Robot robot(argv[1]);
    std::array<double, 7> q_goal;
    for (uint i = 0; i < 7; i++) {
      q_goal[i] = std::stod(argv[i + 2]);
    }
    double speed_factor = std::stod(argv[9]);
    // Set additional parameters always before the control loop, NEVER in the
    // control loop
    // Set collision behavior:
    robot.setCollisionBehavior(
        {{20.0,  20.0,  20.0,  20.0,  20.0,  20.0,  20.0}},
        {{20.0,  20.0,  20.0,  20.0,  20.0,  20.0,  20.0}},
        {{10.0,  10.0,  10.0,  10.0,  10.0,  10.0,  10.0}},
        {{10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0}},
        {{20.0, 20.0, 20.0, 20.0, 20.0, 20.0}},
        {{20.0, 20.0, 20.0, 20.0, 20.0, 20.0}},
        {{10.0, 10.0, 10.0, 10.0, 10.0, 10.0}},
        {{10.0, 10.0, 10.0, 10.0, 10.0, 10.0}});
    // Reads the start position
    auto start_position = robot.readOnce().q_d;
    std::array<double, 7> q_start;
    for (int i = 0; i < 7; i++) {
      q_start[i] = start_position[i];
    }
    // Initialization

    std::array<double, 7> dq_max = {2.0, 2.0, 2.0, 2.0, 2.5, 2.5, 2.5};
    std::array<double, 7> ddq_max_start = {5, 5, 5, 5, 5, 5, 5};
    std::array<double, 7> ddq_max_goal = {5, 5, 5, 5, 5, 5, 5};
    for (int joint_index = 0; joint_index < 7; joint_index++) {
       dq_max[joint_index] = speed_factor * dq_max[joint_index];
       ddq_max_start[joint_index] = speed_factor * ddq_max_start[joint_index];
       ddq_max_goal[joint_index] = speed_factor * ddq_max_goal[joint_index];
    }
    double delta_q_motion_finished = 0.000001;
    double time = 0.0;
    std::array<double, 7> dq_max_reach = dq_max;
    std::array<double, 7> dq_max_sync;
    std::array<double, 7> t_1 = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, 7> delta_t_2 = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, 7> t_f = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, 7> t_1_sync = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, 7> t_2_sync = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, 7> delta_t_2_sync = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, 7> t_f_sync = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, 7> q_1 = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, 7> delta_q;
    std::array<double, 7> delta_q_d;

    // double* delta_q_d[7];// = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    bool motion_finished_flag;
    sub(q_goal, q_start, &delta_q);
    int sign_delta_q[7];
    for (int joint_index = 0; joint_index < 7; joint_index++) {
      sign_delta_q[joint_index] = sgn(delta_q[joint_index]);
      if (std::abs(delta_q[joint_index]) > delta_q_motion_finished) {  // moving joints
        if (std::abs(delta_q[joint_index]) <
            (3 / 4 * (std::pow(dq_max[joint_index], 2) / ddq_max_start[joint_index]) +
             3 / 4 * (std::pow(dq_max[joint_index], 2) / ddq_max_goal[joint_index]))) {
          dq_max_reach[joint_index] =
              std::sqrt(4 / 3 * delta_q[joint_index] * sign_delta_q[joint_index] *
                        (ddq_max_start[joint_index] * ddq_max_goal[joint_index]) /
                        (ddq_max_start[joint_index] + ddq_max_goal[joint_index]));
        }
        t_1[joint_index] = 1.5 * dq_max_reach[joint_index] / ddq_max_start[joint_index];
        delta_t_2[joint_index] = 1.5 * dq_max_reach[joint_index] / ddq_max_goal[joint_index];
        t_f[joint_index] = t_1[joint_index] / 2 + delta_t_2[joint_index] / 2 +
                           std::abs(delta_q[joint_index]) / dq_max_reach[joint_index];
      }
    }
    double max_t_f;
    max_vector(t_f, &max_t_f);
    for (int joint_index = 0; joint_index < 7; joint_index++) {
      if (std::abs(delta_q[joint_index]) > delta_q_motion_finished) {  // moving joints
        double a = 1.5 / 2.0 * (ddq_max_goal[joint_index] + ddq_max_start[joint_index]);
        double b = -1.0 * max_t_f * ddq_max_goal[joint_index] * ddq_max_start[joint_index];
        double c =
            std::abs(delta_q[joint_index]) * ddq_max_goal[joint_index] * ddq_max_start[joint_index];
        double delta = b * b - 4.0 * a * c;
        dq_max_sync[joint_index] = (-1 * b - std::sqrt(delta)) / (2.0 * a);
        t_1_sync[joint_index] = 1.5 * dq_max_sync[joint_index] / ddq_max_start[joint_index];
        delta_t_2_sync[joint_index] = 1.5 * dq_max_sync[joint_index] / ddq_max_goal[joint_index];
        t_f_sync[joint_index] = t_1_sync[joint_index] / 2 + delta_t_2_sync[joint_index] / 2 +
                                std::abs(delta_q[joint_index] / dq_max_sync[joint_index]);
        t_2_sync[joint_index] = t_f_sync[joint_index] - delta_t_2_sync[joint_index];
        q_1[joint_index] =
            dq_max_sync[joint_index] * sign_delta_q[joint_index] * (0.5 * t_1_sync[joint_index]);
      }
    }
    robot.control([=, &time, &delta_q_d, &motion_finished_flag](
                      const franka::RobotState&,
                      franka::Duration time_step) -> franka::JointPositions {

      time += time_step.s();

      calculation_of_desired_values(time, delta_q, dq_max_sync, t_1_sync, t_2_sync, t_f_sync, q_1,
                                    delta_q_motion_finished, &delta_q_d, &motion_finished_flag);

      if (motion_finished_flag == true) {
        std::cout << std::endl << "Finished motion, shutting down example" << std::endl;
        return franka::Stop;
      }

      return {{q_start[0] + delta_q_d[0], q_start[1] + delta_q_d[1], q_start[2] + delta_q_d[2],
               q_start[3] + delta_q_d[3], q_start[4] + delta_q_d[4], q_start[5] + delta_q_d[5],
               q_start[6] + delta_q_d[6]}};
    });
  } catch (const franka::Exception& e) {
    std::cout << e.what() << std::endl;
    return -1;
  }

  return 0;
}
