/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIASTREAMTRACK_H_
#define MEDIASTREAMTRACK_H_

#include "mozilla/DOMEventTargetHelper.h"
#include "nsID.h"
#include "StreamBuffer.h"

BEGIN_WORKERS_NAMESPACE
class VideoWorkerPrivate;
END_WORKERS_NAMESPACE

namespace mozilla {

class DOMMediaStream;

namespace dom {

class AudioStreamTrack;
class VideoStreamTrack;

/**
 * Class representing a track in a DOMMediaStream.
 */
class MediaStreamTrack : public DOMEventTargetHelper {
public:
  /**
   * aTrackID is the MediaStreamGraph track ID for the track in the
   * MediaStream owned by aStream.
   */
  MediaStreamTrack(DOMMediaStream* aStream, TrackID aTrackID);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaStreamTrack,
                                           DOMEventTargetHelper)

  DOMMediaStream* GetParentObject() const { return mStream; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override = 0;

  DOMMediaStream* GetStream() const { return mStream; }
  TrackID GetTrackID() const { return mTrackID; }
  virtual AudioStreamTrack* AsAudioStreamTrack() { return nullptr; }
  virtual VideoStreamTrack* AsVideoStreamTrack() { return nullptr; }

  // WebIDL
  virtual void GetKind(nsAString& aKind) = 0;
  void GetId(nsAString& aID) const;
  void GetLabel(nsAString& aLabel) { aLabel.Truncate(); }
  bool Enabled() { return mEnabled; }
  void SetEnabled(bool aEnabled);
  void Stop();

  void AddWorkerMonitor(JSContext* cx,
                        workers::VideoWorkerPrivate& worker,
                        JS::Handle<JSObject*> parameters);
  void RemoveWorkerMonitor(JSContext* cx,
                           workers::VideoWorkerPrivate& worker,
                           JS::Handle<JSObject*> parameters);

  already_AddRefed<MediaStreamTrack>
  AddWorkerProcessor(JSContext* cx,
                     workers::VideoWorkerPrivate& worker,
                     JS::Handle<JSObject*> parameters);
  void RemoveWorkerProcessor(JSContext* cx,
                             workers::VideoWorkerPrivate& worker,
                             JS::Handle<JSObject*> parameters);

  // Notifications from the MediaStreamGraph
  void NotifyEnded() { mEnded = true; }

  // Webrtc allows the remote side to name tracks whatever it wants, and we
  // need to surface this to content.
  void AssignId(const nsAString& aID) { mID = aID; }
  void SetStream(DOMMediaStream* stream) { printf_stderr("stream = %p this=%p\n", stream, this); mStream = stream; }
  void SetTrackID(TrackID id) { mTrackID = id; }
protected:
  virtual ~MediaStreamTrack();

  nsRefPtr<DOMMediaStream> mStream;
  TrackID mTrackID;
  nsString mID;
  bool mEnded;
  bool mEnabled;
};

} // namespace dom
} // namespace mozilla

#endif /* MEDIASTREAMTRACK_H_ */
