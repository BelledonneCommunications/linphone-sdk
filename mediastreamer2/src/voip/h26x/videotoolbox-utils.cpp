/*
 Mediastreamer2 videotoolbox-utils.h
 Copyright (C) 2018 Belledonne Communications SARL

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <sstream>
#include <unordered_map>

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
	{ kVTVideoDecoderRemovedErr                 , "video decoder removed"                   }
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

} // namespace mediastreamer
