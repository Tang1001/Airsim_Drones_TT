#pragma once

#include <vehicles/multirotor/api/MultirotorRpcLibClient.hpp>
#include "FieldBuilding/FieldMathUtil.hpp"
#include "json.hpp"

#define PI 3.14159

namespace sxc { namespace path_planning_controller {

using msr::airlib::MultirotorRpcLibClient;
using msr::airlib::VectorMath;
using Eigen::Vector3d;
using Eigen::Vector2d;
using Eigen::Quaterniond;
using std::vector;
using std::tuple;
using std::make_tuple;
using json = nlohmann::json;

class Firmware{

public:
	std::string dronename="Drone2";

	//Firmware(){}

	Firmware(std::string name)
	{
		dronename = name;
	}

	//是用这个来移动无人机
	//done
	Firmware* moveByQuaternionZAsync(const Quaterniond& q, const double ref_z) {
		float pitch, roll, yaw;
		VectorMath::toEulerianAngle(q.cast<float>(), pitch, roll, yaw);

		const float limit = M_PIf * 6;
		const float anglescaler = std::max(std::max(abs(pitch) / limit, abs(roll) / limit), 1.0f);
		pitch /= anglescaler;
		roll /= anglescaler;

		client->moveByAngleZAsync(pitch, roll, float(ref_z), yaw, max_duration, dronename);
		//client->moveByAngleZAsync(pitch, roll, float(ref_z), yaw, max_duration);
		return this;
	}


	Firmware* connect() {
		client = new MultirotorRpcLibClient();
		client->confirmConnection();
		return this;
	}

	Firmware* disconnect(){
		delete client;
		client = nullptr;
		return this;
	}

	//done
	Firmware* open() {
		//client->enableApiControl(true,"Drone1");
		//client->enableApiControl(true, "Drone2");
		//client->armDisarm(true, "Drone1");
		//client->armDisarm(true, "Drone2");
		//client->takeoffAsync(2.0f, "Drone1")->waitOnLastTask();
		//client->takeoffAsync(2.0f, "Drone2")->waitOnLastTask();
		client->enableApiControl(true, dronename);
		client->armDisarm(true, dronename);
		client->takeoffAsync(2.0f, dronename)->waitOnLastTask();
		return this;
	}

	//done
	Firmware* close(){
		client->landAsync(60,dronename)->waitOnLastTask();
		client->armDisarm(false, dronename);
		client->enableApiControl(false, dronename);
		return this;
	}

	//done
	//Airsim bug
	Firmware* moveToZ(const double z, const double speed) {
		const auto timeout = static_cast<float>(fabs(z - getZ()) / speed + 2);
		
		client->moveToZAsync(static_cast<float>(z), static_cast<float>(speed), timeout, YawMode(), 1, 1, dronename)->waitOnLastTask();

		//client->moveToZAsync(static_cast<float>(z), static_cast<float>(speed), timeout)->waitOnLastTask();
		
		//client->moveToZAsync(static_cast<float>(z), static_cast<float>(speed), timeout, YawMode(),1,1,"Drone1")->waitOnLastTask();
		//client->moveToZAsync(static_cast<float>(z), static_cast<float>(speed), timeout, YawMode(),1,1,"Drone2")->waitOnLastTask();
		return this;
	}

	//done
	//Airsim bug
	Firmware* rotateToYaw(const double yaw) {
		const auto angle = static_cast<float>(yaw * M_PI / 180);
		client->moveByAngleZAsync(0, 0, static_cast<float>(getZ()), angle, 5, dronename)->waitOnLastTask();
		//client->moveByAngleZAsync(0, 0, static_cast<float>(getZ()), angle, 5)->waitOnLastTask();
		return this;
	}

	//done
	//Airsim bug
	Firmware* hoverAsync() {
		client->moveByVelocityZAsync(0,0, static_cast<float>(getZ()), 0.1f, DrivetrainType::MaxDegreeOfFreedom ,YawMode(), dronename);
		//client->moveByVelocityZAsync(0, 0, static_cast<float>(getZ()), 0.1f);
		return this;
	}

	//done
	double getZ() const {
		return client->getMultirotorState(dronename).getPosition().cast<double>().z();
		//return client->getMultirotorState().getPosition().cast<double>().z();
	}

	Vector2d getOtherPosition() const {
		if (dronename == "Drone1")
		{
			return client->getMultirotorState("Drone2").getPosition().cast<double>().head<2>();
		}
		else
		{
			return client->getMultirotorState("Drone1").getPosition().cast<double>().head<2>();
		}
	}

