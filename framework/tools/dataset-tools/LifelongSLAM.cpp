/*

 Copyright (c) 2017 University of Edinburgh, Imperial College, University of Manchester.
 Developed in the PAMELA project, EPSRC Programme Grant EP/K008730/1

 This code is licensed under the MIT License.

 */


#include "include/LifelongSLAM.h"

#include <io/SLAMFile.h>
#include <io/SLAMFrame.h>
#include <io/sensor/AccelerometerSensor.h>
#include <io/sensor/GyroSensor.h>
#include <io/sensor/OdomSensor.h>
#include <io/sensor/GroundTruthSensor.h>
#include <io/sensor/PointCloudSensor.h>
#include <io/format/PointCloud.h>
#include <Eigen/Eigen>

#include <yaml-cpp/yaml.h>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

#include <iostream>
#include <fstream>

using namespace slambench::io ;

#include <cstdio>
#include <dirent.h>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

using namespace slambench::io;

void Dijkstra(int n, int s, std::vector<std::vector<int> > G, std::vector<bool>& vis, std::vector<int>& d, std::vector<int>& pre)
{
       fill(d.begin(), d.end(), INF);
       for (int i = 0; i < n; ++i)
              pre[i] = i;

       d[s] = 0;
       for (int i = 0; i < n; ++i)
       {
              int u = -1;
              int MIN = INF;
              for (int j = 0; j < n; ++j)
			  {
                     if (vis[j] == false && d[j] < MIN)
                     {
                           u = j;
                           MIN = d[j];
                     }
              }

              if (u == -1)
                     return;
              vis[u] = true;
              for (int v = 0; v < n; ++v)
              {
                     if (vis[v] == false && d[u] + G[u][v] < d[v]) {
                           d[v] = d[u] + G[u][v];
                           pre[v] = u;
                     }
              }
       }
}

void DFSPrint(int s, int v, std::vector<int> pre, std::vector<int> &result)
{
       if (v == s) {
              result.push_back(s);
              return;
       }
       DFSPrint(s, pre[v], pre, result);
       result.push_back(v);
}

Eigen::Matrix4f slambench::io::compute_trans_matrix(std::string input_name_1, std::string input_name_2, std::string filename)
{
    YAML::Node f = YAML::LoadFile(filename.c_str());

    std::map<std::string, int> name_to_index;
    std::map<trans_direction, Eigen::Matrix4f> transations;
    int num = 0;
    for(int i = 0; i < f["trans_matrix"].size(); i++) {
        int index_1, index_2;
        std::string name_1 = f["trans_matrix"][i]["parent_frame"].as<std::string>();
        if(name_to_index.count(name_1) == 0){
            name_to_index[name_1] = num;
            num++;
        }
        std::string name_2 = f["trans_matrix"][i]["child_frame"].as<std::string>();
        if(name_to_index.count(name_2) == 0){
            name_to_index[name_2] = num;
            num++;
        }
        index_1 = name_to_index[name_1];
        index_2 = name_to_index[name_2];
        trans_direction dir(index_1, index_2);
        Eigen::Matrix4f trans_matrix;
        trans_matrix << f["trans_matrix"][i]["matrix"]["data"][0].as<float>(),
                f["trans_matrix"][i]["matrix"]["data"][1].as<float>(),
                f["trans_matrix"][i]["matrix"]["data"][2].as<float>(),
                f["trans_matrix"][i]["matrix"]["data"][3].as<float>(),
                f["trans_matrix"][i]["matrix"]["data"][4].as<float>(),
                f["trans_matrix"][i]["matrix"]["data"][5].as<float>(),
                f["trans_matrix"][i]["matrix"]["data"][6].as<float>(),
                f["trans_matrix"][i]["matrix"]["data"][7].as<float>(),
                f["trans_matrix"][i]["matrix"]["data"][8].as<float>(),
                f["trans_matrix"][i]["matrix"]["data"][9].as<float>(),
                f["trans_matrix"][i]["matrix"]["data"][10].as<float>(),
                f["trans_matrix"][i]["matrix"]["data"][11].as<float>(),
                f["trans_matrix"][i]["matrix"]["data"][12].as<float>(),
                f["trans_matrix"][i]["matrix"]["data"][13].as<float>(),
                f["trans_matrix"][i]["matrix"]["data"][14].as<float>(),
                f["trans_matrix"][i]["matrix"]["data"][15].as<float>();//problem
        transations[dir] = trans_matrix;
        trans_direction dir_inv(index_2, index_1);
        transations[dir_inv] = trans_matrix.inverse();
    }

       int s = name_to_index[input_name_1];
       int v = name_to_index[input_name_2];
       int n = name_to_index.size();

       std::vector<std::vector<int> > G;
       std::vector<int> g;
       for(int j = 0; j < n; j++) {
           g.push_back(INF);
       }
       for(int i = 0; i < n; i++) {
           G.push_back(g);
       }
       for(std::map<trans_direction, Eigen::Matrix4f>::iterator it = transations.begin(); it != transations.end(); ++it) {
           trans_direction index_pair = it->first;
           G[index_pair.first][index_pair.second] = 1;
       }

       std::vector<bool> vis(n);
       std::vector<int> d(n);
       std::vector<int> pre(n);
       Dijkstra(n, s, G, vis, d, pre);
       std::vector<int> result;
       DFSPrint(s, v, pre, result);
 
       Eigen::Matrix4f result_matrix = Eigen::MatrixXf::Identity(4,4);
       for(std::vector<int>::iterator it = result.begin(); it != (result.end() - 1); it++ ){
            trans_direction dir(*it, *(it + 1));
            Eigen::Matrix4f trans = transations[dir];
            result_matrix = result_matrix * trans;
       }
	//    std::cout<<input_name_1<<" to "<<input_name_2<<std::endl;
	//    std::cout<<result_matrix<<std::endl;
       return result_matrix;
}


