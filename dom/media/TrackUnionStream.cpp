/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamGraphImpl.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/unused.h"

#include "AudioSegment.h"
#include "VideoSegment.h"
#include "nsContentUtils.h"
#include "nsIAppShell.h"
#include "nsIObserver.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "nsWidgetsCID.h"
#include "prerror.h"
#include "prlog.h"
#include "mozilla/Attributes.h"
#include "TrackUnionStream.h"
#include "ImageContainer.h"
#include "AudioChannelService.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioNodeExternalInputStream.h"
#include "webaudio/MediaStreamAudioDestinationNode.h"
#include <algorithm>
#include "DOMMediaStream.h"
#include "GeckoProfiler.h"
#ifdef MOZ_WEBRTC
#include "AudioOutputObserver.h"
#endif

#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "VideoProcessEvent.h"
#include "WorkerScope.h"
#include "nsQueryObject.h"
#include "VideoStreamTrack.h"
#include "mozilla/dom/ImageBitmap.h"

using namespace mozilla::layers;
using namespace mozilla::dom;
using namespace mozilla::gfx;

namespace mozilla {

using dom::workers::VideoWorkerPrivate;

#ifdef STREAM_LOG
#undef STREAM_LOG
#endif

PRLogModuleInfo* gTrackUnionStreamLog;
#define STREAM_LOG(type, msg) PR_LOG(gTrackUnionStreamLog, type, msg)

TrackUnionStream::TrackUnionStream(DOMMediaStream* aWrapper) :
  ProcessedMediaStream(aWrapper)
{
  if (!gTrackUnionStreamLog) {
    gTrackUnionStreamLog = PR_NewLogModule("TrackUnionStream");
  }
}

