/*

 Copyright (c) 2017 University of Edinburgh, Imperial College, University of Manchester.
 Developed in the PAMELA project, EPSRC Programme Grant EP/K008730/1

 This code is licensed under the MIT License.

 */


#include "outputs/Output.h"
#include "outputs/TrajectoryAlignmentMethod.h"
#include "outputs/TrajectoryInterface.h"

#include <iostream>

using namespace slambench::outputs;

BaseOutput::BaseOutput(const std::string& name, const values::ValueDescription &type, bool main_output) : name_(name), type_(type), only_keep_most_recent_(false), active_(false), main_(main_output)
{

}

BaseOutput::~BaseOutput()
{

}

const BaseOutput::value_map_t::value_type& BaseOutput::GetMostRecentValue() const
{
	if(GetValues().empty()) {
		throw std::logic_error("No values in output");
	}
	
	return *GetValues().rbegin();
}

bool BaseOutput::Empty() const
{
	return GetValues().empty();
}


Output::Output(const std::string& name, values::ValueType type, bool main_output) : BaseOutput(name, type, main_output)
{

}

Output::~Output()
{

}

void Output::AddPoint(timestamp_t time, const values::Value *value)
{
	assert(value->GetType() == GetType());
	assert(IsActive());
	if(GetKeepOnlyMostRecent()) {
		for(auto i : values_) {
			delete i.second;
		}
		values_.clear();
	}
	values_[time] = value;
	
	Updated();
}

const Output::value_map_t& Output::GetValues() const
{
	assert(IsActive());
	return values_;
}

const BaseOutput::value_map_t::value_type& Output::GetMostRecentValue() const
{
	if(values_.empty()) {
		throw std::logic_error("No values available in output");
	}
	return *values_.rbegin();
}

void BaseOutput::Updated() const
{
	for(auto i : update_callbacks_) {
		i(this);
	}
}

DerivedOutput::DerivedOutput(const std::string &name, values::ValueType type, const std::initializer_list<BaseOutput*> &derived_from, bool main) : BaseOutput(name, type, main), derived_from_(derived_from)
{
	for(BaseOutput *i : derived_from_) {
		i->AddUpdateCallback([this](const BaseOutput*){this->Invalidate();});
	}
}

DerivedOutput::~DerivedOutput()
{

}

bool DerivedOutput::Empty() const
{
	if(!up_to_date_) {
		recalculate();
	}
	
	return cached_values_.empty();
}

void DerivedOutput::recalculate() const
{
	const_cast<DerivedOutput*>(this)->Recalculate();
	const_cast<DerivedOutput*>(this)->up_to_date_ = true;
	Updated();
}


const BaseOutput::value_map_t::value_type& DerivedOutput::GetMostRecentValue() const
{
	if(!up_to_date_) {
		recalculate();
	}
	
	return *cached_values_.rbegin();
}

const BaseOutput::value_map_t& DerivedOutput::GetValues() const
{
	if(!up_to_date_) {
		recalculate();
	}
	
	return cached_values_;
}

void DerivedOutput::Invalidate()
{
	up_to_date_ = false;
}

BaseOutput::value_map_t& DerivedOutput::GetCachedValueMap()
{
	return cached_values_;
}


AlignmentOutput::AlignmentOutput(const std::string& name, TrajectoryInterface* gt_trajectory, BaseOutput* trajectory, TrajectoryAlignmentMethod *method) : DerivedOutput(name, values::VT_MATRIX, {trajectory}), gt_trajectory_(gt_trajectory), trajectory_(trajectory), method_(method)
{

}

AlignmentOutput::~AlignmentOutput()
{

}


void AlignmentOutput::Recalculate()
{
	assert(GetKeepOnlyMostRecent());
	auto &target = GetCachedValueMap();
	
	for(auto i : target) {
		delete i.second;
	}
	target.clear();
	
	if(trajectory_->Empty()) {
		return;
	}
	
	slambench::outputs::PoseOutputTrajectoryInterface traj_int(trajectory_);

	transformation = (*method_)(gt_trajectory_->GetAll(), traj_int.GetAll());
	auto &last_point = trajectory_->GetMostRecentValue();

	target.insert({last_point.first, new values::TypedValue<Eigen::Matrix4f>(transformation)});
}