bool analyseLifelongSLAMFolder(const std::string &dirname) {

	static const std::vector<std::string> requirements = {
			"d400_accelerometer.txt",
            "d400_gyroscope.txt",
			"t265_accelerometer.txt",
            "t265_gyroscope.txt",
            "odom.txt",
			"color.txt",
			"color",
			"depth.txt",
			"depth",
			"aligned_depth.txt",
			"aligned_depth",
			"Fisheye_1.txt",
			"Fisheye_1",
			"Fisheye_2.txt",
			"Fisheye_2",
			"GroundTruth.txt",
			"sensors.yaml",
			"trans_matrix.yaml"
	};

	try {
		if ( !boost::filesystem::exists( dirname ) ) return false;

		boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
		for ( auto requirement : requirements ) {
			bool seen = false;

			for ( boost::filesystem::directory_iterator itr( dirname ); itr != end_itr; ++itr ) {
				if (requirement == itr->path().filename()) {
					seen = true;
				}
			}

			if (!seen) {
				std::cout << "File not found: <dataset_dir>/" << requirement << std::endl;
				return false;
			}
		}
	} catch (boost::filesystem::filesystem_error& e)  {
		std::cerr << "I/O Error with directory " << dirname << std::endl;
		std::cerr << e.what() << std::endl;
		return false;
	}

	return true;
}


