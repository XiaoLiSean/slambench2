/*

 Copyright (c) 2017 University of Edinburgh, Imperial College, University of Manchester.
 Developed in the PAMELA project, EPSRC Programme Grant EP/K008730/1

 This code is licensed under the MIT License.

 */


#include "io/sensor/GyroSensor.h"
#include "io/sensor/SensorDatabase.h"
#include "io/serialisation/Serialiser.h"
#include "io/deserialisation/Deserialiser.h"

using namespace slambench::io;

const Sensor::sensor_type_t GyroSensor::kGyroType = "Gyro";

GyroSensor::GyroSensor(const sensor_name_t &sensor_name) : Sensor(sensor_name, kGyroType) {

}
size_t GyroSensor::GetFrameSize(const SLAMFrame *frame) const  {
	(void)frame;
	return 3 * sizeof(float);
}

class GyroSerialiser : public SensorSerialiser {
	bool SerialiseSensorSpecific(Serialiser* serialiser, const Sensor* s) override {
		// nothing to do
		GyroSensor *sensor = (GyroSensor*)s;

		// serialiser->Write(&sensor->GyroscopeNoiseDensity, sizeof(sensor->GyroscopeNoiseDensity));
		// serialiser->Write(&sensor->GyroscopeDriftNoiseDensity, sizeof(sensor->GyroscopeDriftNoiseDensity));
		// serialiser->Write(&sensor->GyroscopeBiasDiffusion, sizeof(sensor->GyroscopeBiasDiffusion));
		// serialiser->Write(&sensor->GyroscopeSaturation          , sizeof(sensor->GyroscopeSaturation));
		serialiser->Write(&sensor->Intrinsic , sizeof(sensor->Intrinsic));
		serialiser->Write(&sensor->NoiseVariances, sizeof(sensor->NoiseVariances));
		serialiser->Write(&sensor->BiasVariances, sizeof(sensor->BiasVariances));

		return true;
	}
};

class GyroDeserialiser : public SensorDeserialiser {
	bool InstantiateSensor(const Sensor::sensor_name_t &sensor_name,const Sensor::sensor_type_t &type, Sensor** s) override {
		if(type != GyroSensor::kGyroType) {
			return false;
		}

		*s = new GyroSensor(sensor_name);

		return true;
	}

	bool DeserialiseSensorSpecific(Deserialiser* d, Sensor* s) override {
		// nothing to do
		GyroSensor *sensor = (GyroSensor*)s;

		// d->Read(&sensor->GyroscopeNoiseDensity, sizeof(sensor->GyroscopeNoiseDensity));
		// d->Read(&sensor->GyroscopeDriftNoiseDensity, sizeof(sensor->GyroscopeDriftNoiseDensity));
		// d->Read(&sensor->GyroscopeBiasDiffusion, sizeof(sensor->GyroscopeBiasDiffusion));
		// d->Read(&sensor->GyroscopeSaturation          , sizeof(sensor->GyroscopeSaturation));
		d->Read(&sensor->Intrinsic , sizeof(sensor->Intrinsic));
		d->Read(&sensor->NoiseVariances, sizeof(sensor->NoiseVariances));
		d->Read(&sensor->BiasVariances, sizeof(sensor->BiasVariances));

		return true;
	}
};

static slambench::io::SensorDatabaseRegistration gyro_reg(GyroSensor::kGyroType, slambench::io::SensorDatabaseEntry(new GyroSerialiser(), new GyroDeserialiser(), false, false));


