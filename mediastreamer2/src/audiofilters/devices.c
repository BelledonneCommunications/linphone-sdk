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



#include "mediastreamer2/devices.h"

#ifdef ANDROID
#include "sys/system_properties.h"
#include <jni.h>
#endif



#ifdef ANDROID
/*
 * 1st column: list of triplet frequency, gain, width
 * 2nd column: mic gain in db
*/
static SoundDeviceAudioHacks SonySGP511Hacks = { "50:0.01:100 600:0.2:100 1350:0.2:100 2000:0.2:100", -4, NULL, 0 };

/*
 * 1st column: value of ro.product.manufacturer
 * 2nd column: value of ro.product.model
 * 3rd column: value of ro.board.platform (and is optional)
 * All these values can be obtained from the device using "adb shell getprop"
*/
static SoundDeviceDescription devices[]={
	{	"HTC",					"Nexus One",			"qsd8k",		0,	300 },
	{	"HTC",					"HTC One X",			"tegra",		0,	150 },	/*has a very good acoustic isolation, which result in calibration saying no echo. */
										/*/But with speaker mode there is a strong echo if software ec is disabled.*/
	{	"HTC",					"HTC One SV",			"msm8960",		0,	200 },
	{	"HTC",					"HTC Desire",			"",				0,	250 },
	{	"HTC",					"HTC Sensation Z710e",	"",				0,	200 },
	{	"HTC",					"HTC Wildfire",			"",				0,	270 },
	{	"HTC",					"HTC One mini 2",		"",				DEVICE_HAS_BUILTIN_AEC|DEVICE_HAS_BUILTIN_OPENSLES_AEC, 0, 0},
	{	"HTC",					"0PCV1",				"msm8226",		DEVICE_HAS_BUILTIN_AEC|DEVICE_HAS_BUILTIN_OPENSLES_AEC|DEVICE_HAS_CRAPPY_ANDROID_FASTTRACK, 0, 0},
	{	"HTC",					"HTC Desire 610",		"msm8226",		DEVICE_HAS_BUILTIN_AEC|DEVICE_HAS_BUILTIN_OPENSLES_AEC|DEVICE_HAS_CRAPPY_ANDROID_FASTTRACK, 0, 0},
	{	"OnePlus",				"A0001",        		"msm8974",      0,      120 },
	{	"HTC",					"HTC One_M8",   		"msm8974",      0,      120 },
	{	"LGE",					"LS670",				"",				0,	170 },
	{	"LGE",					"Nexus 5",				"msm8974",		0,	0 , 16000 },
	{	"LGE", 					"LG-H815",				"msm8992",	    DEVICE_HAS_BUILTIN_AEC, 0 },
	{	"LGE", 					"LG-H735", 				"msm8916",	    DEVICE_HAS_BUILTIN_OPENSLES_AEC, 0, 16000},
	{   "LGE",                  "LG-H850",              "msm8996",      DEVICE_HAS_BUILTIN_OPENSLES_AEC, 0}, /* LG5 */
	{	"motorola",				"DROID RAZR",			"",				0,	400 },
	{	"motorola",				"MB860",				"",				0,	200 },
	{	"motorola",				"XT907",				"",				0,	500 },
	{	"motorola",				"DROIX X2",				"",				0,	320 },
	{	"motorola",				"MotoG3",				"msm8916",		DEVICE_HAS_BUILTIN_AEC_CRAPPY,	100 }, /*The MotoG3 audio capture hangs for several seconds when switching to speaker mode*/
	{   "motorola",             "Nexus 6",              "msm8084",      DEVICE_HAS_BUILTIN_OPENSLES_AEC, 0}, /* Nexus 6*/
	{	"samsung",				"GT-S5360",				"bcm21553",		0,	250 }, /*<Galaxy Y*/
	{	"samsung",				"GT-S5360L",			"",				0,	250 }, /*<Galaxy Y*/
	{	"samsung",				"GT-S6102",				"",				DEVICE_HAS_BUILTIN_AEC,	0 }, /*<Galaxy Y duo*/
	{	"samsung",				"GT-S5570",				"",				0,	160 }, /*<Galaxy Y duo*/
	{	"samsung",				"GT-S5300",				"",				DEVICE_HAS_BUILTIN_AEC,	0 }, /*<Galaxy Pocket*/
	{	"samsung",				"GT-S5830i",			"",				0,	200 }, /* Galaxy S */
	{	"samsung",				"GT-S5830",				"",				0,	170 }, /* Galaxy S */
	{	"samsung",				"GT-S5660",				"",				0,	160 }, /* Galaxy Gio */
	{	"samsung",				"GT-I9000",				"",				0,	200 }, /* Galaxy S */
	{	"samsung",				"GT-I9001",				"",				0,	150 }, /* Galaxy S+ */
	{	"samsung",				"GT-I9070",				"",				DEVICE_HAS_BUILTIN_AEC,	0 }, /* Galaxy S Advance */
	{	"samsung",				"SPH-D700",				"",				0,	200 }, /* Galaxy S Epic 4G*/
	{	"samsung",				"GT-I9100",				"",				DEVICE_HAS_BUILTIN_AEC | DEVICE_HAS_BUILTIN_OPENSLES_AEC,	0 }, /*Galaxy S2*/
	{	"samsung",				"GT-I9100P",			"s5pc210",		DEVICE_HAS_BUILTIN_AEC | DEVICE_HAS_BUILTIN_OPENSLES_AEC,	0 }, /*Galaxy S2*/
	{	"samsung",				"GT-S7562",				"",				DEVICE_HAS_BUILTIN_AEC,	0 }, /*<Galaxy S Duo*/
	{	"samsung",				"SCH-I415",				"",				DEVICE_HAS_BUILTIN_AEC,	0 }, /* Galaxy S ??*/
	{	"samsung",				"SCH-I425",				"",				DEVICE_HAS_BUILTIN_AEC,	0 }, /* Galaxy S ??*/
	{	"samsung",				"SCH-I535",				"",				DEVICE_HAS_BUILTIN_AEC,	0 }, /* Galaxy S ??*/
	{	"samsung",				"GT-I9300",				"exynos4",		DEVICE_HAS_BUILTIN_AEC | DEVICE_HAS_BUILTIN_OPENSLES_AEC,	0 },  /*Galaxy S3*/
	{	"samsung",				"SAMSUNG-SGH-I747",		"",				DEVICE_HAS_BUILTIN_AEC | DEVICE_HAS_BUILTIN_OPENSLES_AEC,	0 }, /* Galaxy S3*/
	{	"samsung",				"SPH-L710",				"",				DEVICE_HAS_BUILTIN_AEC | DEVICE_HAS_BUILTIN_OPENSLES_AEC,	0 }, /* Galaxy S3*/
	{	"samsung",				"SPH-D710",				"",				DEVICE_HAS_BUILTIN_AEC | DEVICE_HAS_BUILTIN_OPENSLES_AEC,	0 }, /* Galaxy S3*/
	{	"samsung",				"SGH-T999",				"",				DEVICE_HAS_BUILTIN_AEC | DEVICE_HAS_BUILTIN_OPENSLES_AEC,	0 },  /*Galaxy S3*/
	{	"samsung",				"GT-I9305",				"",				DEVICE_HAS_BUILTIN_AEC | DEVICE_HAS_UNSTANDARD_LIBMEDIA | DEVICE_HAS_BUILTIN_OPENSLES_AEC, 0 }, /*Galaxy S3*/
	{	"samsung",				"SAMSUNG-SGH-I337",		"",				DEVICE_HAS_BUILTIN_AEC | DEVICE_HAS_BUILTIN_OPENSLES_AEC,	0 }, /* Galaxy S4 ? */
	{	"samsung",				"GT-I9195",				"",				DEVICE_HAS_BUILTIN_AEC | DEVICE_HAS_BUILTIN_OPENSLES_AEC,	0 }, /* Galaxy S4 mini*/
	{   "samsung",              "SM-G900F",             "msm8974",      DEVICE_HAS_BUILTIN_OPENSLES_AEC, 0 }, /* Galaxy S5 */
	{   "samsung",              "SM-G920F",             "exynos5",      DEVICE_HAS_BUILTIN_OPENSLES_AEC, 0 }, /* Galaxy S6*/
	{   "samsung",              "SM-G930F",             "exynos5",      DEVICE_HAS_BUILTIN_OPENSLES_AEC, 0 }, /* Galaxy S7*/
	{	"samsung",				"GT-N7000",				"",				DEVICE_HAS_BUILTIN_AEC,	0 },  /*Galaxy Note*/
	{	"samsung",				"GT-N7100",				"exynos4",		DEVICE_HAS_BUILTIN_AEC|DEVICE_HAS_UNSTANDARD_LIBMEDIA, 0 }, /*Galaxy Note 2  */
	{	"samsung",				"GT-N7105",				"",				DEVICE_HAS_BUILTIN_AEC|DEVICE_HAS_UNSTANDARD_LIBMEDIA,	0 },  /*Galaxy Note 2 t0lte*/
	{	"samsung",				"SGH-T889",				"",				DEVICE_HAS_BUILTIN_AEC|DEVICE_HAS_UNSTANDARD_LIBMEDIA,	0 },  /*Galaxy Note 2 t0lte*/
	{	"samsung",				"SGH-I317",				"",				DEVICE_HAS_BUILTIN_AEC|DEVICE_HAS_UNSTANDARD_LIBMEDIA, 0 }, /*Galaxy Note 2 t0lte*/
	{	"samsung",				"SPH-L900",				"",				DEVICE_HAS_BUILTIN_AEC|DEVICE_HAS_UNSTANDARD_LIBMEDIA, 0 }, /*Galaxy Note 2 t0ltespr*/
	{	"samsung",				"Nexus S",				"s5pc110",		DEVICE_HAS_BUILTIN_AEC_CRAPPY,	180 }, /*Nexus S gives calibration around 240ms, but in practice the internal buffer size shrinks after a couple of seconds.*/
	{	"samsung",				"Galaxy Nexus", 		"",				0,	120 },
	{	"samsung",				"GT-I9250",				"",				DEVICE_HAS_BUILTIN_AEC|DEVICE_HAS_UNSTANDARD_LIBMEDIA, 0 }, /*galaxy nexus (maguro)*/
	{	"samsung",				"GT-S5570I",			"",				0,	250},
	{	"samsung",				"GT-P3100",				"",				DEVICE_HAS_BUILTIN_AEC, 0 }, /* Galaxy Tab*/
	{	"samsung",				"GT-P7500",				"",				DEVICE_HAS_BUILTIN_AEC, 0 }, /* Galaxy Tab*/
	{	"samsung",				"GT-P7510",				"",				DEVICE_HAS_BUILTIN_AEC, 0 }, /* Galaxy Tab*/
	{	"samsung",				"GT-I915",				"",				DEVICE_HAS_BUILTIN_AEC, 0 }, /* Verizon Tab*/
	{	"samsung",				"GT-I8190N",			"montblanc",	DEVICE_HAS_BUILTIN_AEC | DEVICE_HAS_BUILTIN_OPENSLES_AEC, 0, 16000 }, /* Galaxy S3 Mini*/
	{	"samsung",				"GT-I8190",				"montblanc",	DEVICE_HAS_BUILTIN_AEC | DEVICE_HAS_BUILTIN_OPENSLES_AEC,	0, 16000 },  /*Galaxy S3 mini*/
	{	"samsung",				"SM-T230",				"mrvl",			DEVICE_HAS_BUILTIN_AEC_CRAPPY, 200, 0},			/*Galaxy Tab 4 wifi*/
	{	"samsung",				"GT-S7580",				"hawaii",		DEVICE_HAS_CRAPPY_OPENGL | DEVICE_HAS_BUILTIN_AEC, 0},
	{	"samsung",				"SHV-E210S",			"",				DEVICE_HAS_UNSTANDARD_LIBMEDIA | DEVICE_HAS_BUILTIN_AEC, 0 },
	{	"samsung",				"SHV-E210L",			"",				DEVICE_HAS_UNSTANDARD_LIBMEDIA | DEVICE_HAS_BUILTIN_AEC, 0 },
	{	"samsung",				"SHV-E220S",			"",				DEVICE_HAS_UNSTANDARD_LIBMEDIA | DEVICE_HAS_BUILTIN_AEC, 0 },
	{	"samsung",				"SGH-I317",				"",				DEVICE_HAS_UNSTANDARD_LIBMEDIA | DEVICE_HAS_BUILTIN_AEC, 0 },
	{	"samsung",				"SHV-E230K",			"",				DEVICE_HAS_UNSTANDARD_LIBMEDIA, 0 },
	{	"samsung",				"SM-G530F",				"",				DEVICE_HAS_UNSTANDARD_LIBMEDIA, 0 },
	{	"samsung",				"SM-T315",				"",				DEVICE_HAS_UNSTANDARD_LIBMEDIA, 0 },


	{	"Sony Ericsson",		"ST15a",				"",				0, 	150 },
	{	"Sony Ericsson",		"S51SE",				"",				0,	150 },
	{	"Sony Ericsson",		"SK17i",				"",				0,	140 },
	{	"Sony Ericsson",		"ST17i",				"",				0,	130 },
	{	"Sony Ericsson",		"ST18i",				"",				0,	140 },
	{	"Sony Ericsson",		"ST25i",				"",				0,	320 },
	{	"Sony Ericsson",		"ST27i",				"",				0,	320 },
	{	"Sony Ericsson",		"LT15i",				"",				0,	150 },
	{	"Sony Ericsson",		"LT18i",				"",				0,	150 },
	{	"Sony Ericsson",		"LT26i",				"",				0,	230 },
	{	"Sony Ericsson",		"LT26ii",				"",				0,	230 },
	{	"Sony Ericsson",		"LT28h",				"",				0,	210 },
	{	"Sony Ericsson",		"MT11i",				"",				0,	150 },
	{	"Sony Ericsson",		"MT15i",				"",				0,	150 },
	{	"Sony Ericsson",		"ST15i",				"msm7x30",		0,	150 },
	{	"Sony",					"SGP511",				"msm8974",		DEVICE_HAS_BUILTIN_AEC_CRAPPY,	130, 0, &SonySGP511Hacks},
	{ 	"Sony", 				"D6503", 				"msm8974",		DEVICE_HAS_BUILTIN_AEC_CRAPPY,	280},
	{ 	"Sony", 				"D6603", 				"msm8974",		DEVICE_HAS_BUILTIN_AEC_CRAPPY,	280},
	{	"Sony",					"D2005",				"",				DEVICE_HAS_UNSTANDARD_LIBMEDIA,	0},

	{	"asus",					"Nexus 7",				"", 			0, 170},
	{	"asus",					"K00E",					"clovertrail", 	0, 200},

	{	"Amazon",				"KFTT",					"omap4",		DEVICE_USE_ANDROID_MIC,200},
	{	"LENOVO",				"Lenovo B6000-F",		"",				DEVICE_HAS_BUILTIN_AEC_CRAPPY,300},
	{	"LENOVO",				"Lenovo S60-a",			"msm8916",		0,	0 ,	44100},
	{	"LENOVO",				"Lenovo A6000",			"",				DEVICE_HAS_UNSTANDARD_LIBMEDIA, 0},
	{	"LENOVO",				"Lenovo A616",			"",				DEVICE_HAS_UNSTANDARD_LIBMEDIA, 0},

	{	"Enspert",				"IGGY",					""		,		0,	320 ,0}, /*Wiko iggy*/
	{	"Yota Devices Limited", "YD201", 				"msm8974", 		DEVICE_HAS_BUILTIN_AEC | DEVICE_HAS_BUILTIN_OPENSLES_AEC, 0, 48000 }, /* Yotaphone 2 */

	{	"Hewlett-Packard",		"Slate 21 Pro",			"tegra4", 		DEVICE_HAS_UNSTANDARD_LIBMEDIA, 250  },
	{	"blackberry",			"STV100-4",				"msm8992",		DEVICE_HAS_BUILTIN_AEC | DEVICE_HAS_BUILTIN_OPENSLES_AEC, 0 , 48000},

	{	"Vodafone",				"Vodafone 985N",				"",				DEVICE_HAS_UNSTANDARD_LIBMEDIA, 0 },
	{	"Acer",					"S57",							"",				DEVICE_HAS_UNSTANDARD_LIBMEDIA, 0 },
	{	"Alcatel",				"4027D",						"",				DEVICE_HAS_UNSTANDARD_LIBMEDIA, 0 },

	{	"HUAWEI",				"HUAWEI P7-L10",				"",				DEVICE_HAS_UNSTANDARD_LIBMEDIA, 0 },
	{	"HUAWEI",				"HUAWEI P7-L11",				"",				DEVICE_HAS_UNSTANDARD_LIBMEDIA, 0 },
	{	"HUAWEI",				"HUAWEI P7-L12",				"",				DEVICE_HAS_UNSTANDARD_LIBMEDIA, 0 },
	{	"HUAWEI",				"HUAWEI MT7-L09",				"",				DEVICE_HAS_UNSTANDARD_LIBMEDIA, 0 },
	{ "WIKO", 			"HIGHWAY 4G", 				"tegra", 			DEVICE_HAS_UNSTANDARD_LIBMEDIA, 0 },

	{	NULL, 					NULL, 					NULL,			0, 	0,	0}
};