bool loadLifelongSLAMDepthData(const std::string &dirname, const std::string &sensor_name, SLAMFile &file, const DepthSensor::disparity_params_t &disparity_params,const DepthSensor::disparity_type_t &disparity_type, bool aligned_depth = false) {

	std::string filename = dirname + "/sensors.yaml";
	YAML::Node f = YAML::LoadFile(filename.c_str());
    

	DepthSensor *depth_sensor = new DepthSensor("Depth");

	depth_sensor->Index = 0;
	depth_sensor->Width = f[sensor_name]["width"].as<int>();
	depth_sensor->Height = f[sensor_name]["height"].as<int>();
	depth_sensor->FrameFormat = frameformat::Raster;
	depth_sensor->PixelFormat = pixelformat::D_I_16;
	depth_sensor->DisparityType = disparity_type;
	depth_sensor->CopyDisparityParams(disparity_params);
	depth_sensor->Description = "Depth";
	Eigen::Matrix4f pose = compute_trans_matrix("RigidBody1", sensor_name, dirname + "/trans_matrix.yaml");
	depth_sensor->CopyPose(pose);

	depth_sensor->Intrinsics[0] = f[sensor_name]["intrinsics"]["data"][0].as<float>() / depth_sensor->Width;
	depth_sensor->Intrinsics[1] = f[sensor_name]["intrinsics"]["data"][1].as<float>() / depth_sensor->Height;
	depth_sensor->Intrinsics[2] = f[sensor_name]["intrinsics"]["data"][2].as<float>() / depth_sensor->Width;
	depth_sensor->Intrinsics[3] = f[sensor_name]["intrinsics"]["data"][3].as<float>() / depth_sensor->Height;

	if (f[sensor_name]["distortion_model"].as<std::string>() == "radial-tangential") {
		depth_sensor->DistortionType = slambench::io::CameraSensor::RadialTangential;
		depth_sensor->RadialTangentialDistortion[0] = f[sensor_name]["distortion_coefficients"]["data"][0].as<float>();
		depth_sensor->RadialTangentialDistortion[1] = f[sensor_name]["distortion_coefficients"]["data"][1].as<float>();
		depth_sensor->RadialTangentialDistortion[2] = f[sensor_name]["distortion_coefficients"]["data"][2].as<float>();
		depth_sensor->RadialTangentialDistortion[3] = f[sensor_name]["distortion_coefficients"]["data"][3].as<float>();
		depth_sensor->RadialTangentialDistortion[4] = 0;//??
	}
	depth_sensor->Index = file.Sensors.size();
	depth_sensor->Rate = f[sensor_name]["fps"].as<float>();

	file.Sensors.AddSensor(depth_sensor);

	std::string line;
	std::string txt_name;
	if (aligned_depth) {
		txt_name = "aligned_depth.txt";
	} else {
		txt_name = "depth.txt";
	}
	std::ifstream infile(dirname + "/" + txt_name);
	boost::smatch match;

	while (std::getline(infile, line)){


		if (line.size() == 0) {
			continue;
		} else if (boost::regex_match(line,match,boost::regex("^\\s*#.*$"))) {
			continue;
		} else if (boost::regex_match(line,match,boost::regex("^([0-9]+)[.]([0-9]+)\\s+(.*)$"))) {

		  int timestampS = std::stoi(match[1]);
		  int timestampNS = std::stoi(match[2]) *  std::pow ( 10, 9 - match[2].length());
		  std::string depthfilename = match[3];

		  ImageFileFrame *depth_frame = new ImageFileFrame();
		  depth_frame->FrameSensor  = depth_sensor;
		  depth_frame->Timestamp.S  = timestampS;
		  depth_frame->Timestamp.Ns = timestampNS;

		  std::stringstream frame_name;
		  frame_name << dirname << "/" << depthfilename ;
		  depth_frame->Filename = frame_name.str();

		  if(access(depth_frame->Filename.c_str(), F_OK) < 0) {
				printf("No depth image for frame (%s)\n", frame_name.str().c_str());
				perror("");
				return false;
		  }

		  file.AddFrame(depth_frame);



		} else {
		  std::cerr << "Unknown line:" << line << std::endl;
		  return false;
		}
	}
	return true;
}


bool loadLifelongSLAMRGBData(const std::string &dirname, const std::string &sensor_name, SLAMFile &file) {
	std::string filename = dirname + "/sensors.yaml";
	YAML::Node f = YAML::LoadFile(filename.c_str());
 
	CameraSensor *rgb_sensor = new CameraSensor("RGB", CameraSensor::kCameraType);
	rgb_sensor->Index = 0;
	rgb_sensor->Width = f[sensor_name]["width"].as<int>();
	rgb_sensor->Height = f[sensor_name]["height"].as<int>();
	rgb_sensor->FrameFormat = frameformat::Raster;
	rgb_sensor->PixelFormat = pixelformat::RGB_III_888;
	rgb_sensor->Description = "RGB";
	Eigen::Matrix4f pose = compute_trans_matrix("RigidBody1", sensor_name, dirname + "/trans_matrix.yaml");
	rgb_sensor->CopyPose(pose);

	rgb_sensor->Intrinsics[0] = f[sensor_name]["intrinsics"]["data"][0].as<float>() / rgb_sensor->Width;
	rgb_sensor->Intrinsics[1] = f[sensor_name]["intrinsics"]["data"][1].as<float>() / rgb_sensor->Height;
	rgb_sensor->Intrinsics[2] = f[sensor_name]["intrinsics"]["data"][2].as<float>() / rgb_sensor->Width;
	rgb_sensor->Intrinsics[3] = f[sensor_name]["intrinsics"]["data"][3].as<float>() / rgb_sensor->Height;

	if (f[sensor_name]["distortion_model"].as<std::string>() == "radial-tangential") {
		rgb_sensor->DistortionType = slambench::io::CameraSensor::RadialTangential;
		rgb_sensor->RadialTangentialDistortion[0] = f[sensor_name]["distortion_coefficients"]["data"][0].as<float>();
		rgb_sensor->RadialTangentialDistortion[1] = f[sensor_name]["distortion_coefficients"]["data"][1].as<float>();
		rgb_sensor->RadialTangentialDistortion[2] = f[sensor_name]["distortion_coefficients"]["data"][2].as<float>();
		rgb_sensor->RadialTangentialDistortion[3] = f[sensor_name]["distortion_coefficients"]["data"][3].as<float>();
		rgb_sensor->RadialTangentialDistortion[4] = 0;//??
	}
	rgb_sensor->Index =file.Sensors.size();
	rgb_sensor->Rate = f[sensor_name]["fps"].as<float>();

	file.Sensors.AddSensor(rgb_sensor);

	std::string line;

	std::ifstream infile(dirname + "/" + "color.txt");

	boost::smatch match;

	while (std::getline(infile, line)){


		if (line.size() == 0) {
			continue;
		} else if (boost::regex_match(line,match,boost::regex("^\\s*#.*$"))) {
			continue;
		} else if (boost::regex_match(line,match,boost::regex("^([0-9]+)[.]([0-9]+)\\s+(.*)$"))) {

		  int timestampS = std::stoi(match[1]);
		  int timestampNS = std::stoi(match[2]) *  std::pow ( 10, 9 - match[2].length());
		  std::string rgbfilename = match[3];

		  ImageFileFrame *rgb_frame = new ImageFileFrame();
		  rgb_frame->FrameSensor = rgb_sensor;
		  rgb_frame->Timestamp.S  = timestampS;
		  rgb_frame->Timestamp.Ns = timestampNS;

		  std::stringstream frame_name;
		  frame_name << dirname << "/" << rgbfilename ;
		  rgb_frame->Filename = frame_name.str();

		  if(access(rgb_frame->Filename.c_str(), F_OK) < 0) {
		    printf("No RGB image for frame (%s)\n", frame_name.str().c_str());
		    perror("");
		    return false;
		  }

		  file.AddFrame(rgb_frame);

		} else {
		  std::cerr << "Unknown line:" << line << std::endl;
		  return false;
		}

	}
	return true;
}

