/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/ImageBitmapImage.h"
#include "nsLayoutUtils.h"

using namespace mozilla::gfx;
using namespace mozilla::layers;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ImageBitmap, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ImageBitmap)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ImageBitmap)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ImageBitmap)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

static inline already_AddRefed<layers::Image>
CreateImageFromRawData(gfx::IntSize aSize, uint32_t aStride, gfx::SurfaceFormat aFormat, uint8_t* aBuffer, uint32_t aBufferLength, ErrorResult& aRv)
{
  nsRefPtr<ImageContainer> container = LayerManager::CreateImageContainer();
  nsRefPtr<layers::Image> image = container->CreateImage(ImageFormat::IMAGEBITMAP_BACKEND);
  if (!image) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  ImageBitmapImage* backend = static_cast<ImageBitmapImage*>(image.get());
  if (!backend->Init(aSize, aStride, aFormat)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  if (backend->GetBufferLength() != aBufferLength) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return nullptr;
  }

  memcpy(backend->GetBuffer(), aBuffer, backend->GetBufferLength());

  return image.forget();
}

static inline already_AddRefed<layers::Image>
CreateImageFromSurface(SourceSurface* aSurface, ErrorResult& aRv)
{
  RefPtr<DataSourceSurface> dataSurface = aSurface->GetDataSurface();
  if (!dataSurface) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  DataSourceSurface::MappedSurface map;
  if (!dataSurface->Map(DataSourceSurface::MapType::READ, &map)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  uint32_t bufferLength = aSurface->GetSize().height * map.mStride;
  nsRefPtr<layers::Image> image = CreateImageFromRawData(aSurface->GetSize(), map.mStride, aSurface->GetFormat(), map.mData, bufferLength, aRv);
  dataSurface->Unmap();

  if (aRv.Failed()) {
    return nullptr;
  }

  return image.forget();
}

static inline bool
CheckSecurityForHTMLElements(const nsLayoutUtils::SurfaceFromElementResult& aRes)
{
  if (aRes.mIsWriteOnly) {
    return false;
  }

  if (!aRes.mCORSUsed) {
    nsIGlobalObject* incumbentSettingsObject = GetIncumbentGlobal();
    if (!incumbentSettingsObject) {
      return false;
    }

    nsIPrincipal* principal = incumbentSettingsObject->PrincipalOrNull();
    if (!principal || !(principal->Subsumes(aRes.mPrincipal))) {
      return false;
    }
  }

  return true;
}

static inline bool
CheckNonBitmapSource(HTMLImageElement& aImageEl)
{
  nsresult rv;

  nsCOMPtr<imgIRequest> imgRequest;
  rv = aImageEl.GetRequest(nsIImageLoadingContent::CURRENT_REQUEST, getter_AddRefs(imgRequest));
  if (NS_SUCCEEDED(rv) && imgRequest) {
    nsCOMPtr<imgIContainer> imgContainer;
    rv = imgRequest->GetImage(getter_AddRefs(imgContainer));
    if (NS_SUCCEEDED(rv) && imgContainer && imgContainer->GetType() == imgIContainer::TYPE_RASTER) {
      return true;
    }
  }

  return false;
}

ImageBitmap::ImageBitmap(nsIGlobalObject* aGlobal, layers::Image* aBackend)
  : mCropRect(0, 0, aBackend->GetSize().width, aBackend->GetSize().height)
  , mBackend(aBackend)
  , mParent(aGlobal)
  , mSurface(nullptr)
{
  MOZ_ASSERT(aBackend, "aBackend is null in ImageBitmap constructor.");
}

ImageBitmap::~ImageBitmap()
{
}

JSObject*
ImageBitmap::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ImageBitmapBinding::Wrap(aCx, this, aGivenProto);
}

void
ImageBitmap::SetCrop(const gfx::IntRect& aRect, ErrorResult& aRv)
{
  int32_t width = aRect.Width(), height = aRect.Height(),
          x = aRect.X(), y = aRect.Y();

  // fix up negative dimensions
  if (width < 0) {
    if (width == INT_MIN) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return;
    }

    CheckedInt32 checkedX = CheckedInt32(x) + width;

    if (!checkedX.isValid()) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return;
    }

    x = checkedX.value();
    width = -width;
  }

  if (height < 0) {
    if (height == INT_MIN) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return;
    }

    CheckedInt32 checkedY = CheckedInt32(y) + height;

    if (!checkedY.isValid()) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return;
    }

    y = checkedY.value();
    height = -height;
  }

  mCropRect = IntRect(x, y, width, height);
}

TemporaryRef<SourceSurface>
ImageBitmap::PrepareForDrawTarget(gfx::DrawTarget* aTarget)
{
  MOZ_ASSERT(aTarget);

  if (!mSurface) {
    mSurface = mBackend->GetAsSourceSurface();
  }

  if (!mSurface) {
    return nullptr;
  }

  RefPtr<DrawTarget> target = aTarget;
  IntRect surfRect(0, 0, mSurface->GetSize().width, mSurface->GetSize().height);

  // Check if we still need to crop our surface
  if (!mCropRect.IsEqualEdges(surfRect)) {

    IntRect surfPortion = surfRect.Intersect(mCropRect);

    // the crop lies entirely outside the surface area, nothing to draw
    if (surfPortion.Width() == 0 || surfPortion.Height() == 0) {
      return mSurface = nullptr;
    }

    IntPoint dest(std::max(0, surfPortion.X() - mCropRect.X()),
                  std::max(0, surfPortion.Y() - mCropRect.Y()));

    target = target->CreateSimilarDrawTarget(mCropRect.Size(),
                                             target->GetFormat());
    if (!target) {
      return mSurface = nullptr;
    }

    // Make mCropRect match new surface we've cropped to
    mCropRect.MoveTo(0, 0);
    target->CopySurface(mSurface, surfPortion, dest);
    mSurface = target->Snapshot();
  }

  // Replace our surface with one optimized for the target we're about to draw
  // to, under the assumption it'll likely be drawn again to that target.
  // This call should be a no-op for already-optimized surfaces
  mSurface = target->OptimizeSourceSurface(mSurface);

  return mSurface;
}