SoundDeviceDescription genericSoundDeviceDescriptor={"Generic", "Generic", "Generic", 0, 250, 0, 0};

JNIEXPORT void JNICALL Java_org_linphone_mediastream_MediastreamerAndroidContext_addSoundDeviceDescription(JNIEnv* env, jobject thiz, jstring jmanufacturer, jstring jmodel, jstring jplatform, jint flags, jint delay, jint rate) {
	const char *manufacturer = jmanufacturer ? (*env)->GetStringUTFChars(env, jmanufacturer, NULL) : NULL;
	const char *model = jmodel ? (*env)->GetStringUTFChars(env, jmodel, NULL) : NULL;
	const char *platform = jplatform ? (*env)->GetStringUTFChars(env, jplatform, NULL) : NULL;

	ms_sound_device_description_add(manufacturer, model, platform, flags, delay, rate);

	(*env)->ReleaseStringUTFChars(env, jmanufacturer, manufacturer);
	(*env)->ReleaseStringUTFChars(env, jmodel, model);
	(*env)->ReleaseStringUTFChars(env, jplatform, platform);
}

#else

static SoundDeviceDescription devices[]={
	{ NULL, NULL, NULL, 0, 0, 0, NULL }
};

SoundDeviceDescription genericSoundDeviceDescriptor={"Generic", "Generic", "Generic", 0, 0, 0, NULL};