bool loadLifelongSLAMGreyData(const std::string &dirname, const std::string &sensor_name, const std::string &source_name, SLAMFile &file) {
	std::string filename = dirname + "/sensors.yaml";
	YAML::Node f = YAML::LoadFile(filename.c_str());

	CameraSensor *grey_sensor = new CameraSensor(sensor_name, CameraSensor::kCameraType);
	grey_sensor->Index = 0;
	grey_sensor->Width = f[sensor_name]["width"].as<int>();
	grey_sensor->Height = f[sensor_name]["height"].as<int>();
	grey_sensor->FrameFormat = frameformat::Raster;
	grey_sensor->PixelFormat = pixelformat::G_I_8;
	grey_sensor->Description = "Grey";
	Eigen::Matrix4f pose = compute_trans_matrix("RigidBody1", sensor_name, dirname + "/trans_matrix.yaml");
	grey_sensor->CopyPose(pose);

	grey_sensor->Intrinsics[0] = f[sensor_name]["intrinsics"]["data"][0].as<float>() / grey_sensor->Width;
	grey_sensor->Intrinsics[1] = f[sensor_name]["intrinsics"]["data"][1].as<float>() / grey_sensor->Height;
	grey_sensor->Intrinsics[2] = f[sensor_name]["intrinsics"]["data"][2].as<float>() / grey_sensor->Width;
	grey_sensor->Intrinsics[3] = f[sensor_name]["intrinsics"]["data"][3].as<float>() / grey_sensor->Height;

	if (f[sensor_name]["distortion_model"].as<std::string>() == "radial-tangential") {
		grey_sensor->DistortionType = slambench::io::CameraSensor::RadialTangential;
	} else if (f[sensor_name]["distortion_model"].as<std::string>() == "Kannala-Brandt") {
		grey_sensor->DistortionType = slambench::io::CameraSensor::KannalaBrandt;
	}
		
	grey_sensor->RadialTangentialDistortion[0] = f[sensor_name]["distortion_coefficients"]["data"][0].as<float>();
	grey_sensor->RadialTangentialDistortion[1] = f[sensor_name]["distortion_coefficients"]["data"][1].as<float>();
	grey_sensor->RadialTangentialDistortion[2] = f[sensor_name]["distortion_coefficients"]["data"][2].as<float>();
	grey_sensor->RadialTangentialDistortion[3] = f[sensor_name]["distortion_coefficients"]["data"][3].as<float>();
	grey_sensor->RadialTangentialDistortion[4] = 0;
	grey_sensor->Index =file.Sensors.size();
	grey_sensor->Rate = f[sensor_name]["fps"].as<float>();

	file.Sensors.AddSensor(grey_sensor);

	std::cout<<"Grey camera sensor created..."<<std::endl;

	std::string line;

	std::ifstream infile(dirname + "/" + source_name + ".txt");

	boost::smatch match;

	while (std::getline(infile, line)){


		if (line.size() == 0) {
			continue;
		} else if (boost::regex_match(line,match,boost::regex("^\\s*#.*$"))) {
			continue;
		} else if (boost::regex_match(line,match,boost::regex("^([0-9]+)[.]([0-9]+)\\s+(.*)$"))) {

		  int timestampS = std::stoi(match[1]);
		  int timestampNS = std::stoi(match[2]) *  std::pow ( 10, 9 - match[2].length());
		  std::string rgbfilename = match[3];

		  ImageFileFrame *grey_frame = new ImageFileFrame();
		  grey_frame->FrameSensor = grey_sensor;
		  grey_frame->Timestamp.S  = timestampS;
		  grey_frame->Timestamp.Ns = timestampNS;

		  std::stringstream frame_name;
		  frame_name << dirname << "/" << rgbfilename ;
		  grey_frame->Filename = frame_name.str();

		  if(access(grey_frame->Filename.c_str(), F_OK) < 0) {
		    printf("No RGB image for frame (%s)\n", frame_name.str().c_str());
		    perror("");
		    return false;
		  }

		  file.AddFrame(grey_frame);

		} else {
		  std::cerr << "Unknown line:" << line << std::endl;
		  return false;
		}

	}
	return true;
}


