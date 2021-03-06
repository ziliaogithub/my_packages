#include <iostream>
#include <string>
#include <math.h>
#include <unistd.h>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <omp.h>

#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>
// #include <velodyne_pointcloud/point_types.h>

#include <pcl/common/common.h>
#include <pcl/io/io.h>
#include <pcl/io/pcd_io.h>
// #include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>

#define TILE_WIDTH 200

struct Key
{
	int x;
	int y;
};

bool operator==(const Key& lhs, const Key& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

namespace std
{
  template <>
  struct hash<Key>
  {
    std::size_t operator()(const Key& k) const
    {
      return hash<int>()(k.x) ^ (hash<int>()(k.y) << 1);
    }
  };
}

int main(int argc, char** argv)
{
	ros::init(argc, argv, "mndt");
	// ros::NodeHandle nh;
	// ros::Publisher mapPub = nh.advertise<sensor_msgs::PointCloud2>("/ndt_map", 1000);


	// Get input pointcloud first
	if(argc < 2)
	{
		std::cerr << "No input .pcd file." << std::endl;
		return -1;
	}

	pcl::PointCloud<pcl::PointXYZI> src; // all input map
	for(int i = 1; i < argc; i++)
	{	
		pcl::PointCloud<pcl::PointXYZI> tmp;
		std::string inputFile = argv[i];

	  std::cout << "Loading " << inputFile << std::endl;
		if(pcl::io::loadPCDFile<pcl::PointXYZI>(inputFile, tmp) == -1)
	  {
	    std::cerr << "Couldn't read " << inputFile << "." << std::endl;
	    return -1;
	  }
	  else
	  	src += tmp;
	}
  std::cout << src.size() << " data points loaded." << std::endl;

  std::unordered_map<Key, pcl::PointCloud<pcl::PointXYZI>> worldMap;

  // Allocate each point in source map to worldMap
  std::chrono::time_point<std::chrono::system_clock> time_start, time_end;
  time_start = std::chrono::system_clock::now();

  for(pcl::PointCloud<pcl::PointXYZI>::const_iterator item = src.begin(); item < src.end(); item++)
  {
  	// Get 2D point
  	Key key;
  	key.x = int(floor(item->x / TILE_WIDTH));
  	key.y = int(floor(item->y / TILE_WIDTH));

	  worldMap[key].push_back(*item);
	}
	time_end = std::chrono::system_clock::now();
	double time_diff = std::chrono::duration_cast<std::chrono::microseconds>(time_end - time_start).count() / 1000.0;

	for (auto& x: worldMap) 
	{
    std::cout << "(" << x.first.x << "," << x.first.y << "): " << x.second.size() << std::endl;
    std::string out_filename = "/home/zwu/map_folder/" + std::to_string(x.first.x) + ";" + std::to_string(x.first.y);
	  pcl::io::savePCDFileBinary(out_filename + ".pcd", x.second);
	  std::cout << "Finished. Saved pointcloud to " << out_filename << ".pcd" << std::endl;
  }

//   std::cout << "Total processing time: " << time_diff << std::endl;

//   time_start = std::chrono::system_clock::now();
//   Key local_key;
//   local_key.x = int(floor(1 / TILE_WIDTH));
//   local_key.y = int(floor(0 / TILE_WIDTH));
//   pcl::PointCloud<pcl::PointXYZI> localMap;
//   Key tmp_key;
// // #pragma omp parallel for shared(localMap, worldMap) num_threads(2) 
//   for(int x = local_key.x - 1; x <= local_key.x; x++)
//     for(int y = local_key.y - 1; y <= local_key.y; y++)
//     {
//     	tmp_key.x = x;
//     	tmp_key.y = y;
//       localMap += worldMap[tmp_key];
//     }
//   time_end = std::chrono::system_clock::now();
// 	time_diff = std::chrono::duration_cast<std::chrono::microseconds>(time_end - time_start).count() / 1000.0;
// 	std::cout << "Total getting local_map time: " << time_diff << std::endl;
	// for(int i = 0; ; i++)
	// {
	// 	localMap.clear();
	// 	for(int j = -1; j <= 1; j++)
	// 	{
	// 		for(int k = -1; k <= 1; k++)
	// 		{
	// 			Key localKey;
	// 			localKey.x = int(floor((0 + j) / TILE_WIDTH));
	// 			localKey.y = int(floor((i + k) / TILE_WIDTH));
	// 			localMap += worldMap[localKey];
	// 		}
		// }

	// 	// Publish cloud
	//   sensor_msgs::PointCloud2::Ptr map_msg_ptr(new sensor_msgs::PointCloud2);
	//   pcl::toROSMsg(localMap, *map_msg_ptr);
	//   map_msg_ptr->header.frame_id = "map";
	//   mapPub.publish(*map_msg_ptr);
	//   if(i > 10) 
	//   	i = 0;
	//   sleep(1);
	// }
	return 0;
}