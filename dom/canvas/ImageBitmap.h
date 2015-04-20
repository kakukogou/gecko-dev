/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ImageBitmap_h
#define mozilla_dom_ImageBitmap_h

#include "mozilla/Attributes.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/ImageBitmapSource.h"
#include "mozilla/dom/ColorFormatPixelLayout.h"
#include "mozilla/gfx/Rect.h"

struct JSContext;
struct JSStructuredCloneWriter;

namespace mozilla {

class ErrorResult;

namespace gfx {
class SourceSurface;
enum class SurfaceFormat : int8_t;
}

namespace layers {
class Image;
}

namespace dom {

class CanvasRenderingContext2D;
class File;
class HTMLCanvasElement;
class HTMLImageElement;
class HTMLVideoElement;
class ImageData;
class Promise;
class CreateImageBitmapFromBlob;
class CreateImageBitmapFromBlobTask;
class CreateImageBitmapFromBlobWorkerTask;
enum class ColorFormat : uint32_t;

class ImageBitmap final : public nsISupports,
                          public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ImageBitmap)

  nsCOMPtr<nsIGlobalObject> GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  uint32_t Width() const
  {
    return mCropRect.Width();
  }

  uint32_t Height() const
  {
    return mCropRect.Height();
  }

  TemporaryRef<gfx::SourceSurface>
  PrepareForDrawTarget(gfx::DrawTarget* aTarget);

  static already_AddRefed<Promise>
  Create(nsIGlobalObject* aGlobal, const ImageBitmapSource& aSrc,
         bool aCrop, const gfx::IntRect& aCropRect, ErrorResult& aRv);

  // Structured clone methods use these to clone ImageBitmap.
  static JSObject*
  ReadStructuredClone(JSContext* aCx, JSStructuredCloneReader* aReader);

  static bool
  WriteStructuredClone(JSContext* aCx, JSStructuredCloneWriter* aWriter, ImageBitmap* aImageBitmap);


  friend CreateImageBitmapFromBlob;
  friend CreateImageBitmapFromBlobTask;
  friend CreateImageBitmapFromBlobWorkerTask;

  // Mozilla extensions
  ColorFormat
  FindOptimalFormat(const Optional<Sequence<ColorFormat>>& possibleFormats);

  int32_t
  MappedDataLength(ColorFormat format, ErrorResult& aRv);

  already_AddRefed<ColorFormatPixelLayout>
  MapDataInto(ColorFormat format, const ArrayBuffer& aWrapBuffer, int32_t offset, int32_t length, ErrorResult& aRv);

  bool SetDataFrom(ColorFormat aFormat, const ArrayBuffer& aWrapBuffer, int32_t aOffset, int32_t aLength, int32_t aWidth, int32_t aHeight, int32_t aStride, ErrorResult& aRv);

protected:

  ImageBitmap(nsIGlobalObject* aGlobal, layers::Image* aBackend);

  virtual ~ImageBitmap();

  void SetCrop(const gfx::IntRect& aRect, ErrorResult& aRv);

  ColorFormat
  GetColorFormat() const;

  already_AddRefed<ColorFormatPixelLayout>
  GetColorFormatPixelLayout() const;

  template<class HTMLElementType>
  static already_AddRefed<ImageBitmap>
  CreateFromElement(nsIGlobalObject* aGlobal, HTMLElementType& aElement, ErrorResult& aRv);

  static already_AddRefed<ImageBitmap>
  CreateInternal(nsIGlobalObject* aGlobal, HTMLImageElement& aImageEl, ErrorResult& aRv);

  static already_AddRefed<ImageBitmap>
  CreateInternal(nsIGlobalObject* aGlobal, HTMLVideoElement& aVideoEl, ErrorResult& aRv);

  static already_AddRefed<ImageBitmap>
  CreateInternal(nsIGlobalObject* aGlobal, HTMLCanvasElement& aCanvasEl, ErrorResult& aRv);

  static already_AddRefed<ImageBitmap>
  CreateInternal(nsIGlobalObject* aGlobal, ImageData& aImageData, ErrorResult& aRv);

  static already_AddRefed<ImageBitmap>
  CreateInternal(nsIGlobalObject* aGlobal, CanvasRenderingContext2D& aCanvasCtx, ErrorResult& aRv);

  static already_AddRefed<ImageBitmap>
  CreateInternal(nsIGlobalObject* aGlobal, ImageBitmap& aImageBitmap, ErrorResult& aRv);

  gfx::IntRect mCropRect;
  nsRefPtr<layers::Image> mBackend;
  nsCOMPtr<nsIGlobalObject> mParent;
  RefPtr<gfx::SourceSurface> mSurface;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ImageBitmap_h