#endif

static bctbx_list_t *sound_device_descriptions;

static bool_t sound_device_match(SoundDeviceDescription *d, const char *manufacturer, const char* model, const char *platform){
	if (strcasecmp(d->manufacturer, manufacturer) == 0
		&& strcmp(d->model, model) == 0){
		if (platform){
			if (d->platform && strcmp(d->platform, platform)==0) {
				return TRUE;
			}
		}else return TRUE; /*accept a match with only manufacturer and model if platform is not provided*/
	}
	return FALSE;
}

void ms_sound_device_description_add(const char *manufacturer, const char *model, const char *platform, unsigned int flags, int delay, int recommended_rate) {
	SoundDeviceDescription *new_sound_device_description = ms_new0(SoundDeviceDescription, 1);
	new_sound_device_description->manufacturer = ms_strdup(manufacturer);
	new_sound_device_description->model = ms_strdup(model);
	new_sound_device_description->platform = ms_strdup(platform);
	new_sound_device_description->flags = flags;
	new_sound_device_description->delay = delay;
	new_sound_device_description->recommended_rate = recommended_rate;

	sound_device_descriptions = bctbx_list_append(sound_device_descriptions, new_sound_device_description);
}

#ifndef PROP_VALUE_MAX
#define PROP_VALUE_MAX 256
#endif