	//done
	void get2DState(Vector2d& pos, double& yaw, Vector2d& v) const {
		auto state = client->getMultirotorState(dronename);
		//auto state = client->getMultirotorState();
		pos = state.getPosition().cast<double>().head<2>();
		v = state.kinematics_estimated.twist.linear.cast<double>().head<2>();
		yaw = static_cast<double>(VectorMath::getYaw(state.getOrientation()));
	}

	//done
	void getBoundary(Vector2d& pos, vector<Vector2d>& boundary) const {
		std::default_random_engine generator;
		std::normal_distribution<double> distribution(0.0, sn*maxDepth);
		const auto ret = client->simGetBoundary(dronename);	//getBoundary 是获取传感器的点
		//const auto ret = client->simGetBoundary();
		pos = ret.pos.cast<double>().head<2>();
		//cout <<pos<< endl;

		Vector2d othpos = Vector2d::Zero();
		auto biancha = loadConfigCoordinate();
		//int num=1;

		/*
		if (dronename == "Drone1")
		{
			cout << "度数开始" << endl;
		}
		*/
		for (const auto& p : ret.boundary) {
			double noise = distribution(generator);
			noise = std::max(noise, -maxDepth/2);
			//const double noise = 0;
			Vector2d b = p.cast<double>().head<2>();
			auto dist = (b - pos).norm();
			dist += noise;		//加上高斯噪声
			b = pos + dist * ((b - pos).normalized());

			/*
			if (dronename == "Drone1")
			{
				cout << num<<" : "<<atan2((b - pos).x(), (b - pos).y()) * 360 / (2 * PI) << "度" << endl;
			}
			*/

			//识别无人机
			/*
			othpos = getOtherPosition();
			othpos << othpos.x() + biancha.x, othpos.y() + biancha.y;
			auto dd = pos - othpos;
			auto dist_pos = dd.norm();	//dist距离
			if (dist_pos < 15)
			{
				double sensor_angle = atan2((b - pos).x(), (b - pos).y()) * 360 / (2 * PI);
				double dd_angle = atan2(dd.x(), dd.y()) * 360 / (2 * PI);
				if (abs(sensor_angle - dd_angle) <= 7.5)
				{
					//cout<<"相差角度:"<< abs(sensor_angle - dd_angle) <<endl;
					//cout << "相差长度:" << dist_pos << endl;
					b = pos + 5 * ((b - pos).normalized());
				}
			}
			*/
			
			//cout << double((b - pos).norm()) << endl;
			boundary.emplace_back(b);

			//num = num + 1;
		}
		/*
		if (dronename == "Drone1")
		{
			cout << "度数结束" << endl;
		}
		*/
	}
	
	
	/*
	Vector3d getPosition() const{
		return client->getMultirotorState("Drone1").getPosition().cast<double>();
	}
	Vector3d getPosition2() const {
		return client->getMultirotorState("Drone2").getPosition().cast<double>();
	}
	*/
	
	
	//done
	Vector3d  getPosition() const {
		return client->getMultirotorState(dronename).getPosition().cast<double>();
	}
	

	//读取两架无人机之间的坐标差
	struct xyConfigCoordinate {
		double x;
		double y;
	};
	xyConfigCoordinate loadConfigCoordinate() const {
		std::ifstream i("C:/Users/zjdell/Documents/AirSim/settings.json");
		json j;
		i >> j;
		xyConfigCoordinate xy;
		xy.x = j.at("Vehicles").at("Drone2").at("X").get<double>();
		xy.y = j.at("Vehicles").at("Drone2").at("Y").get<double>();
		//cout << xy.x << endl;
		//cout << xy.y << endl;
		return xy;
	}

	

	//double ref_speed = 2.0;
	
	double g = 9.8;
	double m = 1.0;//from Airsim
	double k = 1.3f / 4.0f;//from Airsim 空气阻力系数
	double maxDepth = 20.0;
	double sn = 0.05;	//噪声占的比例
	//double ref_z = float(-12.0);

	double positionTolerance = 1.0;
	double velocityTolerance = 1.0;

	int bcStrategy = 1;
	int meshStrategy = 1;
	int boundaryStrategy = 1;
	double maxDist = 2.5 * maxDepth;
	double minGap = 2;
private:

	float max_duration = 0.1f;
	MultirotorRpcLibClient* client = nullptr;

};

}} //namespace