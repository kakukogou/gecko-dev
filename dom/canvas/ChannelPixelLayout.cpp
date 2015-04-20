/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ChannelPixelLayout.h"
#include "mozilla/dom/ImageBitmapBinding.h"

namespace mozilla {
namespace dom {


// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(ChannelPixelLayout)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ChannelPixelLayout)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ChannelPixelLayout)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ChannelPixelLayout)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ChannelPixelLayout::ChannelPixelLayout()
  : mOffset(-1)
  , mWidth(-1)
  , mHeight(-1)
  , mStride(-1)
  , mSkip(-1)
{
    // Add |MOZ_COUNT_CTOR(ChannelPixelLayout);| for a non-refcounted object.
}

ChannelPixelLayout::~ChannelPixelLayout()
{
    // Add |MOZ_COUNT_DTOR(ChannelPixelLayout);| for a non-refcounted object.
}

JSObject*
ChannelPixelLayout::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ChannelPixelLayoutBinding::Wrap(aCx, this, aGivenProto);
}


} // namespace dom
} // namespace mozilla
