/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/ImageBitmapImage.h"
#include "nsLayoutUtils.h"
#include "imgTools.h"
#include "js/StructuredClone.h"

using namespace mozilla::gfx;
using namespace mozilla::layers;

namespace mozilla {
namespace dom {

using namespace workers;

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
CheckSecurityForHTMLElements(bool aIsWriteOnly, bool aCORSUsed, nsIPrincipal* aPrincipal)
{
  if (aIsWriteOnly) {
    return false;
  }

  if (!aCORSUsed) {
    nsIGlobalObject* incumbentSettingsObject = GetIncumbentGlobal();
    if (!incumbentSettingsObject) {
      return false;
    }

    nsIPrincipal* principal = incumbentSettingsObject->PrincipalOrNull();
    if (!principal || !(principal->Subsumes(aPrincipal))) {
      return false;
    }
  }

  return true;
}

static inline bool
CheckSecurityForHTMLElements(const nsLayoutUtils::SurfaceFromElementResult& aRes)
{
  return CheckSecurityForHTMLElements(aRes.mIsWriteOnly, aRes.mCORSUsed, aRes.mPrincipal);
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
  : mCropRect()
  , mBackend(aBackend)
  , mParent(aGlobal)
  , mSurface(nullptr)
{
//  MOZ_ASSERT(aBackend, "aBackend is null in ImageBitmap constructor.");
  if (mBackend) {
    mCropRect.x = 0;
    mCropRect.y = 0;
    mCropRect.width = aBackend->GetSize().width;
    mCropRect.height = aBackend->GetSize().height;
  }
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
    if (mBackend) {
      mSurface = mBackend->GetAsSourceSurface();
    } else {
      return nullptr;
    }
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
  // check network state
  if (aVideoEl.NetworkState() == HTMLMediaElement::NETWORK_EMPTY) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // check ready state
  if (aVideoEl.ReadyState() == HTMLMediaElement::HAVE_NOTHING ||
      aVideoEl.ReadyState() == HTMLMediaElement::HAVE_METADATA) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Check security
  nsCOMPtr<nsIPrincipal> principal = aVideoEl.GetCurrentPrincipal();
  bool CORSUsed = aVideoEl.GetCORSMode() != CORS_NONE;
  if (!CheckSecurityForHTMLElements(false, CORSUsed, principal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // create ImageBitmap
  ImageContainer *container = aVideoEl.GetImageContainer();

  if (!container) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  nsRefPtr<layers::Image> backend = container->LockCurrentImage();
  nsRefPtr<ImageBitmap> ret = new ImageBitmap(aGlobal, backend);
  container->UnlockCurrentImage();

  return ret.forget();
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

/* static */
already_AddRefed<ImageBitmap>
ImageBitmap::CreateInternal(nsIGlobalObject* aGlobal, ImageData& aImageData, ErrorResult& aRv)
{
  // copy data into SourceSurface
  dom::Uint8ClampedArray array;
  DebugOnly<bool> inited = array.Init(aImageData.GetDataObject());
  MOZ_ASSERT(inited);

  array.ComputeLengthAndData();
  const SurfaceFormat FORMAT = SurfaceFormat::B8G8R8A8;
  const uint32_t BYTES_PER_PIXEL = BytesPerPixel(FORMAT);
  const uint32_t imageWidth = aImageData.Width();
  const uint32_t imageHeight = aImageData.Height();
  const uint32_t imageStride = imageWidth * BYTES_PER_PIXEL;
  const uint32_t dataLength = array.Length();
  const gfx::IntSize imageSize(imageWidth, imageHeight);

  // check the ImageData is neutered or not
  if (imageWidth == 0 || imageHeight == 0 ||
      (imageWidth * imageHeight * BYTES_PER_PIXEL) != dataLength) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsRefPtr<layers::Image> backend =
    CreateImageFromRawData(imageSize, imageStride, gfx::SurfaceFormat::B8G8R8A8, array.Data(), dataLength, aRv);

  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<ImageBitmap> ret = new ImageBitmap(aGlobal, backend);
  return ret.forget();
}

/* static */
already_AddRefed<ImageBitmap>
ImageBitmap::CreateInternal(nsIGlobalObject* aGlobal, CanvasRenderingContext2D& aCanvasCtx, ErrorResult& aRv)
{
  // check origin-clean
  if (aCanvasCtx.GetCanvas()->IsWriteOnly()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  RefPtr<SourceSurface> surface = aCanvasCtx.GetSurfaceSnapshot();

  if (!surface) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  const IntSize surfaceSize = surface->GetSize();
  if (surfaceSize.width == 0 || surfaceSize.height == 0) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsRefPtr<layers::Image> backend = CreateImageFromSurface(surface, aRv);

  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<ImageBitmap> ret = new ImageBitmap(aGlobal, backend);
  return ret.forget();
}

/* static */
already_AddRefed<ImageBitmap>
ImageBitmap::CreateInternal(nsIGlobalObject* aGlobal, ImageBitmap& aImageBitmap, ErrorResult& aRv)
{
  ImageBitmapImage* srcBackend = static_cast<ImageBitmapImage*>(aImageBitmap.mBackend.get());

  if (!srcBackend) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  nsRefPtr<layers::Image> backend = srcBackend->Clone();
  nsRefPtr<ImageBitmap> ret = new ImageBitmap(aGlobal, backend);
  return ret.forget();
}

class FulfillImageBitmapPromise
{
protected:
  FulfillImageBitmapPromise(Promise* aPromise, ImageBitmap* aImageBitmap)
  : mPromise(aPromise)
  , mImageBitmap(aImageBitmap)
  {
    MOZ_ASSERT(aPromise);
  }

  void DoFulfillImageBitmapPromise()
  {
    mPromise->MaybeResolve(mImageBitmap);
  }

private:
  nsRefPtr<Promise> mPromise;
  nsRefPtr<ImageBitmap> mImageBitmap;
};

class FulfillImageBitmapPromiseTask final : public nsRunnable,
                                            public FulfillImageBitmapPromise
{
public:
  FulfillImageBitmapPromiseTask(Promise* aPromise, ImageBitmap* aImageBitmap)
  : FulfillImageBitmapPromise(aPromise, aImageBitmap)
  {
  }

  NS_IMETHOD Run() override
  {
    DoFulfillImageBitmapPromise();
    return NS_OK;
  }
};

class FulfillImageBitmapPromiseWorkerTask final : public WorkerSameThreadRunnable,
                                                  public FulfillImageBitmapPromise
{
public:
  FulfillImageBitmapPromiseWorkerTask(Promise* aPromise, ImageBitmap* aImageBitmap)
  : WorkerSameThreadRunnable(GetCurrentThreadWorkerPrivate()),
    FulfillImageBitmapPromise(aPromise, aImageBitmap)
  {
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    DoFulfillImageBitmapPromise();
    return true;
  }
};

static inline void
AsyncFulfillImageBitmapPromise(Promise* aPromise, ImageBitmap* aImageBitmap)
{
  if (NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> task =
      new FulfillImageBitmapPromiseTask(aPromise, aImageBitmap);
    NS_DispatchToCurrentThread(task); // Actually, to the main-thread.
  } else {
    nsRefPtr<FulfillImageBitmapPromiseWorkerTask> task =
      new FulfillImageBitmapPromiseWorkerTask(aPromise, aImageBitmap);
    task->Dispatch(GetCurrentThreadWorkerPrivate()->GetJSContext()); // Actually, to the current worker-thread.
  }
}

static inline TemporaryRef<SourceSurface>
DecodeBlob(Blob& aBlob, ErrorResult& aRv)
{
  // get the internal stream of the blob
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = aBlob.Impl()->GetInternalStream(getter_AddRefs(stream));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  // get the MIME type string of the blob
  // the type will be checked in the DecodeImage() method.
  nsAutoString mimeTypeUTF16;
  aBlob.GetType(mimeTypeUTF16);

  // get the Component object;
  nsCOMPtr<imgITools> imgtool = do_GetService(NS_IMGTOOLS_CID);
  if (!imgtool) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  // decode image
  NS_ConvertUTF16toUTF8 mimeTypeUTF8(mimeTypeUTF16); // NS_ConvertUTF16toUTF8 ---|> nsAutoCString
  nsCOMPtr<imgIContainer> imgContainer;
  rv = imgtool->DecodeImage(stream, mimeTypeUTF8, getter_AddRefs(imgContainer));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  // get the surface out
  uint32_t frameFlags = imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_WANT_DATA_SURFACE;
  uint32_t whichFrame = imgIContainer::FRAME_FIRST;
  RefPtr<SourceSurface> surface = imgContainer->GetFrame(whichFrame, frameFlags);

  if (!surface) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  return surface.forget();
}

class CreateImageBitmapFromBlob
{
protected:
  CreateImageBitmapFromBlob(Promise* aPromise,
                            nsIGlobalObject* aGlobal,
                            Blob& aBlob,
                            bool aCrop,
                            const IntRect& aCropRect)
  : mPromise(aPromise),
    mGlobalObject(aGlobal),
    mBlob(&aBlob),
    mCrop(aCrop),
    mCropRect(aCropRect)
  {
  }

  virtual ~CreateImageBitmapFromBlob()
  {
  }

  void DoCreateImageBitmapFromBlob(ErrorResult& aRv)
  {
    nsRefPtr<ImageBitmap> imageBitmap = CreateImageBitmap(aRv);

    // handle errors while creating ImageBitmap
    // (1) error occurs during reading of the object
    // (2) the image data is not in a supported file format
    // (3) the image data is corrupted
    // All these three cases should reject promise with null value
    if (aRv.Failed()) {
      mPromise->MaybeReject(aRv);
      return;
    }

    if (imageBitmap && mCrop) {
      imageBitmap->SetCrop(mCropRect, aRv);

      if (aRv.Failed()) {
        mPromise->MaybeReject(aRv);
        return;
      }
    }

    mPromise->MaybeResolve(imageBitmap);
    return;
  }

  virtual already_AddRefed<ImageBitmap> CreateImageBitmap(ErrorResult& aRv) = 0;

  nsRefPtr<Promise> mPromise;
  nsCOMPtr<nsIGlobalObject> mGlobalObject;
  RefPtr<mozilla::dom::Blob> mBlob;
  bool mCrop;
  IntRect mCropRect;
};

class CreateImageBitmapFromBlobTask final : public nsRunnable,
                                            public CreateImageBitmapFromBlob
{
public:
  CreateImageBitmapFromBlobTask(Promise* aPromise,
                                nsIGlobalObject* aGlobal,
                                Blob& aBlob,
                                bool aCrop, IntRect const &aCropRect)
  :CreateImageBitmapFromBlob(aPromise, aGlobal, aBlob, aCrop, aCropRect)
  {
  }

  NS_IMETHOD Run() override
  {
    ErrorResult error;
    DoCreateImageBitmapFromBlob(error);
    return NS_OK;
  }

private:
  already_AddRefed<ImageBitmap> CreateImageBitmap(ErrorResult& aRv) override
  {
    RefPtr<SourceSurface> surface = DecodeBlob(*mBlob, aRv);

    if (aRv.Failed()) {
      return nullptr;
    }

    nsRefPtr<layers::Image> backend = CreateImageFromSurface(surface, aRv);

    if (aRv.Failed()) {
      return nullptr;
    }

    // create ImageBitmap object
    nsRefPtr<ImageBitmap> imageBitmap = new ImageBitmap(mGlobalObject, backend);
    return imageBitmap.forget();
  }
};

class CreateImageBitmapFromBlobWorkerTask final : public WorkerSameThreadRunnable,
                                                  public CreateImageBitmapFromBlob
{
  // This is a synchronous task.
  class DecodeBlobInMainThreadSyncTask final : public WorkerMainThreadRunnable
  {
  public:
    DecodeBlobInMainThreadSyncTask(WorkerPrivate* aWorkerPrivate,
                                   Blob* aBlob,
                                   ErrorResult* aError,
                                   layers::Image** aImage)
    : WorkerMainThreadRunnable(aWorkerPrivate)
    , mBlob(aBlob)
    , mError(aError)
    , mImage(aImage)
    {
    }

    bool MainThreadRun() override
    {
      // Decode the blob into a SourceSurface
      RefPtr<SourceSurface> surface = DecodeBlob(*mBlob, *mError);

      if (mError->Failed()) {
        return false;
      }

      nsRefPtr<layers::Image> image = CreateImageFromSurface(surface, *mError);
      if (mError->Failed()) {
        return false;
      }

      image.forget(mImage);

      return true;
    }

  private:
    Blob* mBlob;
    ErrorResult* mError;
    layers::Image** mImage;
  };

public:
  CreateImageBitmapFromBlobWorkerTask(Promise* aPromise,
                                  nsIGlobalObject* aGlobal,
                                  mozilla::dom::Blob& aBlob,
                                  bool aCrop, IntRect const &aCropRect)
  : WorkerSameThreadRunnable(GetCurrentThreadWorkerPrivate()),
    CreateImageBitmapFromBlob(aPromise, aGlobal, aBlob, aCrop, aCropRect)
  {
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    ErrorResult error;
    DoCreateImageBitmapFromBlob(error);
    return !(error.Failed());
  }

private:
  already_AddRefed<ImageBitmap> CreateImageBitmap(ErrorResult& aRv) override
  {
    nsRefPtr<layers::Image> backend;

    nsRefPtr<DecodeBlobInMainThreadSyncTask> task =
      new DecodeBlobInMainThreadSyncTask(mWorkerPrivate, mBlob, &aRv, getter_AddRefs(backend));
    task->Dispatch(mWorkerPrivate->GetJSContext()); // This is a synchronous call.

    if (aRv.Failed()) {
      mPromise->MaybeReject(aRv);
      return nullptr;
    }

    // create ImageBitmap object
    nsRefPtr<ImageBitmap> imageBitmap = new ImageBitmap(mGlobalObject, backend);
    return imageBitmap.forget();
  }

};

static inline void
AsyncCreateImageBitmapFromBlob(Promise* aPromise, nsIGlobalObject* aGlobal,
                               Blob& aBlob, bool aCrop, IntRect const &aCropRect)
{
  if (NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> task =
      new CreateImageBitmapFromBlobTask(aPromise, aGlobal, aBlob, aCrop, aCropRect);
    NS_DispatchToCurrentThread(task); // Actually, to the main-thread.
  } else {
    nsRefPtr<CreateImageBitmapFromBlobWorkerTask> task =
      new CreateImageBitmapFromBlobWorkerTask(aPromise, aGlobal, aBlob, aCrop, aCropRect);
    task->Dispatch(GetCurrentThreadWorkerPrivate()->GetJSContext()); // Actually, to the current worker-thread.
  }
}

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
  } else if (aSrc.IsImageData()) {
    imageBitmap = CreateInternal(aGlobal, aSrc.GetAsImageData(), aRv);
  } else if (aSrc.IsCanvasRenderingContext2D()) {
    MOZ_ASSERT(NS_IsMainThread(), "Creating ImageBitmap from CanvasRenderingContext2D off the main thread.");
    imageBitmap = CreateInternal(aGlobal, aSrc.GetAsCanvasRenderingContext2D(), aRv);
  } else if (aSrc.IsImageBitmap()) {
    imageBitmap = CreateInternal(aGlobal, aSrc.GetAsImageBitmap(), aRv);
  } else if (aSrc.IsBlob()) {
    AsyncCreateImageBitmapFromBlob(promise, aGlobal, aSrc.GetAsBlob(), aCrop, aCropRect);
    return promise.forget();
  } else {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  }

  if (imageBitmap && aCrop) {
    imageBitmap->SetCrop(aCropRect, aRv);
  }

  if (!aRv.Failed()) {
    AsyncFulfillImageBitmapPromise(promise, imageBitmap);
  }

  return promise.forget();
}

/* static */
already_AddRefed<ImageBitmap>
ImageBitmap::Create(nsIGlobalObject* aGlobal, layers::Image* aImage)
{
  nsRefPtr<ImageBitmap> imagebitmap = new ImageBitmap(aGlobal, aImage);
  return imagebitmap.forget();
}

/* static */
JSObject*
ImageBitmap::ReadStructuredClone(JSContext* aCx, JSStructuredCloneReader* aReader)
{
  uint32_t cropRectX_;
  uint32_t cropRectY_;
  uint32_t cropRectWidth_;
  uint32_t cropRectHeight_;
  void* backend_;

  if (!JS_ReadUint32Pair(aReader, &cropRectX_, &cropRectY_) ||
      !JS_ReadUint32Pair(aReader, &cropRectWidth_, &cropRectHeight_) ||
      !JS_ReadPtr(aReader, &backend_)) {
    return nullptr;
  }

  int32_t cropRectX = BitwiseCast<int32_t>(cropRectX_);
  int32_t cropRectY = BitwiseCast<int32_t>(cropRectY_);
  int32_t cropRectWidth = BitwiseCast<int32_t>(cropRectWidth_);
  int32_t cropRectHeight = BitwiseCast<int32_t>(cropRectHeight_);
  layers::Image* backend = static_cast<layers::Image*>(backend_);

  // Get the current global object.
  nsIGlobalObject *global = xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));
  if (!global) {
    return nullptr;
  }

  // Protect the result from a moving GC in ~nsRefPtr.
  JS::Rooted<JSObject*> result(aCx);

  // Create a new ImageBitmap.
  nsRefPtr<ImageBitmap> imageBitmap = new ImageBitmap(global, backend);

  ErrorResult error;
  imageBitmap->SetCrop(IntRect(cropRectX, cropRectY, cropRectWidth, cropRectHeight), error);
  if (error.Failed()) {
    return nullptr;
  }

  // Wrap it in a JS::Value.
  result = imageBitmap->WrapObject(aCx, JS::NullPtr());

  return result;
}

/* static */
bool
ImageBitmap::WriteStructuredClone(JSContext* aCx, JSStructuredCloneWriter* aWriter, ImageBitmap* aImageBitmap)
{
  const uint32_t cropRectX = BitwiseCast<uint32_t>(aImageBitmap->mCropRect.x);
  const uint32_t cropRectY = BitwiseCast<uint32_t>(aImageBitmap->mCropRect.y);
  const uint32_t cropRectWidth = BitwiseCast<uint32_t>(aImageBitmap->mCropRect.width);
  const uint32_t cropRectHeight = BitwiseCast<uint32_t>(aImageBitmap->mCropRect.height);

  bool writeResult =  JS_WriteUint32Pair(aWriter, SCTAG_DOM_IMAGEBITMAP, 0) &&
                      JS_WriteUint32Pair(aWriter, cropRectX, cropRectY) &&
                      JS_WriteUint32Pair(aWriter, cropRectWidth, cropRectHeight) &&
                      JS_WritePtr(aWriter, aImageBitmap->mBackend);

  return writeResult;
}

} // namespace dom
} // namespace mozilla