  void TrackUnionStream::RemoveInput(MediaInputPort* aPort)
  {
    for (int32_t i = mTrackMap.Length() - 1; i >= 0; --i) {
      if (mTrackMap[i].mInputPort == aPort) {
        EndTrack(i);
        mTrackMap.RemoveElementAt(i);
      }
    }
    ProcessedMediaStream::RemoveInput(aPort);
  }
  void TrackUnionStream::ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags)
  {
    if (IsFinishedOnGraphThread()) {
      return;
    }
    nsAutoTArray<bool,8> mappedTracksFinished;
    nsAutoTArray<bool,8> mappedTracksWithMatchingInputTracks;
    for (uint32_t i = 0; i < mTrackMap.Length(); ++i) {
      mappedTracksFinished.AppendElement(true);
      mappedTracksWithMatchingInputTracks.AppendElement(false);
    }
    bool allFinished = !mInputs.IsEmpty();
    bool allHaveCurrentData = !mInputs.IsEmpty();
    for (uint32_t i = 0; i < mInputs.Length(); ++i) {
      MediaStream* stream = mInputs[i]->GetSource();
      if (!stream->IsFinishedOnGraphThread()) {
        // XXX we really should check whether 'stream' has finished within time aTo,
        // not just that it's finishing when all its queued data eventually runs
        // out.
        allFinished = false;
      }
      if (!stream->HasCurrentData()) {
        allHaveCurrentData = false;
      }
      bool trackAdded = false;
      for (StreamBuffer::TrackIter tracks(stream->GetStreamBuffer());
           !tracks.IsEnded(); tracks.Next()) {
        bool found = false;
        for (uint32_t j = 0; j < mTrackMap.Length(); ++j) {
          TrackMapEntry* map = &mTrackMap[j];
          if (map->mInputPort == mInputs[i] && map->mInputTrackID == tracks->GetID()) {
            bool trackFinished;
            StreamBuffer::Track* outputTrack = mBuffer.FindTrack(map->mOutputTrackID);
            if (!outputTrack || outputTrack->IsEnded()) {
              trackFinished = true;
            } else {
              CopyTrackData(tracks.get(), j, aFrom, aTo, &trackFinished);
            }
            mappedTracksFinished[j] = trackFinished;
            mappedTracksWithMatchingInputTracks[j] = true;
            found = true;
            break;
          }
        }
        if (!found) {
          bool trackFinished = false;
          trackAdded = true;
          uint32_t mapIndex = AddTrack(mInputs[i], tracks.get(), aFrom);
          CopyTrackData(tracks.get(), mapIndex, aFrom, aTo, &trackFinished);
          mappedTracksFinished.AppendElement(trackFinished);
          mappedTracksWithMatchingInputTracks.AppendElement(true);
        }
      }
      if (trackAdded) {
        for (MediaStreamListener* l : mListeners) {
          l->NotifyFinishedTrackCreation(Graph());
        }
      }
    }
    for (int32_t i = mTrackMap.Length() - 1; i >= 0; --i) {
      if (mappedTracksFinished[i]) {
        EndTrack(i);
      } else {
        allFinished = false;
      }
      if (!mappedTracksWithMatchingInputTracks[i]) {
        mTrackMap.RemoveElementAt(i);
      }
    }
    if (allFinished && mAutofinish && (aFlags & ALLOW_FINISH)) {
      // All streams have finished and won't add any more tracks, and
      // all our tracks have actually finished and been removed from our map,
      // so we're finished now.
      FinishOnGraphThread();
    } else {
      mBuffer.AdvanceKnownTracksTime(GraphTimeToStreamTime(aTo));
    }
    if (allHaveCurrentData) {
      // We can make progress if we're not blocked
      mHasCurrentData = true;
    }
  }

  // Forward SetTrackEnabled(output_track_id, enabled) to the Source MediaStream,
  // translating the output track ID into the correct ID in the source.
  void TrackUnionStream::ForwardTrackEnabled(TrackID aOutputID, bool aEnabled)
  {
    for (int32_t i = mTrackMap.Length() - 1; i >= 0; --i) {
      if (mTrackMap[i].mOutputTrackID == aOutputID) {
        mTrackMap[i].mInputPort->GetSource()->
          SetTrackEnabled(mTrackMap[i].mInputTrackID, aEnabled);
      }
    }
  }

  void TrackUnionStream::AddWorkerMonitor(dom::workers::VideoWorkerPrivate* aWorker,
                                          TrackID aTrackID,
                                          const nsString& aID)
  {
    class Message final : public ControlMessage
    {
    public:
      Message(TrackUnionStream* aStream, VideoWorkerPrivate* aWorker,
              TrackID aTrackID, const nsString& aID)
        : ControlMessage(aStream), mVideoWorker(aWorker),
          mTrackID(aTrackID), mID(aID)
      {}
      virtual void Run() override
      {
        mStream->AsTrackUnionStream()->AddWorkerMonitorImpl(mVideoWorker, mTrackID, mID);
      }
      VideoWorkerPrivate* mVideoWorker;
      TrackID mTrackID;
      nsString mID;
    };
    GraphImpl()->AppendMessage(new Message(this, aWorker,
                                           aTrackID, aID));
  }

  void TrackUnionStream::AddWorkerMonitorImpl(dom::workers::VideoWorkerPrivate* aWorker,
                                              TrackID aTrackID,
                                              const nsString& aID)
  {
    for (uint32_t i = 0; i < mTrackMap.Length(); ++i) {
      TrackMapEntry* entry = &mTrackMap[i];
      if (entry->mOutputTrackID == aTrackID) {
        entry->mID = aID;
        bool bFound = false;
        for (uint32_t j = 0; j < entry->mVideoWorkerMonitors.Length(); ++j) {
          if (aWorker == entry->mVideoWorkerMonitors[j]) {
            bFound = true;
          }
        }
        if (!bFound) {
          entry->mVideoWorkerMonitors.AppendElement(aWorker);
        }
      }
    }
  }

  void TrackUnionStream::RemoveWorkerMonitor(dom::workers::VideoWorkerPrivate* aWorker,
                                             TrackID aTrackID)
  {
    class Message final : public ControlMessage
    {
    public:
      Message(TrackUnionStream* aStream, VideoWorkerPrivate* aWorker,
              TrackID aTrackID)
        : ControlMessage(aStream), mVideoWorker(aWorker),
          mTrackID(aTrackID)
      {}
      virtual void Run() override
      {
        mStream->AsTrackUnionStream()->RemoveWorkerMonitorImpl(mVideoWorker, mTrackID);
      }
      VideoWorkerPrivate* mVideoWorker;
      TrackID mTrackID;
    };
    GraphImpl()->AppendMessage(new Message(this, aWorker,
                                           aTrackID));
  }

  void TrackUnionStream::RemoveWorkerMonitorImpl(dom::workers::VideoWorkerPrivate* aWorker,
                                                 TrackID aTrackID)
  {
    for (uint32_t i = 0; i < mTrackMap.Length(); ++i) {
      TrackMapEntry* entry = &mTrackMap[i];
      if (entry->mOutputTrackID == aTrackID) {
        if (entry->mVideoWorkerMonitors.RemoveElement(aWorker)) {
          return;
        }
      }
    }
  }

  void TrackUnionStream::AddWorkerProcessor(dom::workers::VideoWorkerPrivate* aWorker,
                                            TrackID aTrackID,
                                            const nsString& aID,
                                            MediaInputPort* aPort)
  {
    class Message final : public ControlMessage
    {
    public:
      Message(TrackUnionStream* aStream, VideoWorkerPrivate* aWorker,
              TrackID aTrackID, const nsString& aID,
              MediaInputPort* aPort)
        : ControlMessage(aStream), mVideoWorker(aWorker),
          mTrackID(aTrackID), mID(aID), mPort(aPort)
      {}
      virtual void Run() override
      {
        mStream->AsTrackUnionStream()->AddWorkerProcessorImpl(mVideoWorker, mTrackID, mID, mPort);
      }
      VideoWorkerPrivate* mVideoWorker;
      TrackID mTrackID;
      nsString mID;
      MediaInputPort* mPort;
    };
    GraphImpl()->AppendMessage(new Message(this, aWorker,
                                           aTrackID, aID, aPort));

  }

  void TrackUnionStream::AddWorkerProcessorImpl(dom::workers::VideoWorkerPrivate* aWorker,
                                                TrackID aTrackID,
                                                const nsString& aID,
                                                MediaInputPort* aPort)
  {
    for (uint32_t i = 0; i < mTrackMap.Length(); ++i) {
      TrackMapEntry* entry = &mTrackMap[i];
      if (entry->mInputTrackID == aTrackID && entry->mInputPort == aPort) {
        entry->mID = aID;
        if (entry->mVideoWorkerProcessor) {
          entry->mVideoWorkerProcessor = aWorker;
        }
        return;
      }
    }

    MediaStream* sourceStream = aPort->GetSource();
    for (StreamBuffer::TrackIter tracks(sourceStream->GetStreamBuffer());
         !tracks.IsEnded(); tracks.Next()) {
      TrackID id = tracks->GetID();
      if (id == aTrackID) {
        uint32_t mapIndex = AddTrack(aPort,
                                     tracks.get(),
                                     this->mGraph->CurrentDriver()->StateComputedTime());
        bool trackFinished = false;
        TrackMapEntry* entry = &mTrackMap[mapIndex];
        entry->mVideoWorkerProcessor = aWorker;
      }
    }
  }

  void TrackUnionStream::RemoveWorkerProcessor(dom::workers::VideoWorkerPrivate* aWorker,
                                               TrackID aTrackID)
  {
    class Message final : public ControlMessage
    {
    public:
      Message(TrackUnionStream* aStream, VideoWorkerPrivate* aWorker,
              TrackID aTrackID)
        : ControlMessage(aStream), mVideoWorker(aWorker),
          mTrackID(aTrackID)
      {}
      virtual void Run() override
      {
        mStream->AsTrackUnionStream()->RemoveWorkerProcessorImpl(mVideoWorker, mTrackID);
      }
      VideoWorkerPrivate* mVideoWorker;
      TrackID mTrackID;
    };
    GraphImpl()->AppendMessage(new Message(this, aWorker,
                                           aTrackID));
  }

  void TrackUnionStream::RemoveWorkerProcessorImpl(dom::workers::VideoWorkerPrivate* aWorker,
                                               TrackID aTrackID)
  {
    for (uint32_t i = 0; i < mTrackMap.Length(); ++i) {
      TrackMapEntry* entry = &mTrackMap[i];
      if (entry->mOutputTrackID == aTrackID) {
        entry->mVideoWorkerProcessor = nullptr;
        return;
      }
    }
  }

  uint32_t TrackUnionStream::AddTrack(MediaInputPort* aPort, StreamBuffer::Track* aTrack,
                    GraphTime aFrom)
  {
    // Use the ID of the source track if it's not already assigned to a track,
    // otherwise allocate a new unique ID.
    TrackID id = aTrack->GetID();
    TrackID maxTrackID = 0;
    for (uint32_t i = 0; i < mTrackMap.Length(); ++i) {
      TrackID outID = mTrackMap[i].mOutputTrackID;
      maxTrackID = std::max(maxTrackID, outID);
    }
    // Note: we might have removed it here, but it might still be in the
    // StreamBuffer if the TrackUnionStream sees its input stream flip from
    // A to B, where both A and B have a track with the same ID
    while (1) {
      // search until we find one not in use here, and not in mBuffer
      if (!mBuffer.FindTrack(id)) {
        break;
      }
      id = ++maxTrackID;
    }

    // Round up the track start time so the track, if anything, starts a
    // little later than the true time. This means we'll have enough
    // samples in our input stream to go just beyond the destination time.
    StreamTime outputStart = GraphTimeToStreamTime(aFrom);

    nsAutoPtr<MediaSegment> segment;
    segment = aTrack->GetSegment()->CreateEmptyClone();
    for (uint32_t j = 0; j < mListeners.Length(); ++j) {
      MediaStreamListener* l = mListeners[j];
      l->NotifyQueuedTrackChanges(Graph(), id, outputStart,
                                  MediaStreamListener::TRACK_EVENT_CREATED,
                                  *segment);
    }
    segment->AppendNullData(outputStart);
    StreamBuffer::Track* track =
      &mBuffer.AddTrack(id, outputStart, segment.forget());
    STREAM_LOG(PR_LOG_DEBUG, ("TrackUnionStream %p adding track %d for input stream %p track %d, start ticks %lld",
                              this, id, aPort->GetSource(), aTrack->GetID(),
                              (long long)outputStart));

    TrackMapEntry* map = mTrackMap.AppendElement();
    map->mEndOfConsumedInputTicks = 0;
    map->mEndOfLastInputIntervalInInputStream = -1;
    map->mEndOfLastInputIntervalInOutputStream = -1;
    map->mInputPort = aPort;
    map->mInputTrackID = aTrack->GetID();
    map->mOutputTrackID = track->GetID();
    map->mSegment = aTrack->GetSegment()->CreateEmptyClone();
    map->mLastImage = nullptr;
    map->mOutputImage = nullptr;
    map->mVideoWorkerProcessor = nullptr;
    map->mLastOutputTime = 0;
    return mTrackMap.Length() - 1;
  }

  void TrackUnionStream::EndTrack(uint32_t aIndex)
  {
    StreamBuffer::Track* outputTrack = mBuffer.FindTrack(mTrackMap[aIndex].mOutputTrackID);
    if (!outputTrack || outputTrack->IsEnded())
      return;
    for (uint32_t j = 0; j < mListeners.Length(); ++j) {
      MediaStreamListener* l = mListeners[j];
      StreamTime offset = outputTrack->GetSegment()->GetDuration();
      nsAutoPtr<MediaSegment> segment;
      segment = outputTrack->GetSegment()->CreateEmptyClone();
      l->NotifyQueuedTrackChanges(Graph(), outputTrack->GetID(), offset,
                                  MediaStreamListener::TRACK_EVENT_ENDED,
                                  *segment);
    }
    outputTrack->SetEnded();
  }


  class VideoProcessEventRunnable final : public workers::WorkerRunnable {
  public:
    VideoProcessEventRunnable(workers::WorkerPrivate* aWorkerPrivate,
                              TargetAndBusyBehavior aBehavior,
                              StreamTime aPlaybackTime,
                              const nsString& aID,
                              layers::Image* aImage,
                              layers::Image** aOutputImage)
    :workers::WorkerRunnable(aWorkerPrivate, aBehavior),
     mPlaybackTime(aPlaybackTime),
     mID(aID),
     mImage(aImage),
     mOutputImage(aOutputImage)
    {
    }

  private:
    virtual bool
    WorkerRun(JSContext* aCx, workers::WorkerPrivate* aWorkerPrivate) override
    {
      DOMEventTargetHelper* aTarget = aWorkerPrivate->GlobalScope();
      nsRefPtr<VideoProcessEvent> event =
        new VideoProcessEvent(aTarget,
                              nullptr, nullptr);
      nsresult rv = event->InitVideoProcessEvent(mPlaybackTime, mID, mImage);
      if (NS_FAILED(rv)) {
        xpc::Throw(aCx, rv);
        return false;
      }
      event->SetTrusted(true);

      nsCOMPtr<nsIDOMEvent> domEvent = do_QueryObject(event);

      nsEventStatus dummy = nsEventStatus_eIgnore;
      aTarget->DispatchDOMEvent(nullptr, domEvent, nullptr, &dummy);
      if (event->HasOutputData())
      {
        ErrorResult eResult;
        nsRefPtr<ImageBitmap> output = event->GetOutputImageBitmap(eResult);

        nsRefPtr<layers::Image> tempImage = output->GetImage();
        tempImage.forget(mOutputImage);
      }
      return true;
    }

    virtual bool
    PreDispatch(JSContext* aCx, workers::WorkerPrivate* aWorkerPrivate) override
    {
      if (mBehavior == WorkerThreadModifyBusyCount) {
        return aWorkerPrivate->ModifyBusyCount(aCx, true);
      }

      return true;
    }

    virtual void
    PostDispatch(JSContext* aCx, workers::WorkerPrivate* aWorkerPrivate,
                 bool aDispatchResult) override
    {
      MOZ_ASSERT(aWorkerPrivate);

      if (!aDispatchResult) {
        if (mBehavior == WorkerThreadModifyBusyCount) {
          aWorkerPrivate->ModifyBusyCount(aCx, false);
        }
        if (aCx) {
          JS_ReportPendingException(aCx);
        }
      }
    }

    ~VideoProcessEventRunnable(){
    }
    StreamTime mPlaybackTime;
    nsString mID;
    nsRefPtr<Image> mImage;
    Image** mOutputImage;
  };

  void TrackUnionStream::CopyTrackData(StreamBuffer::Track* aInputTrack,
                     uint32_t aMapIndex, GraphTime aFrom, GraphTime aTo,
                     bool* aOutputTrackFinished)
  {
    TrackMapEntry* map = &mTrackMap[aMapIndex];
    StreamBuffer::Track* outputTrack = mBuffer.FindTrack(map->mOutputTrackID);
    MOZ_ASSERT(outputTrack && !outputTrack->IsEnded(), "Can't copy to ended track");

    MediaSegment* segment = map->mSegment;
    MediaStream* source = map->mInputPort->GetSource();

    GraphTime next;
    *aOutputTrackFinished = false;
    for (GraphTime t = aFrom; t < aTo; t = next) {
      MediaInputPort::InputInterval interval = map->mInputPort->GetNextInputInterval(t);
      interval.mEnd = std::min(interval.mEnd, aTo);
      StreamTime inputEnd = source->GraphTimeToStreamTime(interval.mEnd);
      StreamTime inputTrackEndPoint = STREAM_TIME_MAX;

      if (aInputTrack->IsEnded() &&
          aInputTrack->GetEnd() <= inputEnd) {
        inputTrackEndPoint = aInputTrack->GetEnd();
        *aOutputTrackFinished = true;
      }

      if (interval.mStart >= interval.mEnd) {
        break;
      }
      StreamTime ticks = interval.mEnd - interval.mStart;
      next = interval.mEnd;

      StreamTime outputStart = outputTrack->GetEnd();

      if (interval.mInputIsBlocked) {
        // Maybe the input track ended?
        segment->AppendNullData(ticks);
        STREAM_LOG(PR_LOG_DEBUG+1, ("TrackUnionStream %p appending %lld ticks of null data to track %d",
                   this, (long long)ticks, outputTrack->GetID()));
      } else if (InMutedCycle()) {
        segment->AppendNullData(ticks);
      } else {
        if (GraphImpl()->StreamSuspended(source)) {
          segment->AppendNullData(aTo - aFrom);
        } else {
//          if (map->mVideoWorkerProcessor && (outputTrack->GetEnd() != GraphTimeToStreamTime(interval.mStart))) {
//            StreamTime duration = GraphTimeToStreamTime(interval.mStart) - outputTrack->GetEnd();
//            if (!map->mOutputImage) {
//              outputTrack->GetSegment()->AppendNullData(duration);
//            } else {
//              VideoSegment lastSegment;
//              IntSize frameSize = map->mOutputImage->GetSize();
//              lastSegment.AppendFrame(map->mOutputImage.forget(), duration, frameSize);
//              outputTrack->GetSegment()->AppendFrom(&lastSegment);
//            }
//          } else {
//            MOZ_ASSERT(outputTrack->GetEnd() == GraphTimeToStreamTime(interval.mStart),
//                       "Samples missing");
//          }

          StreamTime inputStart = source->GraphTimeToStreamTime(interval.mStart);
          if (segment->GetType() == MediaSegment::VIDEO){
            if (map->mVideoWorkerProcessor && map->mOutputImage) {
                VideoSegment processedSegment;
                IntSize frameSize = map->mOutputImage->GetSize();
                StreamTime duration = inputEnd - map->mLastOutputTime;
                map->mLastOutputTime = inputEnd;
                processedSegment.AppendFrame(map->mOutputImage.forget(), duration, frameSize);
                segment->AppendFrom(&processedSegment);
            }
            nsAutoPtr<MediaSegment> tmpSegment;
            tmpSegment = aInputTrack->GetSegment()->CreateEmptyClone();
            tmpSegment->AppendSlice(*aInputTrack->GetSegment(),
                                   std::min(inputTrackEndPoint, inputStart),
                                   std::min(inputTrackEndPoint, inputEnd));
            VideoSegment* videoSegment = static_cast<VideoSegment*>(tmpSegment.get());
            for (VideoSegment::ChunkIterator i(*videoSegment); !i.IsEnded(); i.Next()) {
              VideoChunk& chunk = *i;
              VideoFrame& frame = chunk.mFrame;
              VideoFrame::Image* image = frame.GetImage();
              if (image != map->mLastImage) {
                map->mLastImage = image;
                DispatchToVideoWorkerMonitor(map,
                                             std::min(inputTrackEndPoint, source->GraphTimeToStreamTime(interval.mStart)),
                                             map->mID);
                if (map->mVideoWorkerProcessor) {
                  nsRefPtr<VideoProcessEventRunnable> runnable =
                    new VideoProcessEventRunnable(map->mVideoWorkerProcessor,
                                                  workers::WorkerRunnable::WorkerThreadModifyBusyCount,
                                                  std::min(inputTrackEndPoint, source->GraphTimeToStreamTime(interval.mStart)),
                                                  map->mID,
                                                  map->mLastImage,
                                                  getter_AddRefs(map->mOutputImage));
                   if(!runnable->Dispatch(nullptr)) {
             //FIXME        rv.Throw(NS_ERROR_FALURE);
                   }
                }
              }
            }
            if (!map->mVideoWorkerProcessor || !map->mLastImage) {
              segment->AppendFrom(videoSegment);
            }
          } else {
            segment->AppendSlice(*aInputTrack->GetSegment(),
                                 std::min(inputTrackEndPoint, inputStart),
                                 std::min(inputTrackEndPoint, inputEnd));
          }
        }
      }
      ApplyTrackDisabling(outputTrack->GetID(), segment);
      for (uint32_t j = 0; j < mListeners.Length(); ++j) {
        MediaStreamListener* l = mListeners[j];
        l->NotifyQueuedTrackChanges(Graph(), outputTrack->GetID(),
                                    outputStart, 0, *segment);
      }
      outputTrack->GetSegment()->AppendFrom(segment);
    }
  }

  void TrackUnionStream::DispatchToVideoWorkerMonitor(TrackMapEntry* aMapEntry,
                                               StreamTime aPlaybackTime,
                                               const nsString& aID)
  {
    if (0 == aMapEntry->mVideoWorkerMonitors.Length()) {
      return;
    }
    for (uint32_t i = 0; i < aMapEntry->mVideoWorkerMonitors.Length(); ++i) {
      workers::VideoWorkerPrivate* worker = aMapEntry->mVideoWorkerMonitors[i];
      nsRefPtr<VideoProcessEventRunnable> runnable =
          new VideoProcessEventRunnable(worker,
                                        workers::WorkerRunnable::WorkerThreadModifyBusyCount,
                                        aPlaybackTime,
                                        aID,
                                        aMapEntry->mLastImage,
                                        getter_AddRefs(aMapEntry->mOutputImage));
      ErrorResult rv;
      if(!runnable->Dispatch(nullptr)) {
//FIXME        rv.Throw(NS_ERROR_FALURE);
      }
    }
  }
} // namespace mozilla