/***********************************************************************************************************/
/***************************************** MS Devices Informations *****************************************/
/***********************************************************************************************************/

MSDevicesInfo *ms_devices_info_new(void) {
	MSDevicesInfo *devices_info = ms_new0(MSDevicesInfo, 1);

	SoundDeviceDescription *d = &devices[0];
	while (d->manufacturer != NULL) {
		ms_devices_info_add(devices_info, d->manufacturer, d->model, d->platform, d->flags, d->delay, d->recommended_rate);
		d++;
	}

	return devices_info;
}

void ms_devices_info_free(MSDevicesInfo *devices_info) {
	if (devices_info->sound_devices_descriptions) bctbx_list_free(devices_info->sound_devices_descriptions);
	ms_free(devices_info);
}

void ms_devices_info_add(MSDevicesInfo *devices_info, const char *manufacturer, const char *model, const char *platform, unsigned int flags, int delay, int recommended_rate) {
	SoundDeviceDescription *new_sound_device_description = ms_new0(SoundDeviceDescription, 1);
	new_sound_device_description->manufacturer = ms_strdup(manufacturer);
	new_sound_device_description->model = ms_strdup(model);
	new_sound_device_description->platform = ms_strdup(platform);
	new_sound_device_description->flags = flags;
	new_sound_device_description->delay = delay;
	new_sound_device_description->recommended_rate = recommended_rate;

	devices_info->sound_devices_descriptions = bctbx_list_append(devices_info->sound_devices_descriptions, new_sound_device_description);
}

