/*
 * ImageBitmapImage.h
 *
 *  Created on: May 14, 2015
 *      Author: kakukogou
 */

#ifndef GFX_LAYERS_IMAGEBITMAPIMAGE_H
#define GFX_LAYERS_IMAGEBITMAPIMAGE_H

#include "ImageContainer.h"

namespace mozilla {

namespace gfx {
enum class SurfaceFormat : int8_t;
}

namespace layers {

class ImageBitmapImage final : public Image {
public:
  ImageBitmapImage();

  ~ImageBitmapImage();

  bool Init(gfx::IntSize const & aSize, uint32_t aStride, gfx::SurfaceFormat aFormat);

  uint8_t* GetBuffer() override
  {
    return mBuffer;
  }

  uint32_t GetBufferLength() const
  {
    return mBufferLength;
  }

  uint32_t GetStride() const
  {
    return mStride;
  }

  gfx::IntSize GetSize() override
  {
    return mSize;
  }

  gfx::SurfaceFormat GetSurfaceFormat() const
  {
    return mSurfaceFormat;
  }

  TemporaryRef<gfx::SourceSurface> GetAsSourceSurface() override;

  already_AddRefed<Image> Clone() const;

private:
  uint8_t* mBuffer;
  uint32_t mBufferLength;
  uint32_t mStride;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mSurfaceFormat;
};

}
}




#endif // GFX_LAYERS_IMAGEBITMAPIMAGE_H
