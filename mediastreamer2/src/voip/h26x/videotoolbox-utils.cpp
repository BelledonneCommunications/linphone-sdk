/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <array>
#include <sstream>
#include <unordered_map>

#include <VideoToolbox/VTVideoEncoderList.h>

#include "videotoolbox-h264-utilities.h"
#include "videotoolbox-h265-utilities.h"

#include "videotoolbox-utils.h"

using namespace std;

namespace mediastreamer {

static unordered_map<OSStatus, string> _errorMsg = {
	{ noErr                                     , "no error"                                },
	{ kCVReturnAllocationFailed                 , "return allocation failed"                },
	{ kVTPropertyNotSupportedErr                , "property not supported"                  },
	{ kVTPropertyReadOnlyErr                    , "read only error"                         },
	{ kVTParameterErr                           , "parameter error"                         },
	{ kVTInvalidSessionErr                      , "invalid session"                         },
	{ kVTAllocationFailedErr		            , "allocation failed"                       },
	{ kVTPixelTransferNotSupportedErr           , "pixel transfer not supported"            },
	{ kVTCouldNotFindVideoDecoderErr            , "could not find video decoder"            },
	{ kVTCouldNotCreateInstanceErr              , "could not create instance"               },
	{ kVTCouldNotFindVideoEncoderErr            , "could not find video encoder"            },
	{ kVTVideoDecoderBadDataErr                 , "bad data"                                },
	{ kVTVideoDecoderUnsupportedDataFormatErr   , "unsupported data format"                 },
	{ kVTVideoDecoderMalfunctionErr             , "decoder malfunction"                     },
	{ kVTVideoEncoderMalfunctionErr             , "encoder mulfunction"                     },
	{ kVTVideoDecoderNotAvailableNowErr         , "decoder not available now"               },
	{ kVTImageRotationNotSupportedErr           , "image rotation not supported"            },
	{ kVTVideoEncoderNotAvailableNowErr         , "encoder not available now"               },
	{ kVTFormatDescriptionChangeNotSupportedErr , "format description change not supported" },
	{ kVTInsufficientSourceColorDataErr         , "insufficient source color data"          },
	{ kVTCouldNotCreateColorCorrectionDataErr   , "could not create color correction data"  },
	{ kVTColorSyncTransformConvertFailedErr     , "color sync transform convert failed"     },
	{ kVTVideoDecoderAuthorizationErr           , "video decoder authorization error"       },
	{ kVTVideoEncoderAuthorizationErr           , "video encoder authorization error"       },
	{ kVTColorCorrectionPixelTransferFailedErr  , "color correction pixel transfer failed"  },
	{ kVTMultiPassStorageIdentifierMismatchErr  , "multi-pass storage identifier mismatch"  },
	{ kVTMultiPassStorageInvalidErr             , "multi-pass storage invalid"              },
	{ kVTFrameSiloInvalidTimeStampErr           , "frame silo invalid timestamp"            },
	{ kVTFrameSiloInvalidTimeRangeErr           , "frame silo invalid time range"           },
	{ kVTCouldNotFindTemporalFilterErr          , "could not find temporal filter"          },
	{ kVTPixelTransferNotPermittedErr           , "pixel transfer not permitted"            },
	{ kVTColorCorrectionImageRotationFailedErr  , "color correction image rotation failed"  },
	{ -17690 /* kVTVideoDecoderRemovedErr */    , "video decoder removed"                   }
};

std::string toString(::OSStatus status) {
	ostringstream message;
	unordered_map<OSStatus, string>::const_iterator it = _errorMsg.find(status);
	if (it != _errorMsg.cend()) {
		message << it->second;
	} else {
		message << "unknown error";
	}
	message << " [osstatus=" << status << "]";
	return message.str();
}

bool VideoToolboxUtilities::encoderIsAvailable() const {
	if (_codecAvailability.empty()) loadCodecAvailability();
	return _codecAvailability.at(getCodecType());
}

void VideoToolboxUtilities::getParameterSets(const CMFormatDescriptionRef format, MSQueue *outPs) const {
	size_t offset = 0;
	size_t parameterSetsCount;
	do {
		const uint8_t *parameterSet;
		size_t parameterSetSize;
		getParameterSet(format, offset++, parameterSet, parameterSetSize, parameterSetsCount);

		mblk_t *nalu = allocb(parameterSetSize, 0);
		memcpy(nalu->b_wptr, parameterSet, parameterSetSize);
		nalu->b_wptr += parameterSetSize;
		ms_queue_put(outPs, nalu);
	} while(offset < parameterSetsCount);
}

CMFormatDescriptionRef VideoToolboxUtilities::createFormatDescription(const H26xParameterSetsStore &psStore) const {
	MSQueue parameterSets;
	ms_queue_init(&parameterSets);

	try {
		psStore.fetchAllPs(&parameterSets);

		vector<const uint8_t *> ptrs;
		vector<size_t> sizes;
		for (const mblk_t *ps = ms_queue_peek_first(&parameterSets); !ms_queue_end(&parameterSets, ps); ps = ms_queue_next(&parameterSets, ps)) {
			ptrs.push_back(ps->b_rptr);
			sizes.push_back(msgdsize(ps));
		}

		CMFormatDescriptionRef formatDesc = createFormatDescription(ptrs.size(), ptrs.data(), sizes.data());
		ms_queue_flush(&parameterSets);
		return formatDesc;
	} catch (const AppleOSError &e) {
		ms_queue_flush(&parameterSets);
		throw;
	}
}

VideoToolboxUtilities *VideoToolboxUtilities::create(const std::string &mime) {
	if (mime == "video/avc") {
		return new VideoToolboxH264Utilities();
	} else if (mime == "video/hevc") {
		return new VideoToolboxH265Utilities();
	} else {
		throw invalid_argument(mime + " not supported");
	}
}

void VideoToolboxUtilities::loadCodecAvailability() {
	array<CMVideoCodecType,2> codecTypes = {{ kCMVideoCodecType_H264, kCMVideoCodecType_HEVC }};
	CFArrayRef encoderLists = nullptr;
	VTCopyVideoEncoderList(nullptr, &encoderLists);
	for (auto codecType : codecTypes) {
		_codecAvailability[codecType] = false;
		for (CFIndex i = 0; i < CFArrayGetCount(encoderLists); i++) {
			CMVideoCodecType codecType2;
			auto dict = static_cast<const CFDictionaryRef>(CFArrayGetValueAtIndex(encoderLists, i));
			auto codecTypeNumber = static_cast<const CFNumberRef>(CFDictionaryGetValue(dict, kVTVideoEncoderList_CodecType));
			CFNumberGetValue(codecTypeNumber, kCFNumberIntType, &codecType2);
			if (codecType == codecType2) {
				_codecAvailability[codecType] = true;
				break;
			}
		}
	}
	CFRelease(encoderLists);
}

std::map<CMVideoCodecType,bool> VideoToolboxUtilities::_codecAvailability;

} // namespace mediastreamer
