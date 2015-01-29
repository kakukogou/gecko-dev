/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoStreamTrack.h"
#include "WorkerPrivate.h"
#include "mozilla/dom/VideoStreamTrackBinding.h"

namespace mozilla {
namespace dom {

VideoStreamTrack::VideoStreamTrack(DOMMediaStream* aStream, TrackID aTrackID)
  : MediaStreamTrack(aStream, aTrackID),
    mForkSource(nullptr, -1) {}

JSObject*
VideoStreamTrack::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VideoStreamTrackBinding::Wrap(aCx, this, aGivenProto);
}

}
}
