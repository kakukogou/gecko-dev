/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/ChannelPixelLayout.h"
#include "mozilla/gfx/2D.h"
#include "ImageContainer.h"
#include "libyuv.h"

using namespace mozilla::gfx;
using namespace mozilla::layers;

namespace mozilla {
namespace dom {

extern inline already_AddRefed<layers::Image>
CreateImageFromRawData(gfx::IntSize aSize, uint32_t aStride, gfx::SurfaceFormat aFormat, uint8_t* aBuffer, uint32_t aBufferLength, ErrorResult& aRv);

static ColorFormat
GetYUVFormatFromPlanarYCbCrData(PlanarYCbCrData const *data) {
  if (data->mYSkip == 0) {
    if (data->mYSize.width == data->mCbCrSize.width) {
      if (data->mYSize.height == data->mCbCrSize.height) {
          return ColorFormat::YUV444P;
      }
      else if (data->mYSize.height == data->mCbCrSize.height * 2) {
        return ColorFormat::YUV422P;
      }
    }
    else if (data->mYSize.width == data->mCbCrSize.width * 2) {
      if (data->mYSize.height == data->mCbCrSize.height * 2) {
        if (data->mCbSkip == 0 && data->mCrSkip == 0) {
          return ColorFormat::YUV420P;
        }
        else if (data->mCbSkip == 1 && data->mCrSkip == 1) {
          if (data->mCbChannel < data->mCrChannel) {
            return ColorFormat::YUV420SP_NV12;
          }
          else if (data->mCbChannel > data->mCrChannel) {
            return ColorFormat::YUV420SP_NV12;
          }
        }
      }
    }
  }
  //  else if (data->mYSkip == 2) {
  //    // something likes YUV24
  //  }

  return ColorFormat::_empty;
}

static SurfaceFormat
ColorFromatToSurfaceFormat(ColorFormat colorFromat) {
  switch (colorFromat) {
  case ColorFormat::RGBA32:
    return SurfaceFormat::R8G8B8A8;
  case ColorFormat::BGRA32:
    return SurfaceFormat::B8G8R8A8;
  case ColorFormat::RGB24:
  case ColorFormat::BGR24:
  case ColorFormat::GRAY8:
    return SurfaceFormat::UNKNOWN;
  case ColorFormat::YUV444P:
  case ColorFormat::YUV422P:
  case ColorFormat::YUV420P:
  case ColorFormat::YUV420SP_NV12:
  case ColorFormat::YUV420SP_NV21:
    return SurfaceFormat::YUV;
  case ColorFormat::HSV:
  case ColorFormat::Lab:
  case ColorFormat::_empty:
  case ColorFormat::EndGuard_:
  default:
    return SurfaceFormat::UNKNOWN;
  }
}

ColorFormat
ImageBitmap::FindOptimalFormat(const Optional<Sequence<ColorFormat>>& possibleFormats)
{
  MOZ_ASSERT(mBackend, "No buffer container in ImageBitmap!");
  return GetColorFormat();
}

int32_t
ImageBitmap::MappedDataLength(ColorFormat format, ErrorResult& aRv)
{
  MOZ_ASSERT(mBackend, "No buffer container in ImageBitmap!");

  if (format == ColorFormat::_empty ||
      format == ColorFormat::BGR24 ||
      format == ColorFormat::RGB24 ||
      format == ColorFormat::GRAY8 ||
      format == ColorFormat::HSV ||
      format == ColorFormat::Lab) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return 0;
  }

