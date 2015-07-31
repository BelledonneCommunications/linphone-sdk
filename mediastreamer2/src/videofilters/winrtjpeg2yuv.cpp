#include <windows.h>
#include <ppltasks.h>
#include <string>

#include <mediastreamer2/mscommon.h>
#include <mediastreamer2/msvideo.h>

using namespace Concurrency;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;


class MSWinRTJpeg2Yuv
{
private:
	static std::wstring UTF8ToUTF16(const char *utf8)
	{
		if ((utf8 == nullptr) || (*utf8 == '\0'))
			return std::wstring();

		int utf8len = static_cast<int>(strlen(utf8));

		// Get the size to alloc for utf-16 string
		int utf16len = MultiByteToWideChar(CP_UTF8, 0, utf8, utf8len, nullptr, 0);
		if (utf16len == 0) {
			DWORD error = GetLastError();
			ms_error("Invalid UTF-8 character, can't convert to UTF-16: %d", error);
			return std::wstring();
		}

		// Do the conversion
		std::wstring utf16;
		utf16.resize(utf16len);
		if (MultiByteToWideChar(CP_UTF8, 0, utf8, utf8len, &utf16[0], (int)utf16.length()) == 0) {
			DWORD error = GetLastError();
			ms_error("Error during string conversion from UTF-8 to UTF-16: %d", error);
			return std::wstring();
		}
		return utf16;
	};

	const char *mJpegPath;
	MSVideoSize *mReqSize;
	HANDLE mConversionEvent;
	mblk_t *mResult;

public:
	MSWinRTJpeg2Yuv(const char *jpgpath, MSVideoSize *reqsize)
		: mJpegPath(jpgpath), mReqSize(reqsize), mResult(NULL)
	{
		mConversionEvent = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
	}
	~MSWinRTJpeg2Yuv()
	{
		CloseHandle(mConversionEvent);
	}

	mblk_t * Convert()
	{
		if (mJpegPath == NULL) return NULL;

		std::wstring wsjpgpath = UTF8ToUTF16(mJpegPath);
		std::replace(wsjpgpath.begin(), wsjpgpath.end(), L'/', L'\\');
		const wchar_t* wjpgpath = wsjpgpath.c_str();
		Platform::String^ sjpgpath = ref new Platform::String(wjpgpath);

		IAsyncOperation<StorageFile^>^ op = StorageFile::GetFileFromPathAsync(sjpgpath);
		op->Completed = ref new AsyncOperationCompletedHandler<StorageFile^>([this](IAsyncOperation<StorageFile^>^ asyncOp, AsyncStatus asyncStatus) {
			if (asyncStatus == AsyncStatus::Completed) {
				StorageFile^ file = asyncOp->GetResults();
				IAsyncOperation<IRandomAccessStream^>^ op = file->OpenAsync(FileAccessMode::Read);
				op->Completed = ref new AsyncOperationCompletedHandler<IRandomAccessStream^>([this](IAsyncOperation<IRandomAccessStream^>^ asyncOp, AsyncStatus asyncStatus) {
					if (asyncStatus == AsyncStatus::Completed) {
						IRandomAccessStream^ stream = asyncOp->GetResults();
						IAsyncOperation<BitmapDecoder^>^ op = BitmapDecoder::CreateAsync(stream);
						op->Completed = ref new AsyncOperationCompletedHandler<BitmapDecoder^>([this](IAsyncOperation<BitmapDecoder^>^ asyncOp, AsyncStatus asyncStatus) {
							if (asyncStatus == AsyncStatus::Completed) {
								BitmapDecoder^ decoder = asyncOp->GetResults();
								BitmapTransform^ transform = ref new BitmapTransform();
								transform->ScaledWidth = this->mReqSize->width;
								transform->ScaledHeight = this->mReqSize->height;
								IAsyncOperation<PixelDataProvider^>^ op = decoder->GetPixelDataAsync(
									decoder->BitmapPixelFormat, decoder->BitmapAlphaMode, transform, ExifOrientationMode::RespectExifOrientation, ColorManagementMode::ColorManageToSRgb);
								op->Completed = ref new AsyncOperationCompletedHandler<PixelDataProvider^>([this](IAsyncOperation<PixelDataProvider^>^ asyncOp, AsyncStatus asyncStatus) {
									if (asyncStatus == AsyncStatus::Completed) {
										PixelDataProvider^ provider = asyncOp->GetResults();
										MSPicture destPicture;
										mResult = ms_yuv_buf_alloc(&destPicture, mReqSize->width, mReqSize->height);
										Platform::Array<unsigned char>^ data = provider->DetachPixelData();
										unsigned char *cdata = data->Data;
										for (int y = 0; y < mReqSize->height; y++) {
											for (int x = 0; x < mReqSize->width; x++) {
												uint8_t b = data->Data[y * mReqSize->width * 4 + x * 4 + 0];
												uint8_t g = data->Data[y * mReqSize->width * 4 + x * 4 + 1];
												uint8_t r = data->Data[y * mReqSize->width * 4 + x * 4 + 2];

												// Y
												*destPicture.planes[0]++ = (uint8_t)((0.257 * r) + (0.504 * g) + (0.098 * b) + 16);

												// U/V subsampling
												if ((y % 2 == 0) && (x % 2 == 0)) {
													uint32_t r32 = 0, g32 = 0, b32 = 0;
													for (int i = 0; i<2; i++) {
														for (int j = 0; j<2; j++) {
															b32 += data->Data[(y + i) * mReqSize->width * 4 + (x + j) * 4 + 0];
															g32 += data->Data[(y + i) * mReqSize->width * 4 + (x + j) * 4 + 1];
															r32 += data->Data[(y + i) * mReqSize->width * 4 + (x + j) * 4 + 2];
														}
													}
													r32 = (uint32_t)(r32 * 0.25f); g32 = (uint32_t)(g32 * 0.25f); b32 = (uint32_t)(b32 * 0.25f);

													// U
													*destPicture.planes[1]++ = (uint8_t)(-(0.148 * r32) - (0.291 * g32) + (0.439 * b32) + 128);
													// V
													*destPicture.planes[2]++ = (uint8_t)((0.439 * r32) - (0.368 * g32) - (0.071 * b32) + 128);
												}
											}
										}
									} else {
										ms_error("Windows::Graphics::Imaging::BitmapDecoder::GetPixelDataAsync failed");
									}
									SetEvent(this->mConversionEvent);
								});
							} else {
								ms_error("Windows::Graphics::Imaging::BitmapDecoder::CreateAsync failed");
								SetEvent(this->mConversionEvent);
							}
						});
					} else {
						ms_error("Windows::Storage::StorageFile::OpenAsync failed");
						SetEvent(this->mConversionEvent);
					}
				});
			} else {
				ms_error("Windows::Storage::StorageFile::GetFileFromPathAsync failed");
				SetEvent(this->mConversionEvent);
			}
		});
		WaitForSingleObjectEx(mConversionEvent, INFINITE, FALSE);

		return mResult;
	};
};


extern "C" __declspec(dllexport) mblk_t * winrtjpeg2yuv(const char *jpgpath, MSVideoSize *reqsize)
{
	mblk_t *result = NULL;
	MSWinRTJpeg2Yuv *converter = new MSWinRTJpeg2Yuv(jpgpath, reqsize);
	result = converter->Convert();
	delete converter;
	return result;
}
