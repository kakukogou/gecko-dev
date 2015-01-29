/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VideoProcessEvent_h
#define mozilla_dom_VideoProcessEvent_h

#include "mozilla/dom/Event.h"

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

struct JSContext;

namespace mozilla {

namespace layers {
class Image;
}

namespace dom {

namespace workers {
class VideoWorkerPrivate;
}

class ImageBitmap;

class VideoProcessEvent final : public Event
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_EVENT
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(VideoProcessEvent, Event)

public:
  VideoProcessEvent(EventTarget* aOwner,
                    nsPresContext* aPresContext,
                    WidgetEvent* aEvent);

protected:
  virtual ~VideoProcessEvent();

public:
  // TODO: return something sensible here, and change the return type
//  VideoProcessEvent* GetParentObject() const;

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void GetTrackId(nsString& aRetVal) const;

  double PlaybackTime() const;

//  bool IsTrusted() const;

  nsresult InitVideoProcessEvent(StreamTime playbackTime,
                                 const nsAString& aTrackId,
                                 layers::Image* aImage);

  already_AddRefed<ImageBitmap> GetInputImageBitmap(ErrorResult& aRv);
  already_AddRefed<ImageBitmap> GetOutputImageBitmap(ErrorResult& aRv);
  bool HasOutputData() const;
private:
  StreamTime mPlaybackTime;
  nsString mTrackId;
  nsRefPtr<ImageBitmap> mInputImageBitmap;
  nsRefPtr<ImageBitmap> mOutputImageBitmap;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_VideoProcessEvent_h