bool loadLifelongSLAMGroundTruthData(const std::string &dirname , SLAMFile &file) {

	GroundTruthSensor *gt_sensor = new GroundTruthSensor("GroundTruth");
	gt_sensor->Index = file.Sensors.size();
	gt_sensor->Description = "GroundTruthSensor";
	file.Sensors.AddSensor(gt_sensor);

	if(!gt_sensor) {
		std::cout << "gt sensor not found..." << std::endl;
		return false;
	} else {
		std::cout << "gt sensor created..." << std::endl;
	}


	std::string line;

	boost::smatch match;
	std::ifstream infile(dirname + "/" + "GroundTruth.txt");

	while (std::getline(infile, line)){
		if (line.size() == 0) {
			continue;
		} else if (boost::regex_match(line,match,boost::regex("^\\s*#.*$"))) {
			continue;
		} else if (boost::regex_match(line,match,boost::regex("^([0-9]+)[.]([0-9]+)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s*$"))) {
            
          int timestampS = std::stoi(match[1]);
		  int timestampNS = std::stoi(match[2]) *  std::pow ( 10, 9 - match[2].length());
          std::string temp3 = match[3], temp4 = match[4], temp5 = match[5], temp6 = match[6], temp7 = match[7], temp8 = match[8], temp9 = match[9];
          char *p3=(char*)temp3.data();
          char *p4=(char*)temp4.data();
          char *p5=(char*)temp5.data();
          char *p6=(char*)temp6.data();
          char *p7=(char*)temp7.data();
          char *p8=(char*)temp8.data();
          char *p9=(char*)temp9.data();

		  float tx =  std::atof(p3);
		  float ty =  std::atof(p4);
		  float tz =  std::atof(p5);
		  float QX =  std::atof(p6);
		  float QY =  std::atof(p7);
		  float QZ =  std::atof(p8);
		  float QW =  std::atof(p9);

		  Eigen::Matrix3f rotationMat = Eigen::Quaternionf(QW,QX,QY,QZ).toRotationMatrix();
		  Eigen::Matrix4f pose = Eigen::Matrix4f::Identity();
		  pose.block(0,0,3,3) = rotationMat;

		  pose.block(0,3,3,1) << tx , ty , tz;


		  SLAMInMemoryFrame *gt_frame = new SLAMInMemoryFrame();
		  gt_frame->FrameSensor = gt_sensor;
		  gt_frame->Timestamp.S  = timestampS;
		  gt_frame->Timestamp.Ns = timestampNS;
		  gt_frame->Data = malloc(gt_frame->GetSize());


		  memcpy(gt_frame->Data,pose.data(),gt_frame->GetSize());

		  file.AddFrame(gt_frame);


		} else {
		  std::cerr << "Unknown line:" << line << std::endl;
		  return false;
		}


	}
	return true;
}


