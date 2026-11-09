// WBQtPreviewImage.h -- the shared conversion for ObjectPreview bridge buffers (the
// 128x128 BGR bottom-up model renders handed over by WBQtObject_/WBQtFence_/WBQtGrove_/
// WBQtPickUnit_RenderPreview).
//
// The MFC ObjectPreview::DrawMyTexture does NOT blit the whole render: it stretches only
// the CENTER QUARTER (source rect (w/4, h/4, w/2, h/2)) into the control -- a deliberate
// 2x zoom into the model (see the commented-out full-blit line above it). Every Qt
// preview must apply the same crop or the model looks half-sized vs MFC.
#ifndef WB_QT_PREVIEW_IMAGE_H
#define WB_QT_PREVIEW_IMAGE_H

#include <QImage>
#include <QPixmap>

namespace WBQtPreviewImage
{
	// bgr = w*h*3 bytes, bottom-up rows (the MFC path relied on the positive-height DIB
	// blit to flip). Returns the top-down RGB image cropped to the MFC center quarter.
	// (The crop is symmetric, so bottom-up vs top-down does not shift it.)
	inline QImage fromBridgeBgr(const unsigned char *bgr, int w, int h)
	{
		QImage img(w, h, QImage::Format_RGB888);
		for (int y = 0; y < h; ++y)
		{
			const unsigned char *src = bgr + (h - 1 - y) * w * 3;
			unsigned char *dst = img.scanLine(y);
			for (int x = 0; x < w; ++x)
			{
				dst[x * 3 + 0] = src[x * 3 + 2];	// R <- B
				dst[x * 3 + 1] = src[x * 3 + 1];	// G
				dst[x * 3 + 2] = src[x * 3 + 0];	// B <- R
			}
		}
		return img.copy(w / 4, h / 4, w / 2, h / 2);
	}

	// Square bottom-up BGRA, as terrain tiles and texture previews come from the bridge, to top-down RGB.
	inline QImage fromBottomUpBgra(const unsigned char *bgra, int extent)
	{
		QImage img(extent, extent, QImage::Format_RGB888);
		for (int y = 0; y < extent; ++y)
		{
			const unsigned char *src = bgra + (extent - 1 - y) * extent * 4;
			unsigned char *dst = img.scanLine(y);
			for (int x = 0; x < extent; ++x)
			{
				dst[x * 3 + 0] = src[x * 4 + 2];	// R <- B
				dst[x * 3 + 1] = src[x * 4 + 1];	// G
				dst[x * 3 + 2] = src[x * 4 + 0];	// B <- R
			}
		}
		return img;
	}

	// Fit the cropped image into the preview label. Upscales use nearest-neighbour like
	// the MFC StretchDIBits (bilinear smoothing turns an upscaled model into a blur);
	// downscales keep the smooth filter (nearest downscaling drops pixels and shimmers).
	inline QPixmap toLabelPixmap(const QImage &img, const QSize &labelSize)
	{
		const bool upscale = (labelSize.width() > img.width() || labelSize.height() > img.height());
		return QPixmap::fromImage(img).scaled(labelSize, Qt::KeepAspectRatio,
			upscale ? Qt::FastTransformation : Qt::SmoothTransformation);
	}
}

#endif // WB_QT_PREVIEW_IMAGE_H
