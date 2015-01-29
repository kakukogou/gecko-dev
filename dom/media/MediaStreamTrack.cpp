/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamTrack.h"

#include "DOMMediaStream.h"
#include "nsIUUIDGenerator.h"
#include "nsServiceManagerUtils.h"
#include "WorkerPrivate.h"
//#include "MediaStreamTrackBinding.h"
#include "AutoFuncLog.h"

namespace mozilla {
namespace dom {

MediaStreamTrack::MediaStreamTrack(DOMMediaStream* aStream, TrackID aTrackID)
  : mStream(aStream), mTrackID(aTrackID), mEnded(false), mEnabled(true)
{
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);

  nsID uuid;
  memset(&uuid, 0, sizeof(uuid));
  if (uuidgen) {
    uuidgen->GenerateUUIDInPlace(&uuid);
  }

  char chars[NSID_LENGTH];
  uuid.ToProvidedString(chars);
  mID = NS_ConvertASCIItoUTF16(chars);
}

MediaStreamTrack::~MediaStreamTrack()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaStreamTrack, DOMEventTargetHelper,
                                   mStream)

NS_IMPL_ADDREF_INHERITED(MediaStreamTrack, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaStreamTrack, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaStreamTrack)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

void
MediaStreamTrack::GetId(nsAString& aID) const
{
  aID = mID;
}

void
MediaStreamTrack::SetEnabled(bool aEnabled)
{
  mEnabled = aEnabled;
  mStream->SetTrackEnabled(mTrackID, aEnabled);
}

void
MediaStreamTrack::Stop()
{
  mStream->StopTrack(mTrackID);
}

void
MediaStreamTrack::AddWorkerMonitor(JSContext* cx,
                                   workers::VideoWorkerPrivate& worker,
                                   JS::Handle<JSObject*> parameters)
{
  mStream->AddWorkerMonitor(&worker, mTrackID, mID);
}

void
MediaStreamTrack::RemoveWorkerMonitor(JSContext* cx,
                                      workers::VideoWorkerPrivate& worker,
                                      JS::Handle<JSObject*> parameters)
{
  mStream->RemoveWorkerMonitor(&worker, mTrackID);
}

already_AddRefed<MediaStreamTrack>
MediaStreamTrack::AddWorkerProcessor(JSContext* cx,
                                     workers::VideoWorkerPrivate& worker,
                                     JS::Handle<JSObject*> parameters)
{
  TAFLOG();
  TFLLOG("worker(%p)", &worker);
  nsRefPtr<VideoStreamTrack> obj = new VideoStreamTrack(nullptr, -1);
  obj->mVideoWorkerProcessor = &worker;
  obj->mForkSource = MakePair(mStream, mTrackID);
  return obj.forget();
}

void
MediaStreamTrack::RemoveWorkerProcessor(JSContext* cx,
                                        workers::VideoWorkerPrivate& worker,
                                        JS::Handle<JSObject*> parameters)
{
  mStream->RemoveWorkerProcessor(&worker, mTrackID);
}

} // namespace dom
} // namespace mozilla