SoundDeviceDescription* ms_devices_info_lookup_device(MSDevicesInfo *devices_info, const char *manufacturer, const char* model, const char *platform) {
	bctbx_list_t *list = devices_info->sound_devices_descriptions;
	SoundDeviceDescription *d = NULL;

	while(list) {
		d = (SoundDeviceDescription*) list->data;
		if (sound_device_match(d, manufacturer, model, platform)) {
			return d;
		}
		list = bctbx_list_next(list);
	}

	if (platform) {
		/*retry without platform*/
		return ms_devices_info_lookup_device(devices_info, manufacturer, model, NULL);
	}

	return NULL;
}

SoundDeviceDescription* ms_devices_info_get_sound_device_description(MSDevicesInfo *devices_info) {
	SoundDeviceDescription *d = NULL;
	char manufacturer[PROP_VALUE_MAX] = {0};
	char model[PROP_VALUE_MAX] = {0};
	char platform[PROP_VALUE_MAX] = {0};
	bool_t exact_match = FALSE;
	bool_t declares_builtin_aec = FALSE;

#ifdef ANDROID

	if (__system_property_get("ro.product.manufacturer", manufacturer) <= 0) {
		ms_warning("Could not get product manufacturer.");
	}
	if (__system_property_get("ro.product.model", model) <= 0) {
		ms_warning("Could not get product model.");
	}
	if (__system_property_get("ro.board.platform", platform) <= 0) {
		ms_warning("Could not get board platform.");
	}

	/* First ask android if the device has an hardware echo canceller (only >=4.2)*/
	{
		JNIEnv *env = ms_get_jni_env();
		jclass aecClass = (*env)->FindClass(env,"android/media/audiofx/AcousticEchoCanceler");
		if (aecClass != NULL) {
			jmethodID isAvailableID = (*env)->GetStaticMethodID(env, aecClass, "isAvailable", "()Z");
			if (isAvailableID != NULL) {
				jboolean ret = (*env)->CallStaticBooleanMethod(env, aecClass, isAvailableID);
				if (ret) {
					ms_message("This device (%s/%s/%s) declares it has a built-in echo canceller.", manufacturer, model, platform);
					declares_builtin_aec = TRUE;
				} else ms_message("This device (%s/%s/%s) says it has no built-in echo canceller.", manufacturer, model, platform);
			} else {
				ms_error("isAvailable() not found in class AcousticEchoCanceler !");
				(*env)->ExceptionClear(env); //very important.
			}
			(*env)->DeleteLocalRef(env, aecClass);
		} else {
			(*env)->ExceptionClear(env); //very important.
		}
	}

#endif

	d = ms_devices_info_lookup_device(devices_info, manufacturer, model, platform);
	if (!d) {
		ms_message("No information available for [%s/%s/%s],", manufacturer, model, platform);
		d = &genericSoundDeviceDescriptor;
	} else {
		ms_message("Found information for [%s/%s/%s] from internal table", manufacturer, model, platform);
		exact_match = TRUE;
	}
	if (declares_builtin_aec) {
		if (exact_match && (d->flags & DEVICE_HAS_BUILTIN_AEC_CRAPPY)) {
			ms_warning("This device declares a builtin AEC but according to internal tables it is known to be misfunctionning, so trusting tables.");
		} else {
			d->flags |= DEVICE_HAS_BUILTIN_AEC;
			d->delay = 0;
		}
	}
	if (d->flags & DEVICE_HAS_CRAPPY_ANDROID_FASTTRACK) ms_warning("Fasttrack playback mode is crappy on this device, not using it.");
	if (d->flags & DEVICE_HAS_CRAPPY_ANDROID_FASTRECORD) ms_warning("Fasttrack record mode is crappy on this device, not using it.");
	if (d->flags & DEVICE_HAS_UNSTANDARD_LIBMEDIA) ms_warning("This device has unstandart libmedia.");
	if (d->flags & DEVICE_HAS_CRAPPY_OPENGL) ms_warning("OpenGL is crappy, not using it.");
	if (d->flags & DEVICE_HAS_CRAPPY_OPENSLES) ms_warning("OpenSles is crappy, not using it.");
	ms_message("Sound device information for [%s/%s/%s] is: builtin=[%s], delay=[%i] ms",
				manufacturer, model, platform, (d->flags & DEVICE_HAS_BUILTIN_AEC) ? "yes" : "no", d->delay);
	return d;
}