bool loadLifelongSLAMAccelerometerData(const std::string &dirname, const std::string &sensor_name, SLAMFile &file) {
	std::string filename = dirname + "/sensors.yaml";
	YAML::Node f = YAML::LoadFile(filename.c_str());

	AccelerometerSensor *accelerometer_sensor = new AccelerometerSensor(sensor_name);
	accelerometer_sensor->Index = file.Sensors.size();
	accelerometer_sensor->Description = "AccelerometerSensor";

	accelerometer_sensor->Rate = f[sensor_name]["fps"].as<float>();

	accelerometer_sensor->AcceleratorNoiseDensity = 2.0000e-3;
	accelerometer_sensor->AcceleratorDriftNoiseDensity = 4.0e-5;
	accelerometer_sensor->AcceleratorBiasDiffusion = 3.0000e-3;
	accelerometer_sensor->AcceleratorSaturation = 176.0;

	Eigen::Matrix4f pose = compute_trans_matrix("RigidBody1", sensor_name, dirname + "/trans_matrix.yaml");
	accelerometer_sensor->CopyPose(pose);

	file.Sensors.AddSensor(accelerometer_sensor);

	if(!accelerometer_sensor) {
		std::cout << "accelerometer_sensor not found..." << std::endl;
		return false;
	}else {
		std::cout << "accelerometer_sensor created..." << std::endl;
	}


	std::string line;

	  boost::smatch match;
	  std::ifstream infile(dirname + "/" + sensor_name + ".txt");

	while (std::getline(infile, line)){

		if (line.size() == 0) {
			continue;
		} else if (boost::regex_match(line,match,boost::regex("^\\s*#.*$"))) {
			continue;
		} else if (boost::regex_match(line,match,boost::regex("^([0-9]+)[.]([0-9]+)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s*$"))) {

		 int timestampS = std::stoi(match[1]);
		  int timestampNS = std::stoi(match[2]) *  std::pow ( 10, 9 - match[2].length());
          std::string temp3 = match[3], temp4 = match[4], temp5 = match[5];
          char *p3=(char*)temp3.data();
          char *p4=(char*)temp4.data();
          char *p5=(char*)temp5.data();

		  float ax =  std::atof(p3);
		  float ay =  std::atof(p4);
		  float az =  std::atof(p5);

		  SLAMInMemoryFrame *accelerometer_frame = new SLAMInMemoryFrame();
		  accelerometer_frame->FrameSensor = accelerometer_sensor;
		  accelerometer_frame->Timestamp.S  = timestampS;
		  accelerometer_frame->Timestamp.Ns = timestampNS;
		  accelerometer_frame->Data = malloc(accelerometer_frame->GetSize());
		  ((float*)accelerometer_frame->Data)[0] = ax;
		  ((float*)accelerometer_frame->Data)[1] = ay;
		  ((float*)accelerometer_frame->Data)[2] = az;

		  file.AddFrame(accelerometer_frame);


		} else {
		  std::cerr << "Unknown line:" << line << std::endl;
		  return false;
		}


	}
	return true;
}


bool loadLifelongSLAMGyroData(const std::string &dirname, const std::string &sensor_name, SLAMFile &file) {
	std::string filename = dirname + "/sensors.yaml";
	YAML::Node f = YAML::LoadFile(filename.c_str());

	GyroSensor *gyro_sensor = new GyroSensor(sensor_name);
	gyro_sensor->Index = file.Sensors.size();
	gyro_sensor->Description = "GyroSensor";

	gyro_sensor->Rate = f[sensor_name]["fps"].as<float>();

	gyro_sensor->GyroscopeNoiseDensity = 1.6968e-04;
	gyro_sensor->GyroscopeDriftNoiseDensity = 4.0e-6;
	gyro_sensor->GyroscopeBiasDiffusion = 1.9393e-05;
	gyro_sensor->GyroscopeSaturation   =   7.8;

	Eigen::Matrix4f pose = compute_trans_matrix("RigidBody1", sensor_name, dirname + "/trans_matrix.yaml");
	gyro_sensor->CopyPose(pose);

	file.Sensors.AddSensor(gyro_sensor);

	if(!gyro_sensor) {
		std::cout << "gyro_sensor not found..." << std::endl;
		return false;
	}else {
		std::cout << "gyro_sensor created..." << std::endl;
	}


	std::string line;

	  boost::smatch match;
	  std::ifstream infile(dirname + "/" + sensor_name + ".txt");

	while (std::getline(infile, line)){

		if (line.size() == 0) {
			continue;
		} else if (boost::regex_match(line,match,boost::regex("^\\s*#.*$"))) {
			continue;
		} else if (boost::regex_match(line,match,boost::regex("^([0-9]+)[.]([0-9]+)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s*$"))) {

		 int timestampS = std::stoi(match[1]);
		  int timestampNS = std::stoi(match[2]) *  std::pow ( 10, 9 - match[2].length());

          std::string temp3 = match[3], temp4 = match[4], temp5 = match[5];
          char *p3=(char*)temp3.data();
          char *p4=(char*)temp4.data();
          char *p5=(char*)temp5.data();

		  float wx =  std::atof(p3);
		  float wy =  std::atof(p4);
		  float wz =  std::atof(p5);

		  SLAMInMemoryFrame *gyro_frame = new SLAMInMemoryFrame();
		  gyro_frame->FrameSensor = gyro_sensor;
		  gyro_frame->Timestamp.S  = timestampS;
		  gyro_frame->Timestamp.Ns = timestampNS;
		  gyro_frame->Data = malloc(gyro_frame->GetSize());
		  ((float*)gyro_frame->Data)[0] = wx;
		  ((float*)gyro_frame->Data)[1] = wy;
		  ((float*)gyro_frame->Data)[2] = wz;

		  file.AddFrame(gyro_frame);


		} else {
		  std::cerr << "Unknown line:" << line << std::endl;
		  return false;
		}


	}
	return true;
}

