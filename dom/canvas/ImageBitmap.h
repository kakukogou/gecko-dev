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
#include "mozilla/gfx/Rect.h"

struct JSContext;

namespace mozilla {

class ErrorResult;

namespace gfx {
class SourceSurface;
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

protected:

  ImageBitmap(nsIGlobalObject* aGlobal, layers::Image* aBackend);

  virtual ~ImageBitmap();

  void SetCrop(const gfx::IntRect& aRect, ErrorResult& aRv);

  template<class HTMLElementType>
  static already_AddRefed<ImageBitmap>
  CreateFromElement(nsIGlobalObject* aGlobal, HTMLElementType& aElement, ErrorResult& aRv);

  static already_AddRefed<ImageBitmap>
  CreateInternal(nsIGlobalObject* aGlobal, HTMLImageElement& aImageEl, ErrorResult& aRv);

  static already_AddRefed<ImageBitmap>
  CreateInternal(nsIGlobalObject* aGlobal, HTMLVideoElement& aVideoEl, ErrorResult& aRv);

  static already_AddRefed<ImageBitmap>
  CreateInternal(nsIGlobalObject* aGlobal, HTMLCanvasElement& aCanvasEl, ErrorResult& aRv);

  gfx::IntRect mCropRect;
  nsRefPtr<layers::Image> mBackend;
  nsCOMPtr<nsIGlobalObject> mParent;
  RefPtr<gfx::SourceSurface> mSurface;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ImageBitmap_h


