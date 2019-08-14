/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef qrcode_image_h
#define qrcode_image_h

#include <zxing/LuminanceSource.h>

class QRCodeImage : public zxing::LuminanceSource {
public:
	QRCodeImage(int width, int height, const uint8_t *y, int ySize);

	zxing::Ref<LuminanceSource> getLuminanceSource();

	zxing::ArrayRef<char> getRow(int y, zxing::ArrayRef<char> row) const;
	zxing::ArrayRef<char> getMatrix() const;
private:
	zxing::ArrayRef<char> frame;
};

#endif