  if (format == ColorFormat::RGBA32 || format == ColorFormat::BGRA32) {
    if (mBackend->GetFormat() == ImageFormat::IMAGEBITMAP_BACKEND) {
      layers::ImageBitmapImage* imagebitmapImage = static_cast<layers::ImageBitmapImage*> (mBackend.get());
      return imagebitmapImage->GetBufferLength();
    } else if (mBackend->GetFormat() == ImageFormat::PLANAR_YCBCR) {
      layers::PlanarYCbCrImage* ycbcrImage = static_cast<layers::PlanarYCbCrImage*> (mBackend.get());
      IntSize const surfaceSize = ycbcrImage->GetSize();
      uint32_t const surfaceDataLength = surfaceSize.width * surfaceSize.height * 4;
      return surfaceDataLength;
    } else {
      aRv.Throw(NS_ERROR_NOT_AVAILABLE);
      return 0;
    }
  }
  else if (format == ColorFormat::YUV444P ||
           format == ColorFormat::YUV422P ||
           format == ColorFormat::YUV420P ||
           format == ColorFormat::YUV420SP_NV12 ||
           format == ColorFormat::YUV420SP_NV21) {
    if (mBackend->GetFormat() == ImageFormat::IMAGEBITMAP_BACKEND) {
      layers::ImageBitmapImage* imagebitmapImage = static_cast<layers::ImageBitmapImage*> (mBackend.get());
      IntSize const surfaceSize = imagebitmapImage->GetSize();
      uint32_t const surfaceStride = imagebitmapImage->GetStride();
      uint32_t const surfaceDataLength = surfaceSize.width * surfaceSize.height * 3 / 2;
      return surfaceDataLength;
    } else if (mBackend->GetFormat() == ImageFormat::PLANAR_YCBCR) {
      layers::PlanarYCbCrImage* ycbcrImage = static_cast<layers::PlanarYCbCrImage*> (mBackend.get());
      return ycbcrImage->GetDataSize();
    } else {
      aRv.Throw(NS_ERROR_NOT_AVAILABLE);
      return 0;
    }
  }
  else {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return 0;
  }
}

already_AddRefed<ColorFormatPixelLayout>
ImageBitmap::MapDataInto(ColorFormat format, const ArrayBuffer& aWrapBuffer, int32_t offset, int32_t length, ErrorResult& aRv)
{
  MOZ_ASSERT(mBackend, "No buffer container in ImageBitmap!");
  MOZ_ASSERT(format != ColorFormat::_empty,"No format is given.");

  if (format == ColorFormat::_empty ||
      format == ColorFormat::BGR24 ||
      format == ColorFormat::RGB24 ||
      format == ColorFormat::GRAY8 ||
      format == ColorFormat::HSV ||
      format == ColorFormat::Lab) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return nullptr;
  }

  // copy the data into buffer
  ColorFormat const optimalFormat = GetColorFormat();

  if (format != optimalFormat) {
    // TODO: do conversion here...
  }

  // fill-in the inputed heap
  uint8_t *dataSrc = nullptr;
  int32_t dataSize = 0;
  gfx::IntSize imageSize;

  if (format == ColorFormat::RGBA32 || format == ColorFormat::BGRA32) {
    layers::ImageBitmapImage* imagebitmapImage = static_cast<layers::ImageBitmapImage*> (mBackend.get());
    dataSrc = imagebitmapImage->GetBuffer();
    dataSize = imagebitmapImage->GetBufferLength();
    imageSize = imagebitmapImage->GetSize();
  }
  else if (format == ColorFormat::YUV444P ||
           format == ColorFormat::YUV422P ||
           format == ColorFormat::YUV420P ||
           format == ColorFormat::YUV420SP_NV12 ||
           format == ColorFormat::YUV420SP_NV21) {
    layers::PlanarYCbCrImage* ycbcrImage = static_cast<layers::PlanarYCbCrImage*> (mBackend.get());
    dataSrc = ycbcrImage->GetData()->mYChannel;
    dataSize = ycbcrImage->GetDataSize();
    imageSize = ycbcrImage->GetSize();
  }
  else {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return nullptr;
  }

  aWrapBuffer.ComputeLengthAndData();
  uint32_t const bufferLength = aWrapBuffer.Length();

