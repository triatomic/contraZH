/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "W3DDevice/GameClient/W3DScreenshot.h"
#include "Common/GlobalData.h"
#include "GameClient/GameText.h"
#include "GameClient/InGameUI.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/surfaceclass.h"
#include "WWLib/mpsc_intrusive_queue.h"
#include <stb_image_write.h>
#include <zlib.h>
#include <vector>

struct ScreenshotThreadData
{
	ScreenshotThreadData()
		: pixelData(nullptr)
	{
	}

	~ScreenshotThreadData()
	{
		delete[] pixelData;
	}

	unsigned char* pixelData; // is owner
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	bool is16Bit;
	char userDataDirectory[_MAX_PATH];
	char leafname[_MAX_FNAME];
	int quality;
	ScreenshotFormat format;
};

// TheInGameUI is not thread safe, so the screenshot threads cannot show the success message
// themselves. Each thread pushes the written filename onto this queue and the main thread
// shows all pending messages in W3D_UpdateScreenshotMessages, so no message is lost when
// multiple screenshot threads finish within the same frame.
struct ScreenshotWrittenMessage
{
	ScreenshotWrittenMessage* next;
	char leafname[_MAX_FNAME];
};
static MPSCIntrusiveQueue<ScreenshotWrittenMessage> s_screenshotWrittenQueue;

// Converts the captured pixels and writes them to a file. Returns non-zero on success.
static int writeScreenshotImage(const ScreenshotThreadData* data, const char* pathname)
{
	const unsigned int width = data->width;
	const unsigned int height = data->height;

	// Convert to R8G8B8 for stb_image_write.
	unsigned char* image = new unsigned char[3 * width * height];

	if (!data->is16Bit)
	{
		// Convert A8R8G8B8/X8R8G8B8 to R8G8B8
		for (unsigned int y = 0; y < height; ++y)
		{
			const unsigned int* srcLine = reinterpret_cast<const unsigned int*>(data->pixelData + y * data->pitch);
			for (unsigned int x = 0; x < width; ++x)
			{
				const unsigned int argb = srcLine[x];
				const unsigned int index = 3 * (x + y * width);
				image[index + 0] = (unsigned char)(argb >> 16); // r
				image[index + 1] = (unsigned char)(argb >> 8);  // g
				image[index + 2] = (unsigned char)(argb >> 0);  // b
			}
		}
	}
	else
	{
		// Convert R5G6B5 to R8G8B8
		for (unsigned int y = 0; y < height; ++y)
		{
			const unsigned short* srcLine = reinterpret_cast<const unsigned short*>(data->pixelData + y * data->pitch);
			for (unsigned int x = 0; x < width; ++x)
			{
				const unsigned short rgb = srcLine[x];
				const unsigned int index = 3 * (x + y * width);
				image[index + 0] = (unsigned char)((rgb & 0xF800) >> 8); // r
				image[index + 1] = (unsigned char)((rgb & 0x07E0) >> 3); // g
				image[index + 2] = (unsigned char)((rgb & 0x001F) << 3); // b
			}
		}
	}

	int success = 0;
	switch (data->format)
	{
		case SCREENSHOT_JPEG:
			success = stbi_write_jpg(pathname, width, height, 3, image, data->quality);
			break;
		case SCREENSHOT_PNG:
			success = stbi_write_png(pathname, width, height, 3, image, width * 3);
			break;
	}

	delete[] image;

	return success;
}

static DWORD WINAPI screenshotThreadFunc(LPVOID param)
{
	ScreenshotThreadData* data = static_cast<ScreenshotThreadData*>(param);

	// TheSuperHackers @feature bobtista 08/07/2026 Save screenshots into a Screenshots subfolder
	// to keep the user data root folder tidy.
	char pathname[_MAX_PATH];
	strlcpy(pathname, data->userDataDirectory, ARRAY_SIZE(pathname));
	strlcat(pathname, "Screenshots\\", ARRAY_SIZE(pathname));
	CreateDirectory(pathname, nullptr);
	strlcat(pathname, data->leafname, ARRAY_SIZE(pathname));

	const int success = writeScreenshotImage(data, pathname);

	if (success)
	{
		ScreenshotWrittenMessage* message = new ScreenshotWrittenMessage;
		strlcpy(message->leafname, data->leafname, ARRAY_SIZE(message->leafname));
		s_screenshotWrittenQueue.Push(message);
	}
	else
	{
		DEBUG_LOG(("Failed to write screenshot %s", pathname));
	}

	delete data;

	return success;
}

