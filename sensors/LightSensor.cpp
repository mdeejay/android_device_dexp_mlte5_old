/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <cutils/log.h>
#include <cutils/properties.h>

#include "sensors.h"
#include "LightSensor.h"

#define EVENT_TYPE_LIGHT		ABS_MISC
/*****************************************************************************/

enum input_device_name {
	LTR559_LS = 0,
	GENERIC_LS,
	LIGHTSENSOR_LEVEL,
	SUPPORTED_LSENSOR_COUNT,
};

enum {
	TYPE_ADC = 0,
	TYPE_LUX,
};

static const char *data_device_name[SUPPORTED_LSENSOR_COUNT] = {
	[LTR559_LS] = "ltr559-ls",
	[GENERIC_LS] = "light",
	[LIGHTSENSOR_LEVEL] = "lightsensor-level",
};

static const char *input_sysfs_path_list[SUPPORTED_LSENSOR_COUNT] = {
	/* This one is for back compatibility, we don't need it for generic HAL.*/
	[LTR559_LS] = "/sys/class/input/%s/device/",
	[GENERIC_LS] = "/sys/class/input/%s/device",
	[LIGHTSENSOR_LEVEL] = "/sys/class/input/%s/device/",
};

static const char *input_sysfs_enable_list[SUPPORTED_LSENSOR_COUNT] = {
	[LTR559_LS] = "enable_als_sensor",
	[GENERIC_LS] = "enable",
	[LIGHTSENSOR_LEVEL] = "enable",
};

static const int input_report_type[SUPPORTED_LSENSOR_COUNT] = {
	[LTR559_LS] = TYPE_LUX,
	[GENERIC_LS] = TYPE_LUX,
	[LIGHTSENSOR_LEVEL] = TYPE_ADC,
};

LightSensor::LightSensor()
: SensorBase(NULL, NULL),
	  mInputReader(4),
	  mHasPendingEvent(false),
	  sensor_index(-1)
{
	int i;

	mPendingEvent.version = sizeof(sensors_event_t);
	mPendingEvent.sensor = SENSORS_LIGHT_HANDLE;
	mPendingEvent.type = SENSOR_TYPE_LIGHT;
	memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));

	for(i = 0; i < SUPPORTED_LSENSOR_COUNT; i++) {
		data_name = data_device_name[i];

		// data_fd is not initialized if data_name passed
		// to SensorBase is NULL.
		data_fd = openInput(data_name);
		if (data_fd > 0) {
			sensor_index = i;
			break;
		}
	}

	if (data_fd > 0) {
		snprintf(input_sysfs_path, sizeof(input_sysfs_path),
				input_sysfs_path_list[i], input_name);
		input_sysfs_path_len = strlen(input_sysfs_path);
		enable(0, 1);
	}
	ALOGI("The light sensor path is %s",input_sysfs_path);
}

LightSensor::LightSensor(char *name)
	: SensorBase(NULL, data_device_name[GENERIC_LS]),
	  mInputReader(4),
	  mHasPendingEvent(false),
	  sensor_index(GENERIC_LS)
{
	mPendingEvent.version = sizeof(sensors_event_t);
	mPendingEvent.sensor = SENSORS_LIGHT_HANDLE;
	mPendingEvent.type = SENSOR_TYPE_LIGHT;
	memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));

	if (data_fd > 0) {
		strlcpy(input_sysfs_path, SYSFS_CLASS, sizeof(input_sysfs_path));
		strlcat(input_sysfs_path, name, sizeof(input_sysfs_path));
		strlcat(input_sysfs_path, "/", sizeof(input_sysfs_path));
		input_sysfs_path_len = strlen(input_sysfs_path);
		ALOGI("The light sensor path is %s",input_sysfs_path);
		enable(0, 1);
	}
}

LightSensor::LightSensor(struct SensorContext *context)
	: SensorBase(NULL, NULL, context),
	  mInputReader(4),
	  mHasPendingEvent(false),
	  sensor_index(GENERIC_LS)
{
	mPendingEvent.version = sizeof(sensors_event_t);
	mPendingEvent.sensor = context->sensor->handle;
	mPendingEvent.type = SENSOR_TYPE_LIGHT;
	memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));

	data_fd = context->data_fd;
	strlcpy(input_sysfs_path, context->enable_path, sizeof(input_sysfs_path));
	input_sysfs_path_len = strlen(input_sysfs_path);
	mUseAbsTimeStamp = false;
}

LightSensor::~LightSensor() {
	if (mEnabled) {
		enable(0, 0);
	}
}

