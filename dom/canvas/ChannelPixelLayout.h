/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ChannelPixelLayout_h
#define mozilla_dom_ChannelPixelLayout_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

struct JSContext;

namespace mozilla {
namespace dom {

class ChannelPixelLayout final : public nsISupports /* or NonRefcountedDOMObject if this is a non-refcounted object */,
                                 public nsWrapperCache /* Change wrapperCache in the binding configuration if you don't want this */
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ChannelPixelLayout)

  friend class ColorFormatPixelLayout;

public:
  ChannelPixelLayout();

protected:
  ~ChannelPixelLayout();

public:
  // TODO: return something sensible here, and change the return type
  ChannelPixelLayout* GetParentObject() const
  {
    return nullptr;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  uint32_t Offset() const
  {
    return mOffset;
  }

  uint32_t Width() const
  {
    return mWidth;
  }

  uint32_t Height() const
  {
    return mHeight;
  }

  uint32_t Stride() const
  {
    return mStride;
  }

  uint32_t Skip() const
  {
    return mSkip;
  }

private:
  uint32_t mOffset;
  uint32_t mWidth;
  uint32_t mHeight;
  uint32_t mStride;
  uint32_t mSkip;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ChannelPixelLayout_h
