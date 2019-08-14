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

#include "qrcode_image.hpp"

using zxing::LuminanceSource;
using zxing::ArrayRef;
using zxing::Ref;

QRCodeImage::QRCodeImage(int width, int height, const uint8_t *y, int ySize) : LuminanceSource(width, height) {
	this->frame = ArrayRef<char>(const_cast<char*>(reinterpret_cast<const char*>(y)), ySize * height);
}

Ref<LuminanceSource> QRCodeImage::getLuminanceSource() {
	return Ref<LuminanceSource>(this);
}

ArrayRef<char> QRCodeImage::getRow(int y, ArrayRef<char> row) const {
	const char* pixelRow = &frame[0] + y * getWidth();
	if (!row) row = ArrayRef<char>(getWidth());
	for (int x = 0; x < getWidth(); x++) {
		row[x] = pixelRow[0];
	}
	return row;
}

ArrayRef<char> QRCodeImage::getMatrix() const {
	return this->frame;
}
