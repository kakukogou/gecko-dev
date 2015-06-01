/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ColorFormatPixelLayout_h
#define mozilla_dom_ColorFormatPixelLayout_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

struct JSContext;

namespace mozilla {
namespace layers {
class Image;
struct PlanarYCbCrData;
}
namespace dom {

class ChannelPixelLayout;
enum class ColorFormat : uint32_t;

class ColorFormatPixelLayout final : public nsISupports /* or NonRefcountedDOMObject if this is a non-refcounted object */,
                                     public nsWrapperCache /* Change wrapperCache in the binding configuration if you don't want this */
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ColorFormatPixelLayout)

public:
  ColorFormatPixelLayout();

//  ColorFormatPixelLayout(ColorFormat format,
//                         gfx::SourceSurface *surface,
//                         layers::PlanarYCbCrData const *ycbcrLayout = nullptr);

  ColorFormatPixelLayout(ColorFormat aFormat);

  ColorFormatPixelLayout(ColorFormat aFormat,
                         layers::Image* aImage);

  static already_AddRefed<ColorFormatPixelLayout>
  CreateRGBAFormat(uint32_t aWidth, uint32_t aHeight, uint32_t aStride);

  static already_AddRefed<ColorFormatPixelLayout>
  CreateYUVFormat(uint32_t aYWidth, uint32_t aYHeight, uint32_t aYStride,
                  uint32_t aUWidth, uint32_t aUHeight, uint32_t aUStride,
                  uint32_t aVWidth, uint32_t aVHeight, uint32_t aVStride);

protected:
  ~ColorFormatPixelLayout();

public:
  // TODO: return something sensible here, and change the return type
  ColorFormatPixelLayout* GetParentObject() const
  {
    return nullptr;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void GetChannels(nsTArray<nsRefPtr<ChannelPixelLayout>>& aRetVal) const;

  static ColorFormat GetYUVFormat(ColorFormatPixelLayout const &layout);

private:
  nsTArray<nsRefPtr<ChannelPixelLayout>> mChannels;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ColorFormatPixelLayout_h
