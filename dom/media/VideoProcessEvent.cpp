/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoProcessEvent.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/VideoProcessEventBinding.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "ImageContainer.h" // for layers::Image

using namespace mozilla::layers;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(VideoProcessEvent)

NS_IMPL_ADDREF_INHERITED(VideoProcessEvent, Event)
NS_IMPL_RELEASE_INHERITED(VideoProcessEvent, Event)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(VideoProcessEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(VideoProcessEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(VideoProcessEvent, Event)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(VideoProcessEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

VideoProcessEvent::VideoProcessEvent(EventTarget* aOwner,
                                     nsPresContext* aPresContext,
                                     WidgetEvent* aEvent)
  : Event(aOwner, aPresContext, aEvent)
  , mPlaybackTime(0)
  , mInputImageBitmap(nullptr)
  , mOutputImageBitmap(nullptr)
{
  // Add |MOZ_COUNT_CTOR(VideoProcessEvent);| for a non-refcounted object.
}

VideoProcessEvent::~VideoProcessEvent()
{
  // Add |MOZ_COUNT_DTOR(VideoProcessEvent);| for a non-refcounted object.
}

JSObject*
VideoProcessEvent::WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VideoProcessEventBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMETHODIMP
VideoProcessEvent::InitVideoProcessEvent(StreamTime playbackTime,
                                         const nsAString& aTrackId,
                                         Image* aImage)
{
   nsresult rv = Event::InitEvent(NS_LITERAL_STRING("videoprocess"),
                                  false /* non-bubbling */,
                                  true /* cancelable */);
   mPlaybackTime = playbackTime;
   mTrackId = aTrackId;
   mInputImageBitmap = ImageBitmap::Create(mOwner, aImage);
   return NS_OK;
}

double
VideoProcessEvent::PlaybackTime() const
{
  return mPlaybackTime;
}

void
VideoProcessEvent::GetTrackId(nsString& aRetVal) const
{
  aRetVal = mTrackId;
}

already_AddRefed<ImageBitmap>
VideoProcessEvent::GetInputImageBitmap(ErrorResult& aRv)
{
  nsRefPtr<ImageBitmap> bitmap = mInputImageBitmap;
  return bitmap.forget();
}

already_AddRefed<ImageBitmap>
VideoProcessEvent::GetOutputImageBitmap(ErrorResult& aRv)
{
  if (!mOutputImageBitmap) {
    mOutputImageBitmap = ImageBitmap::Create(mOwner, nullptr);
  }
  nsRefPtr<ImageBitmap> bitmap = mOutputImageBitmap;
  return bitmap.forget();
}

bool
VideoProcessEvent::HasOutputData() const
{
  if (mOutputImageBitmap) {
    nsRefPtr<layers::Image> tempImage = mOutputImageBitmap->GetImage();
    if (tempImage) {
      return true;
    }
  }
  return false;
}

} // namespace dom
} // namespace mozilla