void W3D_UpdateScreenshotMessages()
{
	ScreenshotWrittenMessage* message = s_screenshotWrittenQueue.Flush();
	while (message != nullptr)
	{
		UnicodeString ufileName;
		ufileName.translate(message->leafname);
		TheInGameUI->message(TheGameText->fetch("GUI:ScreenCapture"), ufileName.str());
		ScreenshotWrittenMessage* next = message->next;
		delete message;
		message = next;
	}
}

// Copies the back buffer. Returns null if its format is not supported.
static ScreenshotThreadData* captureBackBuffer(ScreenshotFormat format, Int jpegQuality)
{
	// TheSuperHackers @bugfix xezon 21/05/2025 Get the back buffer and create a copy of the surface.
	// Originally this code took the front buffer and tried to lock it. This does not work when the
	// render view clips outside the desktop boundaries. It crashed the game.
	SurfaceClass* surface = DX8Wrapper::_Get_DX8_Back_Buffer();
	SurfaceClass::SurfaceDescription surfaceDesc;
	surface->Get_Description(surfaceDesc);

	// TheSuperHackers @bugfix bobtista 08/07/2026 Support the 16 bit back buffer format that the
	// game uses when running in 16 bit color mode. Reading it with the 32 bit stride read garbage.
	const bool is32Bit = surfaceDesc.Format == WW3D_FORMAT_A8R8G8B8 || surfaceDesc.Format == WW3D_FORMAT_X8R8G8B8;
	const bool is16Bit = surfaceDesc.Format == WW3D_FORMAT_R5G6B5;

	if (!is32Bit && !is16Bit)
	{
		DEBUG_LOG(("Screenshot does not support back buffer format %d", (int)surfaceDesc.Format));
		surface->Release_Ref();
		return nullptr;
	}

	SurfaceClass* surfaceCopy = NEW_REF(SurfaceClass, (surfaceDesc.Width, surfaceDesc.Height, surfaceDesc.Format));
	DX8Wrapper::_Copy_DX8_Rects(surface->Peek_D3D_Surface(), nullptr, 0, surfaceCopy->Peek_D3D_Surface(), nullptr);

	surface->Release_Ref();
	surface = nullptr;

	struct Rect
	{
		int Pitch;
		void* pBits;
	} lrect;

	lrect.pBits = surfaceCopy->Lock(&lrect.Pitch);
	if (lrect.pBits == nullptr)
	{
		surfaceCopy->Release_Ref();
		return nullptr;
	}

	ScreenshotThreadData* threadData = new ScreenshotThreadData();
	threadData->width = surfaceDesc.Width;
	threadData->height = surfaceDesc.Height;
	threadData->pitch = lrect.Pitch;
	threadData->is16Bit = is16Bit;
	threadData->quality = jpegQuality;
	threadData->format = format;

	// Copy the locked surface with a single memcpy, including any row padding. The pixel
	// conversion and all file operations are done on the screenshot thread to keep the
	// main thread cheap.
	threadData->pixelData = new unsigned char[threadData->pitch * threadData->height];
	memcpy(threadData->pixelData, lrect.pBits, threadData->pitch * threadData->height);

	surfaceCopy->Unlock();
	surfaceCopy->Release_Ref();
	surfaceCopy = nullptr;

	return threadData;
}

#if defined(BUILD_WITH_D3D9)

// A band of the scene this tall passes through system memory at a time.
enum { SCENE_BAND_ROWS = 256 };

// Compressed bytes gather to this many before they leave as one IDAT chunk.
enum { PNG_CHUNK_BYTES = 256 * 1024 };

// PNG stores its numbers high byte first.
static void storeBigEndian(unsigned char* bytes, unsigned int value)
{
	bytes[0] = (unsigned char)(value >> 24);
	bytes[1] = (unsigned char)(value >> 16);
	bytes[2] = (unsigned char)(value >> 8);
	bytes[3] = (unsigned char)value;
}

static bool writePngChunk(FILE* file, const char* type, const unsigned char* data, unsigned int length)
{
	unsigned char header[8];
	storeBigEndian(header, length);
	memcpy(header + 4, type, 4);

	unsigned long crc = crc32(0, header + 4, 4);
	if (length > 0)
	{
		crc = crc32(crc, data, length);
	}
	unsigned char tail[4];
	storeBigEndian(tail, (unsigned int)crc);

	return fwrite(header, 1, 8, file) == 8
		&& (length == 0 || fwrite(data, 1, length, file) == length)
		&& fwrite(tail, 1, 4, file) == 4;
}

