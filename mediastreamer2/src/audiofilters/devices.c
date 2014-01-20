/*
 * devices.c - sound device information table (latency, presence of AEC etc...)
 *
 * Copyright (C) 2009-2012  Belledonne Communications, Grenoble, France
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */



#include "devices.h"

#ifdef ANDROID
#include "sys/system_properties.h"
#include <jni.h>
#endif



#ifdef ANDROID
static SoundDeviceDescription devices[]={
	{	"HTC",		"Nexus One",	"qsd8k",	0,	300 },
	{	"HTC",		"HTC One X",	"tegra",	0,	150 },	/*has a very good acoustic isolation, which result in calibration saying no echo. */
										/*/But with speaker mode there is a strong echo if software ec is disabled.*/
	{	"HTC",		"HTC One SV",	"msm8960",	0,	200 },	
	{	"HTC",		"HTC Desire",	"",			0,	250 },
	{	"HTC",		"HTC Sensation Z710e",	"",	0,	200 },
	{	"HTC",		"HTC Wildfire",	"",			0,	270 },
	
	
	{	"LGE",		"Nexus 4",		"msm8960",	0,	230 }, /* has built-in AEC starting from 4.3*/
	{	"LGE",		"LS670",		"",			0,	170 },
	
	{	"motorola",	"DROID RAZR",	"",			0,	400 },
	{	"motorola",	"MB860",		"",			0,	200 },
	{	"motorola",	"XT907",		"",			0,	500 },
	{	"motorola",	"DROIX X2",		"",			0,	320 },

	{	"samsung",	"GT-S5360",		"bcm21553",	0,	250 }, /*<Galaxy Y*/
	{	"samsung",	"GT-S5360L",	"",			0,	250 }, /*<Galaxy Y*/
	{	"samsung",	"GT-S6102",		"",			DEVICE_HAS_BUILTIN_AEC,	0 }, /*<Galaxy Y duo*/
	{	"samsung",	"GT-S5570",		"",			0,	160 }, /*<Galaxy Y duo*/
	{	"samsung",	"GT-S5300",		"",			DEVICE_HAS_BUILTIN_AEC,	0 }, /*<Galaxy Pocket*/
	{	"samsung",	"GT-S5830i",		"",		0,	200 }, /* Galaxy S */
	{	"samsung",	"GT-S5830",		"",			0,	170 }, /* Galaxy S */
	{	"samsung",	"GT-S5660",		"",			0,	160 }, /* Galaxy Gio */
	{	"samsung",	"GT-I9000",		"",			0,	200 }, /* Galaxy S */
	{	"samsung",	"GT-I9001",		"",			0,	150 }, /* Galaxy S+ */
	{	"samsung",	"GT-I9070",		"",			DEVICE_HAS_BUILTIN_AEC,	0 }, /* Galaxy S Advance */
	{	"samsung",	"SPH-D700",		"",			0,	200 }, /* Galaxy S Epic 4G*/
	{	"samsung",	"GT-I9100",		"",			DEVICE_HAS_BUILTIN_AEC,	0 }, /*Galaxy S2*/
	{	"samsung",	"GT-I9100P",	"s5pc210",	DEVICE_HAS_BUILTIN_AEC,	0 }, /*Galaxy S2*/
	{	"samsung",	"GT-S7562",		"",			DEVICE_HAS_BUILTIN_AEC,	0 }, /*<Galaxy S Duo*/
	{	"samsung",	"SCH-I415",		"",			DEVICE_HAS_BUILTIN_AEC,	0 }, /* Galaxy S ??*/
	{	"samsung",	"SCH-I425",		"",			DEVICE_HAS_BUILTIN_AEC,	0 }, /* Galaxy S ??*/
	{	"samsung",	"SCH-I535",		"",			DEVICE_HAS_BUILTIN_AEC,	0 }, /* Galaxy S ??*/
	{	"samsung",	"SPH-D710",		"",			DEVICE_HAS_BUILTIN_AEC,	0 }, /* Galaxy S2 Epic 4G*/
	{	"samsung",	"GT-I9300",		"exynos4",	DEVICE_HAS_BUILTIN_AEC,	0 },  /*Galaxy S3*/
	{	"samsung",	"SAMSUNG-SGH-I747","",		DEVICE_HAS_BUILTIN_AEC,	0 }, /* Galaxy S3*/
	{	"samsung",	"SPH-L710","",				DEVICE_HAS_BUILTIN_AEC,	0 }, /* Galaxy S3*/
	{	"samsung",	"SPH-D710","",				DEVICE_HAS_BUILTIN_AEC,	0 }, /* Galaxy S3*/
	{	"samsung",	"SGH-T999",		"",			DEVICE_HAS_BUILTIN_AEC,	0 },  /*Galaxy S3*/
	{	"samsung",	"SAMSUNG-SGH-I337","",		DEVICE_HAS_BUILTIN_AEC,	0 }, /* Galaxy S4 ? */
	{	"samsung",	"GT-I9195",		"",			DEVICE_HAS_BUILTIN_AEC,	0 }, /* Galaxy S4 mini*/
	{	"samsung",	"GT-N7000",		"",			DEVICE_HAS_BUILTIN_AEC,	0 },  /*Galaxy Note*/
	{	"samsung",	"GT-N7100",		"",			DEVICE_HAS_BUILTIN_AEC,	0 },  /*Galaxy Note 2*/
	{	"samsung",	"GT-N7105",		"",			DEVICE_HAS_BUILTIN_AEC,	0 },  /*Galaxy Note 2*/
	{	"samsung",	"SGH-T889",		"",			DEVICE_HAS_BUILTIN_AEC,	0 },  /*Galaxy Note 2*/
	{	"samsung",	"Nexus S",		"s5pc110",	DEVICE_HAS_BUILTIN_AEC_CRAPPY,	180 }, /*Nexus S gives calibration around 240ms, but in practice the internal buffer size shrinks after a couple of seconds.*/
	{	"samsung",	"Galaxy Nexus", "",			0,	120 },
	{	"samsung",	"GT-S5570I",	"",			0,	250},
	{	"samsung",	"GT-P3100",		"",			DEVICE_HAS_BUILTIN_AEC, 0 }, /* Galaxy Tab*/
	{	"samsung",	"GT-P7500",		"",			DEVICE_HAS_BUILTIN_AEC, 0 }, /* Galaxy Tab*/
	{	"samsung",	"GT-P7510",		"",			DEVICE_HAS_BUILTIN_AEC, 0 }, /* Galaxy Tab*/
	{	"samsung",	"GT-I915",		"",			DEVICE_HAS_BUILTIN_AEC, 0 }, /* Verizon Tab*/
	{	"samsung",	"GT-I8190N",	"montblanc",	DEVICE_HAS_BUILTIN_AEC, 0, 16000 }, /* Galaxy S3 Mini*/
	{	"samsung",	"GT-I8190",	"montblanc",	DEVICE_HAS_BUILTIN_AEC,	0, 16000 },  /*Galaxy S3 mini*/
	
	
	{	"Sony Ericsson","ST15a",	"",			0, 	150 },
	{	"Sony Ericsson","S51SE",	"",			0,	150 },
	{	"Sony Ericsson","SK17i",	"",			0,	140 },
	{	"Sony Ericsson","ST17i",	"",			0,	130 },
	{	"Sony Ericsson","ST18i",	"",			0,	140 },
	{	"Sony Ericsson","ST25i",	"",			0,	320 },
	{	"Sony Ericsson","ST27i",	"",			0,	320 },
	{	"Sony Ericsson","LT15i",	"",			0,	150 },
	{	"Sony Ericsson","LT18i",	"",			0,	150 },
	{	"Sony Ericsson","LT26i",	"",			0,	230 },
	{	"Sony Ericsson","LT26ii",	"",			0,	230 },
	{	"Sony Ericsson","LT28h",	"",			0,	210 },
	{	"Sony Ericsson","MT11i",	"",			0,	150 },
	{	"Sony Ericsson","MT15i",	"",			0,	150 },
	{	"Sony Ericsson","ST15i",	"msm7x30",		0,	150 },
	
	{	"asus",		"Nexus 7",	"", 			0, 170},
	{	"asus",		"K00E",		"clovertrail", 	0, 200},
	
	{	"Amazon",		"KFTT",		"omap4",	DEVICE_USE_ANDROID_MIC,200},
	{	"LENOVO",		"Lenovo B6000-F",		"",DEVICE_HAS_BUILTIN_AEC_CRAPPY,300},
	{	NULL, NULL, NULL, 0, 0,0}
};

