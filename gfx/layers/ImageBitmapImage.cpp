/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageBitmapImage.h"
#include "Layers.h"
#include "libyuv.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

ImageBitmapImage::ImageBitmapImage()
 : Image(nullptr, ImageFormat::IMAGEBITMAP_BACKEND)
 , mBuffer(nullptr)
 , mBufferLength(0)
 , mStride(0)
 , mSize()
 , mSurfaceFormat(gfx::SurfaceFormat::UNKNOWN)
 {

 }

ImageBitmapImage::~ImageBitmapImage()
{
  if (mBuffer) {
    delete[] mBuffer;
  }
}

bool
ImageBitmapImage::Init(gfx::IntSize const & aSize, uint32_t aStride, gfx::SurfaceFormat aFormat)
{
  // assume RGBA format for now.
  uint32_t bufferLength = aSize.height * aStride;
  mBuffer = new uint8_t[bufferLength];

  if (!mBuffer) {
    return false;
  }

  mBufferLength = bufferLength;
  mSize = aSize;
  mStride = aStride;
  mSurfaceFormat = aFormat;

  return true;
}

TemporaryRef<gfx::SourceSurface>
ImageBitmapImage::GetAsSourceSurface()
{
  RefPtr<DataSourceSurface> dataSurface =
    Factory::CreateDataSourceSurfaceWithStride(mSize, gfx::SurfaceFormat::B8G8R8A8, mStride);

  if (NS_WARN_IF(!dataSurface)) {
    return nullptr;
  }

  DataSourceSurface::MappedSurface map;
  if (!dataSurface->Map(DataSourceSurface::MapType::READ, &map)) {
    return nullptr;
  }

  if (mSurfaceFormat == SurfaceFormat::R8G8B8A8 ||
      mSurfaceFormat == SurfaceFormat::R8G8B8X8) {
    libyuv::ABGRToARGB(mBuffer, mStride, map.mData, map.mStride, mSize.width, mSize.height);
  } else {
    memcpy(map.mData, mBuffer, mBufferLength);
  }

  dataSurface->Unmap();

  return dataSurface.forget();
}

already_AddRefed<Image>
ImageBitmapImage::Clone() const
{
  nsRefPtr<ImageContainer> container = LayerManager::CreateImageContainer();
  nsRefPtr<layers::Image> image = container->CreateImage(ImageFormat::IMAGEBITMAP_BACKEND);
  if (!image) {
    return nullptr;
  }

  ImageBitmapImage* backend = static_cast<ImageBitmapImage*>(image.get());
  if (!backend->Init(mSize, mStride, mSurfaceFormat)) {
    return nullptr;
  }

  memcpy(backend->GetBuffer(), mBuffer, mBufferLength);

  return image.forget();
}

}
}