// Compresses the stream's pending input, or with Z_FINISH everything left, and writes each filled buffer as a chunk.
static bool deflateToPng(z_stream& stream, int flush, std::vector<unsigned char>& output, FILE* file)
{
	int result;
	do
	{
		result = deflate(&stream, flush);
		if (result != Z_OK && result != Z_STREAM_END)
		{
			return false;
		}
		if (stream.avail_out == 0 || flush == Z_FINISH)
		{
			const unsigned int produced = (unsigned int)output.size() - stream.avail_out;
			if (produced > 0 && !writePngChunk(file, "IDAT", &output[0], produced))
			{
				return false;
			}
			stream.next_out = &output[0];
			stream.avail_out = (unsigned int)output.size();
		}
	}
	while ((flush == Z_FINISH) ? (result != Z_STREAM_END) : (stream.avail_in > 0));

	return true;
}

// Writes the scene as a PNG one band at a time, because a whole copy of a 16K scene does not fit the game's address space.
static bool writeSceneTargetPng(IDirect3DSurface8* scene, const char* pathname)
{
	IDirect3DDevice8* device = DX8Wrapper::_Get_D3D_Device8();
	D3DSURFACE_DESC desc;
	if (FAILED(scene->GetDesc(&desc)) || (desc.Format != D3DFMT_X8R8G8B8 && desc.Format != D3DFMT_A8R8G8B8))
	{
		return false;
	}

	const UINT bandRows = (desc.Height < SCENE_BAND_ROWS) ? desc.Height : (UINT)SCENE_BAND_ROWS;
	IDirect3DSurface8* band = nullptr;
	IDirect3DSurface8* readback = nullptr;
	FILE* file = nullptr;
	bool ok = SUCCEEDED(device->CreateRenderTarget(desc.Width, bandRows, desc.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &band, nullptr))
		&& SUCCEEDED(device->CreateOffscreenPlainSurface(desc.Width, bandRows, desc.Format, D3DPOOL_SYSTEMMEM, &readback, nullptr))
		&& (file = fopen(pathname, "wb")) != nullptr;

	z_stream stream;
	memset(&stream, 0, sizeof(stream));
	const bool deflating = ok && deflateInit(&stream, Z_BEST_SPEED) == Z_OK;
	ok = deflating;

	if (ok)
	{
		static const unsigned char signature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };

		// 8 bits a channel, RGB, no interlace
		unsigned char header[13] = { 0, 0, 0, 0, 0, 0, 0, 0, 8, 2, 0, 0, 0 };
		storeBigEndian(header, desc.Width);
		storeBigEndian(header + 4, desc.Height);
		ok = fwrite(signature, 1, sizeof(signature), file) == sizeof(signature)
			&& writePngChunk(file, "IHDR", header, sizeof(header));
	}

	std::vector<unsigned char> output(PNG_CHUNK_BYTES);
	stream.next_out = &output[0];
	stream.avail_out = (unsigned int)output.size();

	// Each row leads with its filter. Sub stores a pixel as its step from the one to its left, which compresses well and costs little.
	std::vector<unsigned char> row(1 + 3 * desc.Width);
	row[0] = 1;

	for (UINT y = 0; ok && y < desc.Height; y += bandRows)
	{
		const UINT rows = (desc.Height - y < bandRows) ? desc.Height - y : bandRows;
		const RECT from = { 0, (LONG)y, (LONG)desc.Width, (LONG)(y + rows) };
		const RECT to = { 0, 0, (LONG)desc.Width, (LONG)rows };
		D3DLOCKED_RECT locked;
		ok = SUCCEEDED(device->StretchRect(scene, &from, band, &to, D3DTEXF_NONE))
			&& SUCCEEDED(device->GetRenderTargetData(band, readback))
			&& SUCCEEDED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY));
		if (!ok)
		{
			break;
		}

		for (UINT line = 0; ok && line < rows; ++line)
		{
			const unsigned char* pixel = static_cast<const unsigned char*>(locked.pBits) + line * locked.Pitch;
			unsigned char* out = &row[1];
			unsigned char left[3] = { 0, 0, 0 };
			for (UINT x = 0; x < desc.Width; ++x)
			{
				// Memory order is blue, green, red
				out[0] = (unsigned char)(pixel[2] - left[0]);
				out[1] = (unsigned char)(pixel[1] - left[1]);
				out[2] = (unsigned char)(pixel[0] - left[2]);
				left[0] = pixel[2];
				left[1] = pixel[1];
				left[2] = pixel[0];
				pixel += 4;
				out += 3;
			}

			stream.next_in = &row[0];
			stream.avail_in = (unsigned int)row.size();
			ok = deflateToPng(stream, Z_NO_FLUSH, output, file);
		}
		readback->UnlockRect();
	}

	ok = ok && deflateToPng(stream, Z_FINISH, output, file) && writePngChunk(file, "IEND", nullptr, 0);

	if (deflating)
	{
		deflateEnd(&stream);
	}
	if (file != nullptr)
	{
		ok = (fclose(file) == 0) && ok;
		if (!ok)
		{
			remove(pathname);
		}
	}
	if (readback != nullptr)
	{
		readback->Release();
	}
	if (band != nullptr)
	{
		band->Release();
	}
	return ok;
}

