/*
 * android_echo.h -Android echo cancellation utilities.
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

#include <jni.h>

#include "android_echo.h"
#include "sys/system_properties.h"

struct EcDescription{
	const char *manufacturer;
	const char *model;
	const char *platform;
	int has_builtin_ec;
	int delay;
};

static EcDescription ec_table[]={
	{	"HTC",		"Nexus One",	"qsd8k",	FALSE,	300 },
	{	"HTC",		"HTC One X",	"tegra",	FALSE,	150 },	//has a very good acoustic isolation, which result in calibration saying no echo. 
																//But with speaker mode there is a strong echo if software ec is disabled.
	{	"HTC",		"HTC Desire",	"",			FALSE,	250 },
	{	"HTC",		"HTC Sensation Z710e",	"",	FALSE,	200 },
	{	"HTC",		"HTC Wildfire",	"",			FALSE,	270 },
	
	
	{	"LGE",		"Nexus 4",		"msm8960",	FALSE,	230 },
	{	"LGE",		"LS670",		"",			FALSE,	170 },
	
	{	"motorola",	"DROID RAZR",	"",			FALSE,	400 },
	{	"motorola",	"MB860",		"",			FALSE,	200 },
	{	"motorola",	"XT907",		"",			FALSE,	500 },

	{	"samsung",	"GT-S5360",		"bcm21553",	FALSE,	250 }, /*<Galaxy Y*/
	{	"samsung",	"GT-S5360L",	"",			FALSE,	250 }, /*<Galaxy Y*/
	{	"samsung",	"GT-S6102",		"",			TRUE,	0 }, /*<Galaxy Y duo*/
	{	"samsung",	"GT-S5570",		"",			FALSE,	160 }, /*<Galaxy Y duo*/
	{	"samsung",	"GT-S5300",		"",			TRUE,	0 }, /*<Galaxy Pocket*/
	{	"samsung",	"GT-S5830i",		"",		FALSE,	200 }, /* Galaxy S */
	{	"samsung",	"GT-S5830",		"",			FALSE,	170 }, /* Galaxy S */
	{	"samsung",	"GT-S5660",		"",			FALSE,	160 }, /* Galaxy Gio */
	{	"samsung",	"GT-I9000",		"",			FALSE,	200 }, /* Galaxy S */
	{	"samsung",	"GT-I9001",		"",			FALSE,	150 }, /* Galaxy S+ */
	{	"samsung",	"GT-I9070",		"",			TRUE,	0 }, /* Galaxy S Advance */
	{	"samsung",	"SPH-D700",		"",			FALSE,	200 }, /* Galaxy S Epic 4G*/
	{	"samsung",	"GT-I9100",		"",			TRUE,	0 }, /*Galaxy S2*/
	{	"samsung",	"GT-I9100P",	"s5pc210",	TRUE,	0 }, /*Galaxy S2*/
	{	"samsung",	"GT-S7562",		"",			TRUE,	0 }, /*<Galaxy S Duo*/
	{	"samsung",	"SCH-I415",		"",			TRUE,	0 }, /* Galaxy S ??*/
	{	"samsung",	"SCH-I425",		"",			TRUE,	0 }, /* Galaxy S ??*/
	{	"samsung",	"SCH-I535",		"",			TRUE,	0 }, /* Galaxy S ??*/
	{	"samsung",	"SPH-D710",		"",			TRUE,	0 }, /* Galaxy S2 Epic 4G*/
	{	"samsung",	"GT-I9300",		"exynos4",	TRUE,	0 },  /*Galaxy S3*/
	{	"samsung",	"SAMSUNG-SGH-I747","",		TRUE,	0 }, /* Galaxy S3*/
	{	"samsung",	"SPH-L710","",				TRUE,	0 }, /* Galaxy S3*/
	{	"samsung",	"SPH-D710","",				TRUE,	0 }, /* Galaxy S3*/
	{	"samsung",	"SGH-T999",		"",			TRUE,	0 },  /*Galaxy S3*/
	{	"samsung",	"GT-I8190",		"",			TRUE,	0 },  /*Galaxy S3*/
	{	"samsung",	"SAMSUNG-SGH-I337","",		TRUE,	0 }, /* Galaxy S4 ? */
	{	"samsung",	"GT-N7000",		"",			TRUE,	0 },  /*Galaxy Note*/
	{	"samsung",	"GT-N7100",		"",			TRUE,	0 },  /*Galaxy Note 2*/
	{	"samsung",	"GT-N7105",		"",			TRUE,	0 },  /*Galaxy Note 2*/
	{	"samsung",	"SGH-T889",		"",			TRUE,	0 },  /*Galaxy Note 2*/
	{	"samsung",	"Nexus S",		"s5pc110",	FALSE,	200 },
	{	"samsung",	"Galaxy Nexus", "",			FALSE,	120 },
	{	"samsung",	"GT-S5570I",	"",			FALSE,	250},
	{	"samsung",	"GT-P3100",		"",			TRUE, 0 }, /* Galaxy Tab*/
	{	"samsung",	"GT-P7500",		"",			TRUE, 0 }, /* Galaxy Tab*/
	{	"samsung",	"GT-P7510",		"",			TRUE, 0 }, /* Galaxy Tab*/
	{	"samsung",	"GT-I915",		"",			TRUE, 0 }, /* Verizon Tab*/
	
	
	{	"Sony Ericsson","SK17i",	"",			FALSE,	140 },
	{	"Sony Ericsson","ST17i",	"",			FALSE,	130 },
	{	"Sony Ericsson","ST18i",	"",			FALSE,	140 },
	{	"Sony Ericsson","ST25i",	"",			FALSE,	320 },
	{	"Sony Ericsson","ST27i",	"",			FALSE,	320 },
	{	"Sony Ericsson","LT15i",	"",			FALSE,	150 },
	{	"Sony Ericsson","LT18i",	"",			FALSE,	150 },
	{	"Sony Ericsson","LT26i",	"",			FALSE,	230 },
	{	"Sony Ericsson","LT26ii",	"",			FALSE,	230 },
	{	"Sony Ericsson","LT28h",	"",			FALSE,	210 },
	{	"Sony Ericsson","MT11i",	"",			FALSE,	150 },
	{	"Sony Ericsson","MT15i",	"",			FALSE,	150 },
	
	{	"asus",			"Nexus 7",	"",			FALSE,	170 },
	
	{	NULL, NULL, NULL, FALSE, 0}
};

