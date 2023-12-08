/*
 * Copyright (c) 2010-2024 Belledonne Communications SARL.
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

#include <bctoolbox/defs.h>

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/mswebcam.h"

#include "msscreensharing_win.h"

#include <algorithm>
#include <dwmapi.h>
#include <list>
#include <map>
#include <mutex>

MsScreenSharing_win::MsScreenSharing_win(MSScreenSharingDesc sourceDesc, FormatData formatData) : MsScreenSharing() {
	mLastFormat = formatData;
	if (mLastFormat.mPixelFormat == MS_PIX_FMT_UNKNOWN) mLastFormat.mPixelFormat = MS_RGBA32_REV;
	mSourceDesc = sourceDesc;
}

MsScreenSharing_win::~MsScreenSharing_win() {
	stop();
	MsScreenSharing_win::uninit();
}

void MsScreenSharing_win::init() {
	ms_debug("[MsScreenSharing_win] Init");
	if (mSourceDesc.type != MS_SCREEN_SHARING_EMPTY) {
		mRunnable = initDisplay();
		switch (mSourceDesc.type) {
			case MSScreenSharingType::MS_SCREEN_SHARING_DISPLAY:
				mLastFormat.mScreenIndex = *(uintptr_t *)(&mSourceDesc.native_data);
				mWindowId = nullptr;
				break;
			case MSScreenSharingType::MS_SCREEN_SHARING_WINDOW:
				mWindowId = (HWND)mSourceDesc.native_data;
				break;
			case MSScreenSharingType::MS_SCREEN_SHARING_AREA:
				ms_error("[MSScreenSharing] Sharing an area is not supported.");
				break;
			default:
				mRunnable = false;
		}
	} else mRunnable = false;
	MsScreenSharing::init();
}

void MsScreenSharing_win::uninit() {
	ms_debug("[MsScreenSharing_win] Uninit");
	clean();
	MsScreenSharing::uninit();
}

// Driver types supported
D3D_DRIVER_TYPE gDriverTypes[] = {D3D_DRIVER_TYPE_HARDWARE};
UINT gNumDriverTypes = ARRAYSIZE(gDriverTypes);

// Feature levels supported
D3D_FEATURE_LEVEL gFeatureLevels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
                                      D3D_FEATURE_LEVEL_9_1};

UINT gNumFeatureLevels = ARRAYSIZE(gFeatureLevels);

template <class... Ts>
void toRelease(Ts &&...inputs) {
	([&] { inputs->Release(); }(), ...);
}

bool MsScreenSharing_win::initDisplay() {
	// Create device
	D3D_FEATURE_LEVEL lFeatureLevel;
	HRESULT hr;
	for (UINT driverTypeIndex = 0; driverTypeIndex < gNumDriverTypes; ++driverTypeIndex) {
		hr = D3D11CreateDevice(nullptr, gDriverTypes[driverTypeIndex], nullptr, 0, gFeatureLevels, gNumFeatureLevels,
		                       D3D11_SDK_VERSION, &mDevice, &lFeatureLevel, &mImmediateContext);
		if (SUCCEEDED(hr)) {
			// Device creation success, no need to loop anymore
			break;
		}
	}
	if (FAILED(hr)) {
		ms_error("[MsScreenSharing_win] Cannot create Direct3D device [%x]", hr);
		clean();
		return false;
	}
	// Get DXGI device
	IDXGIDevice *dxgiDevice;
	hr = mDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
	if (FAILED(hr)) {
		ms_error("[MsScreenSharing_win] Cannot get DXGI Device [%x]", hr);
		toRelease(dxgiDevice);
		return false;
	}
	// Get DXGI adapter
	IDXGIAdapter *dxgiAdapter;
	hr = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void **>(&dxgiAdapter));
	dxgiDevice->Release();
	if (FAILED(hr)) {
		ms_error("[MsScreenSharing_win] Cannot get DXGI Adapter [%x]", hr);
		toRelease(dxgiAdapter);
		return false;
	}
	UINT Output = 0;
	// Get output
	IDXGIOutput *dxgiOutput;
	hr = dxgiAdapter->EnumOutputs(Output, &dxgiOutput);

	UINT i = 0;
	std::vector<IDXGIOutput *> vOutputs;
	while (dxgiAdapter->EnumOutputs(i, &dxgiOutput) != DXGI_ERROR_NOT_FOUND) {
		IDXGIOutput1 *dxgiOutput1 = nullptr;
		IDXGIOutputDuplication *deskDupl = nullptr;
		vOutputs.push_back(dxgiOutput);
		++i;
		DXGI_OUTPUT_DESC outputDesc;
		hr = dxgiOutput->GetDesc(&outputDesc);
		if (FAILED(hr)) {
			ms_warning("[MsScreenSharing_win] Couldn't get description for the screen %d [%x]", i, hr);
			toRelease(dxgiOutput);
			continue;
		} else
			ms_debug("[MsScreenSharing_win] Screen %i size : %dx%d / %dx%d", outputDesc.DesktopCoordinates.left,
			         outputDesc.DesktopCoordinates.right, outputDesc.DesktopCoordinates.top,
			         outputDesc.DesktopCoordinates.bottom);

		// Query for Output 1

		hr = dxgiOutput->QueryInterface(IID_PPV_ARGS(&dxgiOutput1));
		if (FAILED(hr)) {
			ms_warning("[MsScreenSharing_win] Cannot get DXGI Output1 for screen %d [%x]", i, hr);
			toRelease(dxgiOutput);
			continue;
		}
		// Create desktop duplication

		hr = dxgiOutput1->DuplicateOutput(mDevice, &deskDupl);

		if (FAILED(hr)) {
			ms_warning("[MsScreenSharing_win] Cannot Duplicate screen %d [%x]", i, hr);
			toRelease(dxgiOutput, dxgiOutput1);
			continue;
		}
		// Create GUI drawing texture
		DXGI_OUTDUPL_DESC outputDuplDesc;
		deskDupl->GetDesc(&outputDuplDesc);
		D3D11_TEXTURE2D_DESC desc;
		desc.Width = outputDuplDesc.ModeDesc.Width;
		desc.Height = outputDuplDesc.ModeDesc.Height;
		desc.Format = outputDuplDesc.ModeDesc.Format;
		desc.ArraySize = 1;
		desc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET;
		desc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.MipLevels = 1;
		desc.CPUAccessFlags = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		ID3D11Texture2D *drawingImage;
		hr = mDevice->CreateTexture2D(&desc, NULL, &drawingImage);
		if (FAILED(hr) || !drawingImage) {
			ms_warning("[MsScreenSharing_win] Cannot create drawing Texture2D on screen %d [%x]", i, hr);
			toRelease(dxgiOutput, dxgiOutput1, deskDupl);
			continue;
		}
		// Create CPU access texture
		desc.Width = outputDuplDesc.ModeDesc.Width;
		desc.Height = outputDuplDesc.ModeDesc.Height;
		desc.Format = outputDuplDesc.ModeDesc.Format;
		desc.ArraySize = 1;
		desc.BindFlags = 0;
		desc.MiscFlags = 0;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.MipLevels = 1;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		desc.Usage = D3D11_USAGE_STAGING;
		ID3D11Texture2D *destImage;
		hr = mDevice->CreateTexture2D(&desc, NULL, &destImage);
		if (!FAILED(hr) && destImage) {
			mScreenDuplications.push_back(
			    ScreenDuplication(deskDupl, drawingImage, destImage, outputDesc, outputDuplDesc));
			ms_message("[MsScreenSharing_win] new size: %dx%d", outputDuplDesc.ModeDesc.Width,
			           outputDuplDesc.ModeDesc.Height);
			toRelease(dxgiOutput, dxgiOutput1);
		} else {
			toRelease(dxgiOutput, dxgiOutput1, deskDupl, drawingImage);
		}
	}
	toRelease(dxgiAdapter);
	if (FAILED(hr)) return false;
	if (mScreenDuplications.size() == 0) return false;
	mScreenRects.clear();
	for (size_t i = 0; i < mScreenDuplications.size(); ++i)
		mScreenRects.push_back(Rect(mScreenDuplications[i].mDescription.DesktopCoordinates.left,
		                            mScreenDuplications[i].mDescription.DesktopCoordinates.top,
		                            mScreenDuplications[i].mDescription.DesktopCoordinates.right,
		                            mScreenDuplications[i].mDescription.DesktopCoordinates.bottom));
	MsScreenSharing::updateScreenConfiguration(mScreenRects);
	return true;
}

void MsScreenSharing_win::getWindowSize(int *windowX, int *windowY, int *windowWidth, int *windowHeight) const {
	if (mSourceDesc.type == MSScreenSharingType::MS_SCREEN_SHARING_DISPLAY &&
	    mLastFormat.mScreenIndex < mScreenRects.size()) {
		auto rect = mScreenRects[mLastFormat.mScreenIndex];
		*windowX = rect.mX1;
		*windowY = rect.mY1;
		*windowWidth = rect.getWidth();
		*windowHeight = rect.getHeight();
	} else {
		// Issue of GetWindowRect : it return shadow area. Use DwmGetWindowAttribute to get exactly the window size.
		RECT rect;
		if (S_OK != DwmGetWindowAttribute(mWindowId, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(RECT))) { // Win32
			ms_warning("[MsScreenSharing_win] Cannot get window size from %x. Set default to 400x400", mWindowId);
			rect.top = rect.left = 0;
			rect.bottom = rect.right = 400;
		}
		*windowX = rect.left + 1; // border?
		*windowY = rect.top;
		*windowWidth = rect.right - rect.left;
		*windowHeight = rect.bottom - rect.top;
	}
}

bool MsScreenSharing_win::prepareImage() {
	D3D11_TEXTURE2D_DESC desc = {};
	HRESULT hr;
	IDXGIResource *desktopResource;
	ID3D11Texture2D *acquiredDesktopImage; // last Frame
	DXGI_OUTDUPL_FRAME_INFO frameInfo;

	mScreenDuplications[mLastFormat.mScreenIndex].mDuplication->ReleaseFrame();
	hr = mScreenDuplications[mLastFormat.mScreenIndex].mDuplication->AcquireNextFrame(0, &frameInfo, &desktopResource);
	if (FAILED(hr)) {
		// ms_warning("[MsScreenSharing_win] Cannot acquire frame [%x]", hr);
		return false;
	}

	// QI for ID3D11Texture2D
	hr = desktopResource->QueryInterface(IID_PPV_ARGS(&acquiredDesktopImage));
	toRelease(desktopResource);
	if (FAILED(hr)) return false;
	if (acquiredDesktopImage == nullptr) return false;
	// Copy image into GDI drawing texture
	mImmediateContext->CopyResource(mScreenDuplications[mLastFormat.mScreenIndex].mDrawingImage, acquiredDesktopImage);
	toRelease(acquiredDesktopImage);
	// Draw cursor image into GDI drawing texture
	IDXGISurface1 *idxgiSurface1;
	hr = mScreenDuplications[mLastFormat.mScreenIndex].mDrawingImage->QueryInterface(IID_PPV_ARGS(&idxgiSurface1));
	if (FAILED(hr)) return false;
	if (mLastFormat.mRecordCursor) {
		CURSORINFO lCursorInfo = {0};
		lCursorInfo.cbSize = sizeof(lCursorInfo);
		auto lBoolres = GetCursorInfo(&lCursorInfo);
		if (lBoolres == TRUE) {
			if (lCursorInfo.flags == CURSOR_SHOWING) {
				auto lCursorPosition = lCursorInfo.ptScreenPos;
				auto lCursorSize = lCursorInfo.cbSize;
				HDC lHDC;
				idxgiSurface1->GetDC(FALSE, &lHDC);
				DrawIconEx(lHDC,
				           lCursorPosition.x -
				               mScreenDuplications[mLastFormat.mScreenIndex].mDescription.DesktopCoordinates.left,
				           lCursorPosition.y -
				               mScreenDuplications[mLastFormat.mScreenIndex].mDescription.DesktopCoordinates.top,
				           lCursorInfo.hCursor, 0, 0, 0, 0, DI_NORMAL | DI_DEFAULTSIZE);
				idxgiSurface1->ReleaseDC(nullptr);
			}
		}
	}
	// Copy from CPU access texture to bitmap buffer
	mImmediateContext->CopyResource(mScreenDuplications[mLastFormat.mScreenIndex].mDestImage,
	                                mScreenDuplications[mLastFormat.mScreenIndex].mDrawingImage);
	toRelease(idxgiSurface1);
	return true;
}

void MsScreenSharing_win::finalizeImage() {
	// Copy image into CPU access texture
	D3D11_MAPPED_SUBRESOURCE resource;
	UINT subresource = D3D11CalcSubresource(0, 0, 0);
	mImmediateContext->Map(mScreenDuplications[mLastFormat.mScreenIndex].mDestImage, subresource, D3D11_MAP_READ_WRITE,
	                       0, &resource);
	static UINT rowPitch = 0;
	if (rowPitch != resource.RowPitch) {
		rowPitch = resource.RowPitch;
	}
	const UINT imageSize =
	    resource.RowPitch * mScreenDuplications[mLastFormat.mScreenIndex].mImageDescription.ModeDesc.Height;
	bool haveData = false;
	for (unsigned int i = 0; !haveData && i < imageSize; ++i)
		if (((uint8_t *)resource.pData)[i] != '\0') {
			haveData = true;
		}
	static int count = -1;
	if (!haveData) {
		mImmediateContext->Unmap(mScreenDuplications[mLastFormat.mScreenIndex].mDestImage, subresource);
		return;
	}
	mFrameLock.lock();
	if (!mLastFormat.mSizeChanged) {
		if (mFrameData) freemsg(mFrameData);
		mFrameData = nullptr;
		int width = mLastFormat.mPosition.getWidth();
		int height = mLastFormat.mPosition.getHeight();
		const unsigned int targetImageSize = width * height * 4;
		const unsigned int targetRowPitch = 4 * width;
		const unsigned int screenTargetX =
		    (mLastFormat.mPosition.mX1 -
		     mScreenDuplications[mLastFormat.mScreenIndex].mDescription.DesktopCoordinates.left);
		const unsigned int screenTargetY =
		    mLastFormat.mPosition.mY1 -
		    mScreenDuplications[mLastFormat.mScreenIndex].mDescription.DesktopCoordinates.top;
		mFrameData = ms_yuv_allocator_get(mAllocator, targetImageSize, width, height);
		if (mFrameData) {
			for (int h = 0; h < height; ++h) {
				int y = screenTargetY + h;
				memcpy(mFrameData->b_rptr + h * targetRowPitch,
				       (char *)resource.pData + (screenTargetX * 4) + (y * resource.RowPitch), targetRowPitch);
			}
		}
	}
	mFrameLock.unlock();

	mImmediateContext->Unmap(mScreenDuplications[mLastFormat.mScreenIndex].mDestImage, subresource);
}

void MsScreenSharing_win::clean() {
	for (size_t i = 0; i < mScreenDuplications.size(); ++i) {
		mScreenDuplications[i].mDestImage->Release();
		mScreenDuplications[i].mDrawingImage->Release();
		mScreenDuplications[i].mDuplication->ReleaseFrame();
		mScreenDuplications[i].mDuplication->Release();
	}
	mScreenDuplications.clear();
	if (mImmediateContext) mImmediateContext->Release();
	if (mDevice) mDevice->Release();
	mImmediateContext = nullptr;
	mDevice = nullptr;
}
