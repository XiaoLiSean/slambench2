# Life-long Semantic SLAM on OpenLORIS-scene #
This project is intended to discuss the limitation of state-of-art SLAM algorithms in dealing with life-long dynamics. Our objective is to develop a mechanism integrating the semantic information of life-long dynamics objects (e.g. furniture) on mapping while simultaneously improve the performance of localization and relocalization.

## Courtesy ##
* The OpenLORIS-Scene dataset is used for its better consideration of daily life implementation scenario where objects' life-long dynamics is presented. For more information regarding the dataset, please refer to https://lifelong-robotic-vision.github.io/dataset/scene and 

```
@article{shi2019openlorisscene,
    title={Are We Ready for Service Robots? The {OpenLORIS-Scene} Datasets for Lifelong {SLAM}},
    author={Xuesong Shi and Dongjiang Li and Pengpeng Zhao and Qinbin Tian and Yuxin Tian and Qiwei Long and Chunhao Zhu and Jingwei Song and Fei Qiao and Le Song and Yangquan Guo and Zhigang Wang and Yimin Zhang and Baoxing Qin and Wei Yang and Fangshi Wang and Rosa H. M. Chan and Qi She},
    journal={arXiv preprint arXiv:1911.05603},
    year={2019}
}
```

* Based on Pamela-Project (https://github.com/pamela-project/slambench2) which developed a well-integrated SLAM evaluation architecture, OpenLORIS provided (https://github.com/lifelong-robotic-vision/slambench2/edit/lifelong/README.md) a framework which enables users to evaluate relocalization function of life-long SLAM algorithms upon two datasets of one common scene during different robot patrol runs. 

## Building & Testing ##

For building dependencies under Ubuntu 18.04, you might come across building failures triggered by dependencies being not found. In this case, you might need to install the corresponding dependency libraries (e.g. CERES, FLANN) to folder ```slambench2/deps``` by yourself with extra care on their own dependencies requirements. The entire dependency system is described in https://github.com/pamela-project/slambench2/blob/master/framework/makefiles/README.md.

For ubuntu 16.04:
```apt-get -y install libvtk6.2 libvtk6-dev unzip libflann-dev wget mercurial git gcc cmake python-numpy freeglut3 freeglut3-dev libglew1.5 libglew1.5-dev libglu1-mesa libglu1-mesa-dev libgl1-mesa-glx libgl1-mesa-dev libxmu-dev libxi-dev  libboost-all-dev cvs libgoogle-glog-dev libatlas-base-dev gfortran  gtk2.0 libgtk2.0-dev libproj9 libproj-dev libyaml-0-2 libyaml-dev libyaml-cpp-dev libhdf5-dev libhdf5-dev```

### Building under Ubuntu 16.04 ###

1. Building dependency system

```
git clone git@github.com:lifelong-robotic-vision/slambench2.git (or your own forked one)
cd slambench2
make deps
make slambench
```

2. Download OpenLORIS dataset from https://lifelong-robotic-vision.github.io/dataset/scene (take office dataset as an example)

```
cd ./dir/to/slambench2
mkdir ./datasets/OpenLORIS
cp ./dir/to/office1-1_7-package.tar ./datasets/OpenLORIS
sudo apt-get p7zip p7zip-full
```

3. Dataset transformation

* Build all data sequences:

```
make ./datasets/OpenLORIS/office1.all
```

* Just one sequence:

```
make ./datasets/OpenLORIS/office1/office1-1.slam
```

* In  OpenLORIS-Scene dataset transformation, 12 parameters are set to decide which sensors to be included, and the default value is ```true```. If you want only part of the sensors, e.g. rgbd and ground-truth, you can run:

```
./build/bin/dataset-generator -d OpenLORIS -i ./datasets/OpenLORIS/office1/office1-1/ -o ./datasets/OpenLORIS/office1/office1-1_rgbd.slam -color true -aligned_depth true -grey false -depth false -d400_accel false -d400_gyro false -fisheye1 false -fisheye2 false -t265_accel false -t265_gyro false -odom false -gt true
```

At least one camera-type sensor must be choosed.

### Testing loader ###

There are three loader avaliable for slambench:
* benchmark_loader
* pangolin_loader
* lifelong_loader

For ```benchmark_loader``` and ```pangolin_loader```, please refer to https://github.com/pamela-project/slambench2. ```lifelong_loader``` is specifically designed by OpenLORIS including evaluation function for SLAM relocalization. We will take testing example on office dataset with orbslam2:

```
make slambench
make orbslam2
make slambench APPS=orbslam2
make ./datasets/OpenLORIS/office1/office1-1.slam
./build/bin/lifelong_loader -i ./datasets/OpenLORIS/office1/office1-1.slam -load ./build/lib/liborbslam2-original-library.so -fo ./1_rgbd
```

Moreover, you can replace ```lifelong_loader``` with other two loaders as:

```
make slambench
make orbslam2
make slambench APPS=orbslam2
make ./datasets/OpenLORIS/office1/office1-1.slam
./build/bin/XXX_loader -i ./datasets/OpenLORIS/office1/office1-1.slam -load ./build/lib/liborbslam2-original-library.so -fo ./1_rgbd
```

If three loaders can excuted normally, testing is a success. One remark: ```lifelong_loader``` is capable of taking multiple datasets only when the ```bool sb_relocalize()``` API is implemented. Taking multiple datasets with ```lifelong_loader``` and benchmark SLAM algorithm (e.g. orbslam2) will lead to failure.
For detail illustration, please refer to https://github.com/pamela-project/slambench2 - Sec. "Compilation of SLAMBench and its benchmarks" and https://github.com/lifelong-robotic-vision/slambench2 - Sec. "Run lifelong_loader".

### Known Issue ###
For slam usecase like kfusion which has special requirements for CUDA, we will need extra dependencies.

With Ubuntu: apt-get -y install nvidia-cuda-toolkit clinfo

With Fedora: yum install cuda

However, graphics drivers provided by Nvidia might cause your cp entering "low-graphics mode" which might attribute to conflicts between driver programs and GPU (e.g. AMD Radeon Graphics). Try following to resolve the issue:

```
Ctrl+Alt+F1 Open a TTY terminal session 
sudo apt-get purge libcuda* nvidia*
sudo apt-get install lightdm
sudo service lightdm restart
```
This will bring your graphic system back while will not resolve the cuda dependencies issue.
