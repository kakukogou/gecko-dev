/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ColorFormatPixelLayout.h"

#include "ColorFormatUtils.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/ImageBitmapImage.h"
#include "ImageContainer.h"
#include "ColorFormatUtils.h"

using namespace mozilla::gfx;
using namespace mozilla::layers;

namespace mozilla {
namespace dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(ColorFormatPixelLayout)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ColorFormatPixelLayout)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ColorFormatPixelLayout)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ColorFormatPixelLayout)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ColorFormatPixelLayout::ColorFormatPixelLayout()
{

}

//ColorFormatPixelLayout::ColorFormatPixelLayout(ColorFormat format,
//                                               SourceSurface *surface,
//                                               PlanarYCbCrData const *ycbcrLayout /* = nullptr */)
//  : mChannels(getChannelCountOfColorFormat(format))
//{
//  // Add |MOZ_COUNT_CTOR(ColorFormatPixelLayout);| for a non-refcounted object.
//
//  // information from format
//  uint8_t channelCount = getChannelCountOfColorFormat(format);
//
//  // set mChannels
//  switch (format) {
//  case ColorFormat::RGBA32:
//  case ColorFormat::BGRA32:
//  case ColorFormat::RGB24:
//  case ColorFormat::BGR24:
//  {
//    // information from surface
//    RefPtr<DataSourceSurface> dataSurface = surface->GetDataSurface();
//    if (!dataSurface) {
//      // something wrong!
//      return;
//    }
//    const IntSize surfaceSize = dataSurface->GetSize();
//    const uint32_t surfaceStride = dataSurface->Stride();
//
//    for (uint8_t i = 0; i < channelCount; ++i) {
//      nsRefPtr<ChannelPixelLayout> *channel = mChannels.AppendElement();
//      (*channel) = new ChannelPixelLayout();
//      (*channel)->mOffset = i;
//      (*channel)->mWidth = surfaceSize.width;
//      (*channel)->mHeight = surfaceSize.height;
//      (*channel)->mStride = surfaceStride;
//      (*channel)->mSkip = channelCount - 1;
//    }
//    break;
//  }
//  case ColorFormat::GRAY8:
//  {
//    break;
//  }
//  case ColorFormat::YUV444P:
//  case ColorFormat::YUV422P:
//  case ColorFormat::YUV420P:
//  case ColorFormat::YUV420SP_NV12:
//  case ColorFormat::YUV420SP_NV21:
//  {
//    if (!ycbcrLayout) {
//      // something wrong
//      return;
//    }
//    nsRefPtr<ChannelPixelLayout> *ychannel = mChannels.AppendElement(); (*ychannel) = new ChannelPixelLayout();
//    nsRefPtr<ChannelPixelLayout> *uchannel = mChannels.AppendElement(); (*uchannel) = new ChannelPixelLayout();
//    nsRefPtr<ChannelPixelLayout> *vchannel = mChannels.AppendElement(); (*vchannel) = new ChannelPixelLayout();
//    (*ychannel)->mOffset = 0;
//    (*uchannel)->mOffset = (*ychannel)->mOffset + ycbcrLayout->mYStride * ycbcrLayout->mYSize.height;
//    (*vchannel)->mOffset = (*uchannel)->mOffset + ycbcrLayout->mCbCrStride * ycbcrLayout->mCbCrSize.height;
//    (*ychannel)->mWidth = ycbcrLayout->mYSize.width;
//    (*ychannel)->mHeight = ycbcrLayout->mYSize.height;
//    (*ychannel)->mStride = ycbcrLayout->mYStride;
//    (*ychannel)->mSkip = ycbcrLayout->mYSkip;
//    (*uchannel)->mWidth = ycbcrLayout->mCbCrSize.width;
//    (*uchannel)->mHeight = ycbcrLayout->mCbCrSize.height;
//    (*uchannel)->mStride = ycbcrLayout->mCbCrStride;
//    (*uchannel)->mSkip = ycbcrLayout->mCbSkip;
//    (*vchannel)->mWidth = ycbcrLayout->mCbCrSize.width;
//    (*vchannel)->mHeight = ycbcrLayout->mCbCrSize.height;
//    (*vchannel)->mStride = ycbcrLayout->mCbCrStride;
//    (*vchannel)->mSkip = ycbcrLayout->mCrSkip;
//
//    break;
//  }
//  case ColorFormat::HSV:
//    break;
//  case ColorFormat::Lab:
//    break;
//  case ColorFormat::_empty:
//  default:
//    ;
//  }
//}