/* static */
template<class HTMLElementType>
already_AddRefed<ImageBitmap>
ImageBitmap::CreateFromElement(nsIGlobalObject* aGlobal, HTMLElementType& aElement, ErrorResult& aRv)
{
  nsLayoutUtils::SurfaceFromElementResult res =
    nsLayoutUtils::SurfaceFromElement(&aElement, nsLayoutUtils::SFE_WANT_FIRST_FRAME);

  // check origin-clean
  if (!CheckSecurityForHTMLElements(res)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  if (!res.mSourceSurface) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  nsRefPtr<layers::Image> backend = CreateImageFromSurface(res.mSourceSurface, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<ImageBitmap> ret = new ImageBitmap(aGlobal, backend);
  return ret.forget();
}

/* static */
already_AddRefed<ImageBitmap>
ImageBitmap::CreateInternal(nsIGlobalObject* aGlobal, HTMLImageElement& aImageEl, ErrorResult& aRv)
{
  if (!aImageEl.Complete()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  if (!CheckNonBitmapSource(aImageEl)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return CreateFromElement(aGlobal, aImageEl, aRv);
}

/* static */
already_AddRefed<ImageBitmap>
ImageBitmap::CreateInternal(nsIGlobalObject* aGlobal, HTMLVideoElement& aVideoEl, ErrorResult& aRv)
{
  if (aVideoEl.NetworkState() == HTMLMediaElement::NETWORK_EMPTY) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  if (aVideoEl.ReadyState() == HTMLMediaElement::HAVE_NOTHING ||
      aVideoEl.ReadyState() == HTMLMediaElement::HAVE_METADATA) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return CreateFromElement(aGlobal, aVideoEl, aRv);
}

/* static */
already_AddRefed<ImageBitmap>
ImageBitmap::CreateInternal(nsIGlobalObject* aGlobal, HTMLCanvasElement& aCanvasEl, ErrorResult& aRv)
{
  if (aCanvasEl.Width() == 0 || aCanvasEl.Height() == 0) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return CreateFromElement(aGlobal, aCanvasEl, aRv);
}

static void
FulfillImageBitmapPromise(Promise* aPromise, ImageBitmap* aImageBitmap)
{
  MOZ_ASSERT(aPromise);
  aPromise->MaybeResolve(aImageBitmap);
}

class FulfillImageBitmapPromiseTask : public nsRunnable
{
public:
  FulfillImageBitmapPromiseTask(Promise* aPromise, ImageBitmap* aImageBitmap)
    : mPromise(aPromise)
    , mImageBitmap(aImageBitmap)
  {
  }

  NS_IMETHOD Run()
  {
    FulfillImageBitmapPromise(mPromise, mImageBitmap);
    return NS_OK;
  }

private:
  nsRefPtr<Promise> mPromise;
  nsRefPtr<ImageBitmap> mImageBitmap;
};

/* static */
already_AddRefed<Promise>
ImageBitmap::Create(nsIGlobalObject* aGlobal, const ImageBitmapSource& aSrc,
                    bool aCrop, const IntRect& aCropRect, ErrorResult& aRv)
{
  MOZ_ASSERT(aGlobal);

  nsRefPtr<Promise> promise = Promise::Create(aGlobal, aRv);

  if (aRv.Failed()) {
    return nullptr;
  }

  if (aCrop && (aCropRect.Width() == 0 || aCropRect.Height() == 0)) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return promise.forget();
  }

  nsRefPtr<ImageBitmap> imageBitmap;

  if (aSrc.IsHTMLImageElement()) {
    MOZ_ASSERT(NS_IsMainThread(), "Creating ImageBitmap from HTMLImageElement off the main thread.");
    imageBitmap = CreateInternal(aGlobal, aSrc.GetAsHTMLImageElement(), aRv);
  } else if (aSrc.IsHTMLVideoElement()) {
    MOZ_ASSERT(NS_IsMainThread(), "Creating ImageBitmap from HTMLImageElement off the main thread.");
    imageBitmap = CreateInternal(aGlobal, aSrc.GetAsHTMLVideoElement(), aRv);
  } else if (aSrc.IsHTMLCanvasElement()) {
    MOZ_ASSERT(NS_IsMainThread(), "Creating ImageBitmap from HTMLImageElement off the main thread.");
    imageBitmap = CreateInternal(aGlobal, aSrc.GetAsHTMLCanvasElement(), aRv);
  } else {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  }

  if (imageBitmap && aCrop) {
    imageBitmap->SetCrop(aCropRect, aRv);
  }

  if (!aRv.Failed()) {
    nsCOMPtr<nsIRunnable> runnable =
      new FulfillImageBitmapPromiseTask(promise, imageBitmap);
    NS_DispatchToCurrentThread(runnable);
  }

  return promise.forget();
}

} // namespace dom
} // namespace mozilla