// Saves the scene from the game thread, which waits for the file because the next frame overwrites the scene.
static bool saveSceneScreenshot(IDirect3DSurface8* scene, const char* leafname)
{
	char pathname[_MAX_PATH];
	strlcpy(pathname, TheGlobalData->getPath_UserData().str(), ARRAY_SIZE(pathname));
	strlcat(pathname, "Screenshots\\", ARRAY_SIZE(pathname));
	CreateDirectory(pathname, nullptr);
	strlcat(pathname, leafname, ARRAY_SIZE(pathname));

	if (!writeSceneTargetPng(scene, pathname))
	{
		DEBUG_LOG(("Failed to write scene screenshot %s", pathname));
		return false;
	}

	ScreenshotWrittenMessage* message = new ScreenshotWrittenMessage;
	strlcpy(message->leafname, leafname, ARRAY_SIZE(message->leafname));
	s_screenshotWrittenQueue.Push(message);
	return true;
}

#endif // BUILD_WITH_D3D9

void W3D_TakeCompressedScreenshot(ScreenshotFormat format, Int jpegQuality)
{
	static constexpr const char* const ScreenshotFormatExtensions[] = { "jpg", "png" };
	static_assert(ARRAY_SIZE(ScreenshotFormatExtensions) == SCREENSHOT_FORMAT_COUNT, "Incorrect array size");

#if defined(BUILD_WITH_D3D9)
	// A scene of a fixed width is saved at its own size, without the interface, and always as a PNG.
	IDirect3DSurface8* scene = (DX8Wrapper::Get_Super_Sample_Width() != 0) ? DX8Wrapper::_Peek_Scene_Target() : nullptr;
	if (scene != nullptr)
	{
		format = SCREENSHOT_PNG;
	}
#endif

	// The filename is created here so the timestamp matches the capture time.
	char leafname[_MAX_FNAME];
	const char* extension = ScreenshotFormatExtensions[format];

	SYSTEMTIME st;
	GetLocalTime(&st);
	sprintf(leafname, "sshot_%04d%02d%02d_%02d%02d%02d_%03d.%s",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, extension);

#if defined(BUILD_WITH_D3D9)
	if (scene != nullptr && saveSceneScreenshot(scene, leafname))
	{
		return;
	}
#endif

	ScreenshotThreadData* threadData = captureBackBuffer(format, jpegQuality);
	if (threadData == nullptr)
	{
		return;
	}
	strlcpy(threadData->userDataDirectory, TheGlobalData->getPath_UserData().str(), ARRAY_SIZE(threadData->userDataDirectory));
	strlcpy(threadData->leafname, leafname, ARRAY_SIZE(threadData->leafname));

	const HANDLE hThread = CreateThread(nullptr, 0, screenshotThreadFunc, threadData, 0, nullptr);
	if (hThread)
	{
		CloseHandle(hThread);
	}
	else
	{
		delete threadData;
	}
}

#ifdef RTS_GAME_MCP
bool W3D_WriteScreenshotFile(const char* pathname, ScreenshotFormat format, Int jpegQuality)
{
	ScreenshotThreadData* data = captureBackBuffer(format, jpegQuality);
	if (data == nullptr)
	{
		return false;
	}
	const int success = writeScreenshotImage(data, pathname);
	delete data;
	return success != 0;
}
#endif