ColorFormatPixelLayout::ColorFormatPixelLayout(ColorFormat aFormat,
                                               layers::Image* aImage)
  : mChannels(getChannelCountOfColorFormat(aFormat)) // init the capacity
{
  // Add |MOZ_COUNT_CTOR(ColorFormatPixelLayout);| for a non-refcounted object.

  // information from format
  uint8_t channelCount = getChannelCountOfColorFormat(aFormat);

  // set mChannels
  switch (aFormat) {
  case ColorFormat::RGBA32:
  case ColorFormat::BGRA32:
  case ColorFormat::RGB24:
  case ColorFormat::BGR24:
  {
    layers::ImageBitmapImage* imagebitmapImage = static_cast<layers::ImageBitmapImage*> (aImage);
    const IntSize surfaceSize = imagebitmapImage->GetSize();
    const uint32_t surfaceStride = imagebitmapImage->GetStride();

    for (uint8_t i = 0; i < channelCount; ++i) {
      nsRefPtr<ChannelPixelLayout> *channel = mChannels.AppendElement();
      (*channel) = new ChannelPixelLayout();
      (*channel)->mOffset = i;
      (*channel)->mWidth = surfaceSize.width;
      (*channel)->mHeight = surfaceSize.height;
      (*channel)->mStride = surfaceStride;
      (*channel)->mSkip = channelCount - 1;
    }
    break;
  }
  case ColorFormat::GRAY8:
  {
    break;
  }
  case ColorFormat::YUV444P:
  case ColorFormat::YUV422P:
  case ColorFormat::YUV420P:
  case ColorFormat::YUV420SP_NV12:
  case ColorFormat::YUV420SP_NV21:
  {
    layers::PlanarYCbCrImage* ycbcrImage = static_cast<layers::PlanarYCbCrImage*> (aImage);
    layers::PlanarYCbCrData const *ycbcrLayout = ycbcrImage->GetData();
    if (!ycbcrLayout) {
      // something wrong
      return;
    }
    nsRefPtr<ChannelPixelLayout> *ychannel = mChannels.AppendElement(); (*ychannel) = new ChannelPixelLayout();
    nsRefPtr<ChannelPixelLayout> *uchannel = mChannels.AppendElement(); (*uchannel) = new ChannelPixelLayout();
    nsRefPtr<ChannelPixelLayout> *vchannel = mChannels.AppendElement(); (*vchannel) = new ChannelPixelLayout();
    (*ychannel)->mOffset = 0;
    (*uchannel)->mOffset = (*ychannel)->mOffset + ycbcrLayout->mYStride * ycbcrLayout->mYSize.height;
    (*vchannel)->mOffset = (*uchannel)->mOffset + ycbcrLayout->mCbCrStride * ycbcrLayout->mCbCrSize.height;
    (*ychannel)->mWidth = ycbcrLayout->mYSize.width;
    (*ychannel)->mHeight = ycbcrLayout->mYSize.height;
    (*ychannel)->mStride = ycbcrLayout->mYStride;
    (*ychannel)->mSkip = ycbcrLayout->mYSkip;
    (*uchannel)->mWidth = ycbcrLayout->mCbCrSize.width;
    (*uchannel)->mHeight = ycbcrLayout->mCbCrSize.height;
    (*uchannel)->mStride = ycbcrLayout->mCbCrStride;
    (*uchannel)->mSkip = ycbcrLayout->mCbSkip;
    (*vchannel)->mWidth = ycbcrLayout->mCbCrSize.width;
    (*vchannel)->mHeight = ycbcrLayout->mCbCrSize.height;
    (*vchannel)->mStride = ycbcrLayout->mCbCrStride;
    (*vchannel)->mSkip = ycbcrLayout->mCrSkip;

    break;
  }
  case ColorFormat::HSV:
    break;
  case ColorFormat::Lab:
    break;
  case ColorFormat::_empty:
  default:
    ;
  }
}

ColorFormatPixelLayout::~ColorFormatPixelLayout()
{
  // Add |MOZ_COUNT_DTOR(ColorFormatPixelLayout);| for a non-refcounted object.
}

JSObject*
ColorFormatPixelLayout::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ColorFormatPixelLayoutBinding::Wrap(aCx, this, aGivenProto);
}

void
ColorFormatPixelLayout::GetChannels(nsTArray<nsRefPtr<ChannelPixelLayout>>& aRetVal) const
{
  aRetVal = mChannels;
}

/* static */
ColorFormat
ColorFormatPixelLayout::GetYUVFormat(ColorFormatPixelLayout const &layout) {
printf_stderr("ColorFormatPixelLayout::GetYUVFormat() +\n");
  nsRefPtr<ChannelPixelLayout> ychannel = layout.mChannels[0];
  nsRefPtr<ChannelPixelLayout> uchannel = layout.mChannels[1];
  nsRefPtr<ChannelPixelLayout> vchannel = layout.mChannels[2];
printf_stderr("ColorFormatPixelLayout::GetYUVFormat() 1\n");
  if (ychannel->mSkip == 0) {
    if (ychannel->mWidth == uchannel->mWidth) {
      if (ychannel->mHeight == uchannel->mHeight) {
          return ColorFormat::YUV444P;
      }
      else if (ychannel->mHeight == uchannel->mHeight * 2) {
        return ColorFormat::YUV422P;
      }
    }
    else if (ychannel->mWidth == uchannel->mWidth * 2) {
      if (ychannel->mHeight == uchannel->mHeight * 2) {
        if (uchannel->mSkip == 0) {
printf_stderr("ColorFormatPixelLayout::GetYUVFormat() 420P -\n");
          return ColorFormat::YUV420P;
        }
        else if (uchannel->mSkip == 1 && vchannel->mSkip == 1) {
          if (uchannel->mOffset < vchannel->mOffset) {
            return ColorFormat::YUV420SP_NV12;
          }
          else if (uchannel->mOffset > vchannel->mOffset) {
            return ColorFormat::YUV420SP_NV12;
          }
        }
      }
    }
  }
//  else if (ychannel.mSkip == 2) {
//    // something likes YUV24
//  }

  return ColorFormat::_empty;
}


} // namespace dom
} // namespace mozilla
