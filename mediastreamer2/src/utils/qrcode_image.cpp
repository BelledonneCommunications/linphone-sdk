/*
 * mediastreamer2 library - modular sound and video processing and streaming
 * Copyright (C) 2018  Simon MORLAT (simon.morlat@linphone.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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