static EcDescription *lookup_by_model(const char *manufacturer, const char* model){
	int i;
	
	for(i=0;ec_table[i].manufacturer!=NULL;i++){
		EcDescription *d=&ec_table[i];
		if (strcasecmp(d->manufacturer,manufacturer)==0 && strcmp(d->model,model)==0){
			return d;
		}
	}
	return NULL;
}

static EcDescription *lookup_by_platform(const char *platform){
	int i;
	
	for(i=0;ec_table[i].manufacturer!=NULL;i++){
		EcDescription *d=&ec_table[i];
		if (strcmp(d->platform,platform)==0){
			return d;
		}
	}
	return NULL;
}

int android_sound_get_echo_params(EchoCancellerParams *params){
	EcDescription *d;
	char manufacturer[PROP_VALUE_MAX]={0};
	char model[PROP_VALUE_MAX]={0};
	char platform[PROP_VALUE_MAX]={0};

	if (__system_property_get("ro.product.manufacturer",manufacturer)<=0){
		ms_warning("Could not get product manufacturer.");
		return -1;
	}
	if (__system_property_get("ro.product.model",model)<=0){
		ms_warning("Could not get product model.");
		return -1;
	}
	if (__system_property_get("ro.board.platform",platform)<=0){
		ms_warning("Could not get board platform.");
	}
	
	/* First ask android if the device has an hardware echo canceller (only >=4.2)*/
	{
		JNIEnv *env=ms_get_jni_env();
		jclass aecClass = env->FindClass("android/media/audiofx/AcousticEchoCanceler");
		if (aecClass!=NULL){
			aecClass= (jclass)env->NewGlobalRef(aecClass);
			jmethodID isAvailableID = env->GetStaticMethodID(aecClass,"isAvailable","()Z");
			if (isAvailableID!=NULL){
				jboolean ret=env->CallStaticBooleanMethod(aecClass,isAvailableID);
				if (ret){
					ms_message("This device (%s/%s/%s) has a built-in echo canceller.",manufacturer,model,platform);
					params->has_builtin_ec=TRUE;
					params->delay=0;
					env->DeleteGlobalRef(aecClass);
					return 0;
				}else ms_message("This device (%s/%s/%s) says it has no built-in echo canceller.",manufacturer,model,platform);
			}else{
				ms_error("isAvailable() not found in class AcousticEchoCanceler !");
				env->ExceptionClear(); //very important.
			}
			env->DeleteGlobalRef(aecClass);
		}else{
			env->ExceptionClear(); //very important.
		}
	}
	
	d=lookup_by_model(manufacturer,model);
	if (!d){
		ms_warning("Lookup by model (%s/%s) failed.",manufacturer,model);
		d=lookup_by_platform(platform);
		if (!d){
			ms_warning("Lookup by platform (%s) also failed.",platform);
			return -1;
		}
	}
	ms_message("Found echo cancellation information for %s/%s/%s: builtin=%s, delay=%i ms",
				manufacturer,model,platform,d->has_builtin_ec ? "yes" : "no", d->delay);
	params->has_builtin_ec=d->has_builtin_ec;
	params->delay=d->delay;
	return 0;
}