bool loadLifelongSLAMOdomData(const std::string &dirname, const std::string &sensor_name, SLAMFile &file) {

	OdomSensor *odom_sensor = new OdomSensor(sensor_name);
	odom_sensor->Index = file.Sensors.size();
	odom_sensor->Description = "OdomSensor";

	Eigen::Matrix4f pose = compute_trans_matrix("RigidBody1", sensor_name, dirname + "/trans_matrix.yaml");
	odom_sensor->CopyPose(pose);

	file.Sensors.AddSensor(odom_sensor);

	if(!odom_sensor) {
		std::cout << "odom_sensor not found..." << std::endl;
		return false;
	}else {
		std::cout << "odom_sensor created..." << std::endl;
	}


	std::string line;

	  boost::smatch match;
	  std::ifstream infile(dirname + "/" + "odom.txt");

	while (std::getline(infile, line)){

		if (line.size() == 0) {
			continue;
		} else if (boost::regex_match(line,match,boost::regex("^\\s*#.*$"))) {
			continue;
		} else if (boost::regex_match(line,match,boost::regex("^([0-9]+)[.]([0-9]+)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s+([+-]?[0-9]+(.[0-9]+([Ee]([+-]?)[0-9]+)?)?)\\s*$"))) {

		int timestampS = std::stoi(match[1]);
		int timestampNS = std::stoi(match[2]) *  std::pow ( 10, 9 - match[2].length());
        std::string temp3 = match[3], temp4 = match[4], temp5 = match[5], temp6 = match[6], temp7 = match[7], temp8 = match[8], temp9 = match[9],
                    temp10 = match[10], temp11 = match[11], temp12 = match[12], temp13 = match[13], temp14 = match[14], temp15 = match[15];
          char *p3=(char*)temp3.data();
          char *p4=(char*)temp4.data();
          char *p5=(char*)temp5.data();
          char *p6=(char*)temp6.data();
          char *p7=(char*)temp7.data();
          char *p8=(char*)temp8.data();
          char *p9=(char*)temp9.data();
          char *p10=(char*)temp10.data();
          char *p11=(char*)temp11.data();
          char *p12=(char*)temp12.data();
          char *p13=(char*)temp13.data();
          char *p14=(char*)temp14.data();
          char *p15=(char*)temp15.data();

		  float px =  std::atof(p3);
		  float py =  std::atof(p4);
		  float pz =  std::atof(p5);
		  float ox =  std::atof(p6);
		  float oy =  std::atof(p7);
		  float oz =  std::atof(p8);
		  float ow =  std::atof(p9);
          float lx =  std::atof(p10);
		  float ly =  std::atof(p11);
		  float lz =  std::atof(p12);
          float ax =  std::atof(p13);
		  float ay =  std::atof(p14);
		  float az =  std::atof(p15);


		  SLAMInMemoryFrame *odom_frame = new SLAMInMemoryFrame();
		  odom_frame->FrameSensor = odom_sensor;
		  odom_frame->Timestamp.S  = timestampS;
		  odom_frame->Timestamp.Ns = timestampNS;
		  odom_frame->Data = malloc(odom_frame->GetSize());
		  ((float*)odom_frame->Data)[0] = px;
		  ((float*)odom_frame->Data)[1] = py;
		  ((float*)odom_frame->Data)[2] = pz;
          ((float*)odom_frame->Data)[3] = ox;
		  ((float*)odom_frame->Data)[4] = oy;
		  ((float*)odom_frame->Data)[5] = oz;
          ((float*)odom_frame->Data)[6] = ow;
          ((float*)odom_frame->Data)[7] = lx;
		  ((float*)odom_frame->Data)[8] = ly;
		  ((float*)odom_frame->Data)[9] = lz;
          ((float*)odom_frame->Data)[10] = ax;
		  ((float*)odom_frame->Data)[11] = ay;
		  ((float*)odom_frame->Data)[12] = az;

		  file.AddFrame(odom_frame);


		} else {
		  std::cerr << "Unknown line:" << line << std::endl;
		  return false;
		}


	}
	return true;
}