static SoundDeviceDescription undefined={"Generic", "Generic", "Generic", 0, 250, 0};

#else

static SoundDeviceDescription devices[]={
	{	NULL, NULL, NULL, 0, 0, 0}
};

static SoundDeviceDescription undefined={"Generic", "Generic", "Generic", 0, 0, 0};

#endif

static SoundDeviceDescription *lookup_by_model(const char *manufacturer, const char* model){
	SoundDeviceDescription *d=&devices[0];
	while (d->manufacturer!=NULL) {
		if (strcasecmp(d->manufacturer,manufacturer)==0 && strcmp(d->model,model)==0){
			return d;
		}
		d++;
	}
	return NULL;
}

static SoundDeviceDescription *lookup_by_platform(const char *platform){
	SoundDeviceDescription *d=&devices[0];
	while (d->manufacturer!=NULL){
		if (strcmp(d->platform,platform)==0){
			return d;
		}
		d++;
	}
	return NULL;
}

#ifndef PROV_VALUE_MAX
#define PROP_VALUE_MAX 256
#endif

SoundDeviceDescription * sound_device_description_get(void){
	SoundDeviceDescription *d;
	char manufacturer[PROP_VALUE_MAX]={0};
	char model[PROP_VALUE_MAX]={0};
	char platform[PROP_VALUE_MAX]={0};
	
	bool_t exact_match=FALSE;
	bool_t declares_builtin_aec=FALSE;

#ifdef ANDROID
	
	if (__system_property_get("ro.product.manufacturer",manufacturer)<=0){
		ms_warning("Could not get product manufacturer.");
	}
	if (__system_property_get("ro.product.model",model)<=0){
		ms_warning("Could not get product model.");
	}
	if (__system_property_get("ro.board.platform",platform)<=0){
		ms_warning("Could not get board platform.");
	}
	
	/* First ask android if the device has an hardware echo canceller (only >=4.2)*/
	{
		JNIEnv *env=ms_get_jni_env();
		jclass aecClass = (*env)->FindClass(env,"android/media/audiofx/AcousticEchoCanceler");
		if (aecClass!=NULL){
			jmethodID isAvailableID = (*env)->GetStaticMethodID(env,aecClass,"isAvailable","()Z");
			if (isAvailableID!=NULL){
				jboolean ret=(*env)->CallStaticBooleanMethod(env,aecClass,isAvailableID);
				if (ret){
					ms_message("This device (%s/%s/%s) declares it has a built-in echo canceller.",manufacturer,model,platform);
					declares_builtin_aec=TRUE;
				}else ms_message("This device (%s/%s/%s) says it has no built-in echo canceller.",manufacturer,model,platform);
			}else{
				ms_error("isAvailable() not found in class AcousticEchoCanceler !");
				(*env)->ExceptionClear(env); //very important.
			}
			(*env)->DeleteLocalRef(env,aecClass);
		}else{
			(*env)->ExceptionClear(env); //very important.
		}
	}
	
#endif
	
	d=lookup_by_model(manufacturer,model);
	if (!d){
		ms_message("No AEC information available for model [%s/%s], trying with platform name [%s].",manufacturer,model,platform);
		d=lookup_by_platform(platform);
		if (!d){
			ms_message("No AEC information available for platform [%s].",platform);
		}
	}else exact_match=TRUE;
	
	if (d) {
		ms_message("Found AEC information for [%s/%s/%s] from internal table: builtin=[%s], delay=[%i] ms",
				manufacturer,model,platform, (d->flags & DEVICE_HAS_BUILTIN_AEC) ? "yes" : "no", d->delay);
	}else d=&undefined;
	
	if (declares_builtin_aec){
		if (exact_match && (d->flags & DEVICE_HAS_BUILTIN_AEC_CRAPPY)){
			ms_warning("This device declares a builtin AEC but according to internal tables it is known to be misfunctionning, so trusting tables.");
		}else{
			d->flags=DEVICE_HAS_BUILTIN_AEC;
			d->delay=0;
		}
	}
	return d;
}

