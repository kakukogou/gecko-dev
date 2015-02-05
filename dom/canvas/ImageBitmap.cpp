/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/gfx/2D.h"
#include "nsLayoutUtils.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ImageBitmap, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ImageBitmap)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ImageBitmap)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ImageBitmap)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ImageBitmap::ImageBitmap(nsIGlobalObject* aGlobal)
  : mParent(aGlobal)
{
}

ImageBitmap::~ImageBitmap()
{
}

JSObject*
ImageBitmap::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ImageBitmapBinding::Wrap(aCx, this, aGivenProto);
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

  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return promise.forget();
}

} // namespace dom
} // namespace mozilla