int LightSensor::setDelay(int32_t, int64_t ns)
{
	int fd;
	char propBuf[PROPERTY_VALUE_MAX];
	property_get("sensors.light.loopback", propBuf, "0");
	if (strcmp(propBuf, "1") == 0) {
		ALOGE("sensors.light.loopback is set");
		return 0;
	}
	int delay_ms = ns / 1000000;
	strlcpy(&input_sysfs_path[input_sysfs_path_len],
			SYSFS_POLL_DELAY, SYSFS_MAXLEN);
	fd = open(input_sysfs_path, O_RDWR);
	if (fd >= 0) {
		char buf[80];
		snprintf(buf, sizeof(buf), "%d", delay_ms);
		write(fd, buf, strlen(buf)+1);
		close(fd);
		return 0;
	}
	return -1;
}

int LightSensor::enable(int32_t, int en)
{
	int flags = en ? 1 : 0;
	char propBuf[PROPERTY_VALUE_MAX];
	property_get("sensors.light.loopback", propBuf, "0");
	if (strcmp(propBuf, "1") == 0) {
		mEnabled = flags;
		ALOGE("sensors.light.loopback is set");
		return 0;
	}
	if (flags != mEnabled) {
		int fd;
		if (sensor_index >= 0) {
			strlcpy(&input_sysfs_path[input_sysfs_path_len],
				input_sysfs_enable_list[sensor_index], sizeof(input_sysfs_path) - input_sysfs_path_len);
		}
		else {
			ALOGE("invalid sensor index:%d\n", sensor_index);
			return -1;
		}
		fd = open(input_sysfs_path, O_RDWR);
		if (fd >= 0) {
			char buf[2];
			int err;
			buf[1] = 0;
			if (flags) {
				buf[0] = '1';
			} else {
				buf[0] = '0';
			}
			err = write(fd, buf, sizeof(buf));
			close(fd);
			mEnabled = flags;
			return 0;
		} else {
			ALOGE("open %s failed.(%s)\n", input_sysfs_path, strerror(errno));
			return -1;
		}
	} else if (flags) { /* already enabled */
		mHasPendingEvent = true;
	}
	return 0;
}

bool LightSensor::hasPendingEvents() const {
	return mHasPendingEvent || mHasPendingMetadata;
}

int LightSensor::readEvents(sensors_event_t* data, int count)
{
	if (count < 1)
		return -EINVAL;

	if (mHasPendingEvent) {
		mHasPendingEvent = false;
		mPendingEvent.timestamp = getTimestamp();
		*data = mPendingEvent;
		return mEnabled ? 1 : 0;
	}

	if (mHasPendingMetadata) {
		mHasPendingMetadata--;
		meta_data.timestamp = getTimestamp();
		*data = meta_data;
		return mEnabled ? 1 : 0;
	}

	ssize_t n = mInputReader.fill(data_fd);
	if (n < 0)
		return n;

	int numEventReceived = 0;
	input_event const* event;

	while (count && mInputReader.readEvent(&event)) {
		int type = event->type;
		if (type == EV_ABS) {
			if (event->code == EVENT_TYPE_LIGHT) {
				mPendingEvent.light = convertEvent(event->value);
			}
		} else if (type == EV_SYN) {
			switch ( event->code ) {
				case SYN_TIME_SEC:
					mUseAbsTimeStamp = true;
					report_time = event->value*1000000000LL;
					break;
				case SYN_TIME_NSEC:
					mUseAbsTimeStamp = true;
					mPendingEvent.timestamp = report_time+event->value;
					break;
				case SYN_REPORT:
					if(mUseAbsTimeStamp != true) {
						mPendingEvent.timestamp = timevalToNano(event->time);
					}
					if (mEnabled) {
						*data++ = mPendingEvent;
						count--;
						numEventReceived++;
					}
					break;
			}
		} else {
			ALOGE("LightSensor: unknown event (type=%d, code=%d)",
					type, event->code);
		}
		mInputReader.next();
	}

	return numEventReceived;
}

float LightSensor::convertEvent(int value)
{
	float lux = 0;

	if (sensor_index >= 0) {
		if (input_report_type[sensor_index] == TYPE_ADC) {
			// Convert adc value to lux assuming:
			// I = 10 * log(Ev) uA
			// R = 47kOhm
			// Max adc value 4095 = 3.3V
			// 1/4 of light reaches sensor
			lux =  powf(10, value * (330.0f / 4095.0f / 47.0f)) * 4;
		} else if (input_report_type[sensor_index] == TYPE_LUX) {
			lux = value;
		} else {
			ALOGE("LightSensor: unknown report type\n");
			lux = 0;
		}
	}

	return lux;
}