  const int32_t neededBufferLength = MappedDataLength(format, aRv);
  if (((uint32_t)(offset + length) > bufferLength) ||
      (length != neededBufferLength)) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  bool isSameColorFormat = false;
  switch(format) {
  case ColorFormat::RGBA32:
  {
    if (optimalFormat == ColorFormat::RGBA32) {
      isSameColorFormat = true;
    } else if (optimalFormat == ColorFormat::BGRA32) {
      layers::ImageBitmapImage* src = static_cast<layers::ImageBitmapImage*> (mBackend.get());
      libyuv::ABGRToARGB(src->GetBuffer(), src->GetStride(),
                         aWrapBuffer.Data()+offset, src->GetStride(),
                         src->GetSize().width, src->GetSize().height);
    } else {
      // all YUV cases
      layers::PlanarYCbCrImage* ycbcrImage = static_cast<layers::PlanarYCbCrImage*> (mBackend.get());
      const layers::PlanarYCbCrData* src = ycbcrImage->GetData();
      libyuv::I420ToABGR(src->mYChannel, src->mYStride,
                         src->mCbChannel, src->mCbCrStride,
                         src->mCrChannel, src->mCbCrStride,
                         aWrapBuffer.Data()+offset, src->mYSize.width*4,
                         src->mYSize.width, src->mYSize.height);
    }
  }
  break;
  case ColorFormat::BGRA32:
  {
    if (optimalFormat == ColorFormat::RGBA32) {
      layers::ImageBitmapImage* src = static_cast<layers::ImageBitmapImage*> (mBackend.get());
      libyuv::ABGRToARGB(src->GetBuffer(), src->GetStride(),
                         aWrapBuffer.Data()+offset, src->GetStride(),
                         src->GetSize().width, src->GetSize().height);
    } else if (optimalFormat == ColorFormat::BGRA32) {
      isSameColorFormat = true;
    } else {
      // all YUV cases
      layers::PlanarYCbCrImage* ycbcrImage = static_cast<layers::PlanarYCbCrImage*> (mBackend.get());
      const layers::PlanarYCbCrData* src = ycbcrImage->GetData();
      libyuv::I420ToARGB(src->mYChannel, src->mYStride,
                         src->mCbChannel, src->mCbCrStride,
                         src->mCrChannel, src->mCbCrStride,
                         aWrapBuffer.Data()+offset, src->mYSize.width*4,
                         src->mYSize.width, src->mYSize.height);
    }
  }
  break;
  case ColorFormat::YUV444P:
  case ColorFormat::YUV422P:
  case ColorFormat::YUV420P:
  case ColorFormat::YUV420SP_NV12:
  case ColorFormat::YUV420SP_NV21:
  {
    layers::PlanarYCbCrImage* ycbcrImage = static_cast<layers::PlanarYCbCrImage*> (mBackend.get());
    const layers::PlanarYCbCrData* src = ycbcrImage->GetData();
    if (optimalFormat == ColorFormat::RGBA32) {
      aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    } else if (optimalFormat == ColorFormat::BGRA32) {
      aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    } else {
      // all YUV cases
      isSameColorFormat = true;
    }
  }
  break;
  }

  if (isSameColorFormat) {
    memcpy(aWrapBuffer.Data()+offset, dataSrc, length);
  }

  return GetColorFormatPixelLayout();
}

bool
ImageBitmap::SetDataFrom(ColorFormat aFormat, const ArrayBuffer& aWrapBuffer, int32_t aOffset, int32_t aLength,
                         int32_t aWidth, int32_t aHeight, int32_t aStride, ErrorResult& aRv)
{
  const SurfaceFormat surfaceFormat = ColorFromatToSurfaceFormat(aFormat);
  if (surfaceFormat == SurfaceFormat::UNKNOWN) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return false;
  }

  // clear current container
  if (mBackend) { mBackend = nullptr; }
  if (mSurface) { mSurface = nullptr; }

  // prepare the input data
  aWrapBuffer.ComputeLengthAndData();
  uint32_t const bufferLength = aWrapBuffer.Length();

  if ((uint32_t)(aOffset + aLength) > bufferLength) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  // create a new backend
  const gfx::IntSize imageSize(aWidth, aHeight);
  mBackend = CreateImageFromRawData(imageSize, aStride, surfaceFormat, aWrapBuffer.Data() + aOffset, aLength, aRv);

  if (aRv.Failed()) {
    return nullptr;
  }

  // reset mCropRect
  mCropRect.x = 0;
  mCropRect.y = 0;
  mCropRect.width = imageSize.width;
  mCropRect.height = imageSize.height;

  return true;
}

ColorFormat
ImageBitmap::GetColorFormat() const
{
  if (mBackend->GetFormat() == ImageFormat::IMAGEBITMAP_BACKEND) {
    layers::ImageBitmapImage* image = static_cast<layers::ImageBitmapImage*> (mBackend.get());

    switch (image->GetSurfaceFormat()) {
    case SurfaceFormat::B8G8R8A8:
    case SurfaceFormat::B8G8R8X8:
      return ColorFormat::BGRA32;
    case SurfaceFormat::R8G8B8A8:
    case SurfaceFormat::R8G8B8X8:
      return ColorFormat::RGBA32;
    case SurfaceFormat::R5G6B5:
    case SurfaceFormat::A8:
    case SurfaceFormat::YUV:
    case SurfaceFormat::UNKNOWN:
    default:
      return ColorFormat::_empty;
    }
  } else if (mBackend->GetFormat() == ImageFormat::PLANAR_YCBCR) {
    layers::PlanarYCbCrImage* ycbcrImage = static_cast<layers::PlanarYCbCrImage*> (mBackend.get());
    return GetYUVFormatFromPlanarYCbCrData(ycbcrImage->GetData());
  }

  return ColorFormat::_empty;
}

already_AddRefed<ColorFormatPixelLayout>
ImageBitmap::GetColorFormatPixelLayout() const
{
  ColorFormat format = GetColorFormat();
  nsRefPtr<ColorFormatPixelLayout> layout(new ColorFormatPixelLayout(format, mBackend));
  return layout.forget();
}

} // namespace dom
} // namespace mozilla