AlignedPoseOutput::AlignedPoseOutput(const std::string& name, AlignmentOutput* alignment, BaseOutput* pose_output) : DerivedOutput(name, values::VT_POSE, {alignment, pose_output}), alignment_(alignment), pose_output_(pose_output)
{

}

AlignedPoseOutput::~AlignedPoseOutput()
{

}

void AlignedPoseOutput::Recalculate()
{
	auto &target = GetCachedValueMap();
	
	for(auto i : target) {
		delete i.second;
	}
	target.clear();
	
	if(!alignment_->IsActive()) return;
	if (alignment_->Empty())  return;
	if(!pose_output_->IsActive()) return;
	if (pose_output_->Empty())  return;
	
	auto newest_alignment = alignment_->GetMostRecentValue().second;
	values::TypedValue<Eigen::Matrix4f> *mv = (values::TypedValue<Eigen::Matrix4f>*)newest_alignment;	
	
	for(auto traj_point : pose_output_->GetValues()) {
		values::PoseValue *pose = (values::PoseValue*)traj_point.second;

		Eigen::Matrix4f tmp = mv->GetValue() * pose->GetValue();
		target.insert({traj_point.first, new values::PoseValue(tmp)});
	}
}

AlignedPointCloudOutput::AlignedPointCloudOutput(const std::string& name, AlignmentOutput* alignment, BaseOutput* pc_output) : DerivedOutput(name, values::VT_POINTCLOUD, {alignment, pc_output}), alignment_(alignment), pointcloud_(pc_output)
{
	SetKeepOnlyMostRecent(true);
}

AlignedPointCloudOutput::~AlignedPointCloudOutput()
{

}

void AlignedPointCloudOutput::Recalculate()
{
	assert(GetKeepOnlyMostRecent());
	auto &target = GetCachedValueMap();
	
	for(auto i : target) {
		delete i.second;
	}
	target.clear();
	
	if(!pointcloud_->IsActive() || pointcloud_->GetValues().empty() || !alignment_->IsActive() || alignment_->GetValues().empty()) {
		return;
	}
	
	auto latest_point = pointcloud_->GetMostRecentValue();
	auto latest_pc = (values::PointCloudValue*)latest_point.second;
	auto latest_ts = latest_point.first; 
	
	auto latest_alignment = ((values::TypedValue<Eigen::Matrix4f>*)alignment_->GetMostRecentValue().second)->GetValue();
	
	auto new_pc = new values::PointCloudValue(*latest_pc);
	new_pc->SetTransform(latest_alignment);
	target.insert({latest_ts, new_pc});
}

PoseToXYZOutput::PoseToXYZOutput(BaseOutput* pose_output) : BaseOutput(pose_output->GetName() + " (XYZ)", values::ValueDescription({{"X", values::VT_DOUBLE}, {"Y", values::VT_DOUBLE}, {"Z", values::VT_DOUBLE}})), pose_output_(pose_output)
{

}

PoseToXYZOutput::~PoseToXYZOutput()
{

}

const BaseOutput::value_map_t& PoseToXYZOutput::GetValues() const
{
	GetMostRecentValue();
	return cached_values_;
}

const BaseOutput::value_map_t::value_type& PoseToXYZOutput::GetMostRecentValue() const
{
	cached_values_.clear();
	
	auto pose_value = pose_output_->GetMostRecentValue();
	auto val = (values::TypeForVT<values::VT_POSE>::type*)pose_value.second;
	
	float x = val->GetValue()(0, 3);
	float y = val->GetValue()(1, 3);
	float z = val->GetValue()(2, 3);
	
	auto xv = new values::TypeForVT<values::VT_DOUBLE>::type(x);
	auto yv = new values::TypeForVT<values::VT_DOUBLE>::type(y);
	auto zv = new values::TypeForVT<values::VT_DOUBLE>::type(z);
	
	cached_values_.insert({pose_value.first, new values::TypeForVT<values::VT_COLLECTION>::type({{"X", xv}, {"Y", yv}, {"Z", zv}})});
	return *cached_values_.begin();
}