SLAMFile* LifelongSLAMReader::GenerateSLAMFile () {

	if(!(grey || color || depth || aligned_depth || fisheye1 ||fisheye2)) {
		std::cerr <<  "No sensors defined\n";
		return nullptr;
	}

	std::string dirname = input;

	if (!analyseLifelongSLAMFolder(dirname))	{
		std::cerr << "Invalid folder." << std::endl;
		return nullptr;
	}


	SLAMFile * slamfilep = new SLAMFile();
	SLAMFile & slamfile  = *slamfilep;

	DepthSensor::disparity_params_t disparity_params =  {0.001,0.0};
	DepthSensor::disparity_type_t disparity_type = DepthSensor::affine_disparity;

	/**
	 * load Depth
	 */

	if(depth && !loadLifelongSLAMDepthData(dirname,"d400_depth_optical_frame", slamfile,disparity_params,disparity_type)) {
		std::cout << "Error while loading LifelongSLAM depth information." << std::endl;
		delete slamfilep;
		return nullptr;
	}

	/**
	 * load Aligned_Depth
	 */

	if(aligned_depth && !loadLifelongSLAMDepthData(dirname,"d400_color_optical_frame", slamfile,disparity_params,disparity_type, true)) {
		std::cout << "Error while loading LifelongSLAM depth information." << std::endl;
		delete slamfilep;
		return nullptr;
	}

	/**
	 * load Grey
	 */

	if(grey && !loadLifelongSLAMGreyData(dirname, "d400_color_optical_frame", "color", slamfile)) {
		std::cout << "Error while loading LifelongSLAM Grey information." << std::endl;
		delete slamfilep;
		return nullptr;
	}

	/**
	 * load RGB
	 */

	if(color && !loadLifelongSLAMRGBData(dirname, "d400_color_optical_frame", slamfile)) {
		std::cout << "Error while loading LifelongSLAM RGB information." << std::endl;
		delete slamfilep;
		return nullptr;
	}

	/**
	 * load Accelerometer
	 */
	if(d400_accel && !loadLifelongSLAMAccelerometerData(dirname, "d400_accelerometer", slamfile)) {
		std::cout << "Error while loading Accelerometer information." << std::endl;
		delete slamfilep;
		return nullptr;
	}

	if(d400_gyro && !loadLifelongSLAMGyroData(dirname, "d400_gyroscope", slamfile)) {
		std::cout << "Error while loading Gyro information." << std::endl;
		delete slamfilep;
		return nullptr;
	}

	/**
	 * load Fisheyes
	 */

	if(fisheye1 && !loadLifelongSLAMGreyData(dirname, "t265_fisheye1_optical_frame", "Fisheye_1", slamfile)) {
		std::cout << "Error while loading LifelongSLAM fisheye1 information." << std::endl;
		delete slamfilep;
		return nullptr;
	}

	if(fisheye2 && !loadLifelongSLAMGreyData(dirname, "t265_fisheye2_optical_frame", "Fisheye_2", slamfile)) {
		std::cout << "Error while loading LifelongSLAM fisheye2 information." << std::endl;
		delete slamfilep;
		return nullptr;
	}

	/**
	 * load Accelerometer
	 */
	if(t265_accel && !loadLifelongSLAMAccelerometerData(dirname, "t265_accelerometer", slamfile)) {
		std::cout << "Error while loading Accelerometer information." << std::endl;
		delete slamfilep;
		return nullptr;
	}


	if(t265_gyro && !loadLifelongSLAMGyroData(dirname, "t265_gyroscope", slamfile)) {
		std::cout << "Error while loading Gyro information." << std::endl;
		delete slamfilep;
		return nullptr;
	}

	/**
	 * load GT
	 */
	if(gt && !loadLifelongSLAMGroundTruthData(dirname, slamfile)) {
		std::cout << "Error while loading gt information." << std::endl;
		delete slamfilep;
		return nullptr;
	}


    if(odom && !loadLifelongSLAMOdomData(dirname, "odometer", slamfile)) {
		std::cout << "Error while loading Odom information." << std::endl;
		delete slamfilep;
		return nullptr;
	}

	return slamfilep;
	}
