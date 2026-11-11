# rialto-gstreamer

rialto-gstreamer is a GStreamer plugin that provides custom sink elements enabling media pipelines within containerized applications to offload audio, video, and subtitle rendering to the Rialto server. It acts as the GStreamer-side adapter in the Rialto framework, translating standard GStreamer pipeline events, state transitions, and media buffers into Rialto Client API calls over IPC. The plugin registers four sink elements — a video sink, an audio sink, a subtitle sink, and a web audio sink — each targeting a distinct media type. By running inside the application process while the actual media processing occurs in the Rialto server, the plugin ensures that hardware-specific resources and DRM handles are never exposed to the application container.

From the device perspective, rialto-gstreamer allows browsers and native application runtimes using GStreamer-based media stacks to deliver protected and unprotected AV content without modifications to the playback engine. The plugin intercepts GStreamer pipeline control and data flows and routes them transparently to the Rialto server, which manages the actual hardware-accelerated decode and render pipeline. This decoupling enables robust isolation between application containers and platform media resources.

At the module level, rialto-gstreamer manages the full lifecycle of a media session: sink element construction and capability negotiation, GStreamer state machine transitions mapped to Rialto pipeline state transitions, media buffer parsing and submission (in both pull and push modes), playback control forwarding, EME-based content protection metadata extraction, and event propagation back to the GStreamer pipeline. A shared `MediaPlayerManager` allows multiple sink elements belonging to the same GStreamer pipeline to share a single `GStreamerMSEMediaPlayerClient` instance, coordinating their interactions with one Rialto media pipeline session.

```mermaid
flowchart LR

%% Styles
classDef Apps stroke:#00B9F1,fill:#E6F7FD,stroke-width:2px
classDef RDKMW stroke:#75D701,fill:#F1FFE6,stroke-width:2px
classDef VL stroke:#808080,fill:#F2F2F2,stroke-width:2px

    subgraph Apps["Apps and Runtimes"]
        WPE_RT["WPE Runtime / Browser"]
        GST["GStreamer Pipeline"]
        RGS["rialto-gstreamer Plugin\n(rialtomsevideosink\nrialtomseaudiosink\nrialtomsesubtitlesink\nrialtowebaudiosink)"]
    end

    subgraph RDKMW["RDK Core Middleware"]
        Rialto["Rialto Server\n(Media Pipeline)"]
        Thunder["WPEFramework (Thunder)"]
        AM["App Manager"]
    end

    subgraph VL["Vendor Layer"]
        HAL["Media HAL / DRM Libraries"]
        BSP["BSP"]
    end

    WPE_RT --> GST
    GST --> RGS
    RGS -->|"Rialto Client IPC\n(RIALTO_SOCKET_PATH)"| Rialto
    RDKMW -->|HAL APIs| VL

class Apps Apps
class RDKMW RDKMW
class VL VL
```

**Key Features & Responsibilities:**

- **GStreamer sink element registration**: Registers four GStreamer sink elements (`rialtomsevideosink`, `rialtomseaudiosink`, `rialtomsesubtitlesink`, `rialtowebaudiosink`) at a configurable rank so the GStreamer element selection mechanism routes media data through the Rialto path.
- **Pull-mode media data feeding**: Responds to `notifyNeedMediaData` requests from the Rialto server by dequeuing buffered `GstSample` objects from `PullModePlaybackDelegate`, parsing them through `BufferParser`, and submitting them as `IMediaPipeline::MediaSegment` objects via `addSegment`.
- **Push-mode web audio delivery**: Delivers web audio streams via `GStreamerWebAudioPlayerClient`, queuing incoming GStreamer buffers and pushing PCM audio data to the Rialto web audio backend using `writeBuffer()` as buffer space becomes available.
- **EME / content protection handling**: Extracts per-buffer DRM protection metadata (key ID, IV, subsamples, cipher mode) from GStreamer buffer protection metadata and attaches it to each submitted media segment.
- **Playback lifecycle management**: Maps GStreamer element state transitions (NULL→READY→PAUSED→PLAYING and reverse) to the corresponding Rialto pipeline operations (load, attach source, play, pause, stop, flush).
- **Multi-sink coordination**: Coordinates audio and video sinks sharing the same parent GstBin so that a single Rialto media pipeline session is created and managed jointly by all active sinks through `MediaPlayerManager`.
- **Flush and data synchronization**: Uses `FlushAndDataSynchronizer` to guard against race conditions between flush operations and data submissions across multiple media sources.
- **Rialto log bridging**: Forwards internal Rialto client library log messages into the GStreamer debug log system via `LogToGstHandler`; when built with `USE_ETHANLOG`, the logging macros instead route exclusively to EthanLog (not both outputs).

---

## Design

The plugin is designed around a delegate pattern layered over standard GStreamer element infrastructure. The four MSE sink element types (`RialtoMSEBaseSink`, `RialtoGStreamerMSEVideoSink`, `RialtoGStreamerMSEAudioSink`, `RialtoGStreamerMSESubtitleSink`) inherit the low-level GStreamer element boilerplate from a common base while delegating all media pipeline logic to a `IPlaybackDelegate` implementation; on these sinks delegates are created at the NULL→READY state transition and destroyed at READY→NULL, cleanly separating GStreamer element lifecycle from Rialto session lifecycle. The web audio sink (`RialtoGStreamerWebAudioSink`) is a separate implementation deriving directly from `GST_TYPE_ELEMENT` — it creates its `PushModeAudioPlaybackDelegate` at NULL→READY but does not reset the delegate on READY→NULL, so MSE lifecycle assumptions do not apply to the web audio path. The base sink element routes all GStreamer callbacks — `chain`, `event`, `query`, `change_state`, `send_event` — through the delegate, keeping the GStreamer element code minimal and testable.

The design separates two distinct data-flow modes. In pull mode, used by MSE video, audio, and subtitle sinks, `PullModePlaybackDelegate` maintains a local queue of incoming GStreamer samples and satisfies Rialto server `NeedData` requests by dequeuing, parsing, and submitting samples asynchronously. In push mode, used by the web audio path, `PushModeAudioPlaybackDelegate` drives the `GStreamerWebAudioPlayerClient` which opens a Rialto web audio session and feeds audio frames directly.

Northbound integration with the GStreamer pipeline is handled entirely through standard GStreamer APIs: caps negotiation, pad events, buffer chains, state change notifications, and GObject properties. The sink elements advertise supported capabilities queried dynamically from `IMediaPipelineCapabilities` at element registration time. Southbound integration is entirely through the Rialto Client library interfaces `IMediaPipeline`, `IMediaPipelineClient`, `IWebAudioPlayer`, `IWebAudioPlayerClient`, and `IControl`, which abstract all IPC with the Rialto server.

IPC with the Rialto server is performed entirely by the Rialto Client library using a Unix domain socket identified by the `RIALTO_SOCKET_PATH` environment variable. The `ControlBackend` registers the plugin as a Rialto control client and waits for the server to signal the `RUNNING` application state before allowing state transitions to proceed.

All playback state, position, and configuration are maintained in memory for the duration of the media session and re-established when a new GStreamer pipeline is created.

```mermaid
flowchart LR

    subgraph Sinks["GStreamer Sink Elements"]
        VSink["RialtoGStreamerMSEVideoSink"]
        ASink["RialtoGStreamerMSEAudioSink"]
        SSink["RialtoGStreamerMSESubtitleSink"]
        WASink["RialtoGStreamerWebAudioSink"]
        BaseSink["RialtoMSEBaseSink"]
    end

    subgraph Delegates["Playback Delegates"]
        PullVideo["PullModeVideoPlaybackDelegate"]
        PullAudio["PullModeAudioPlaybackDelegate"]
        PullSubtitle["PullModeSubtitlePlaybackDelegate"]
        PushAudio["PushModeAudioPlaybackDelegate"]
    end

    subgraph Clients["Media Client Layer"]
        MPMgr["MediaPlayerManager"]
        MPClient["GStreamerMSEMediaPlayerClient"]
        WAClient["GStreamerWebAudioPlayerClient"]
        CtrlBackend["ControlBackend"]
    end

    subgraph Support["Support Components"]
        BufParser["BufferParser"]
        MsgQueue["MessageQueue"]
        FlushSync["FlushAndDataSynchronizer"]
        LogHandler["LogToGstHandler"]
        Timer["Timer"]
    end

    subgraph RialtoLib["Rialto Client Library"]
        IMP["IMediaPipeline"]
        IWA["IWebAudioPlayer"]
        ICtrl["IControl"]
    end

    VSink -->|delegates to| PullVideo
    ASink -->|delegates to| PullAudio
    ASink -->|or delegates to| PushAudio
    SSink -->|delegates to| PullSubtitle
    WASink -->|delegates to| PushAudio

    PullVideo --> MPMgr
    PullAudio --> MPMgr
    PullSubtitle --> MPMgr
    MPMgr --> MPClient
    PushAudio --> WAClient

    MPClient --> BufParser
    MPClient --> MsgQueue
    MPClient --> FlushSync
    PullVideo --> CtrlBackend
    PullAudio --> CtrlBackend
    PullSubtitle --> CtrlBackend

    MPClient -->|IMediaPipeline calls| IMP
    WAClient -->|IWebAudioPlayer calls| IWA
    CtrlBackend -->|IControl registration| ICtrl
```

#### Threading Model

- **Threading Architecture**: Multi-threaded with one dedicated worker thread per `MessageQueue` instance, plus per-source `BufferPuller` threads in pull mode.
- **Main Thread**: GStreamer state-change dispatching, property get/set, and pad event handling execute on the calling GStreamer thread (typically a streaming thread or the application thread).
- **Worker Threads**:
  - _MessageQueue worker thread_: Owned by `GStreamerMSEMediaPlayerClient`. Processes all messages posted from Rialto server callbacks (`NeedDataMessage`, `PlaybackStateMessage`, `SetPositionMessage`, `QosMessage`, `BufferUnderflowMessage`, `SourceFlushedMessage`, etc.) serially to prevent concurrent access to pipeline state.
  - _BufferPuller queue thread_: One per attached source in pull mode. Receives `PullBufferMessage` requests, dequeues buffered `GstSample` objects from `PullModePlaybackDelegate`, parses them with `BufferParser`, and submits segments via `addSegment`.
  - _Timer thread_: Used by `Timer` for one-shot or periodic callbacks, primarily in the web audio path.
- **Synchronization**: `m_sinkMutex` (per sink) protects the delegate pointer and queued properties. `m_mediaPlayerClientsMutex` (static on `MediaPlayerManager`) protects the shared media player client map. `m_playbackInfoMutex` protects the `PlaybackInfo` struct updated by the Rialto server notification. `FlushAndDataSynchronizer` uses a `std::condition_variable` in `waitIfRequired()` to delay the start of a new flush until the source is neither mid-flush nor holding post-flush data; sample pulling during a server flush is suppressed separately by the delegate.
- **Async / Event Dispatch**: Most Rialto server callbacks (e.g., `notifyPlaybackState`, `notifyNeedMediaData`) are received on the Rialto client IPC thread and immediately posted as `Message` objects to the `MessageQueue`, which processes them on the worker thread. This prevents IPC callback threads from blocking on GStreamer pipeline operations. Exceptions: `notifyPlaybackInfo` updates a mutex-protected `PlaybackInfo` cache directly, and web audio `notifyState` invokes delegate EOS/error handlers synchronously — neither goes through the `MessageQueue`.

### Prerequisites and Dependencies

#### Platform and Integration Requirements

- **Build Dependencies**: `gstreamer1.0`, `gstreamer1.0-plugins-base` (`gstreamer-app-1.0`, `gstreamer-audio-1.0`, `gstreamer-pbutils-1.0`), `glib-2.0`, `openssl`, `jsoncpp`, `protobuf`, `rialto` (RialtoClient), `rialto-ocdm`. Requires `enable_rialto` Yocto distro feature flag.
- **HAL**: Hardware abstraction is fully handled by the Rialto server; this component operates exclusively through the Rialto Client library interfaces.
- **Systemd Services**: The Rialto server daemon must be running and have signaled the `RUNNING` application state before sink elements can reach READY at all. `ControlBackend::waitForRunning` runs during the NULL→READY transition in both `PullModePlaybackDelegate::changeState()` and `PushModeAudioPlaybackDelegate::changeState()`; if its 1-second wait expires, NULL→READY itself returns `GST_STATE_CHANGE_FAILURE`, so the sink never enters READY.
- **Runtime Configuration**: Runtime behavior is governed by the `RIALTO_SOCKET_PATH` and `RIALTO_SINKS_RANK` environment variables.
- **Startup Order**: The Rialto server process must be active and its Unix domain socket available at `RIALTO_SOCKET_PATH` before any sink element transitions from NULL to READY.

---

### Component State Flow

#### Initialization to Active State

The plugin is loaded by the GStreamer plugin registry when the runtime process starts a GStreamer pipeline. Sink registration occurs inside `rialto_mse_sinks_init`, which checks the `RIALTO_SOCKET_PATH` and `RIALTO_SINKS_RANK` environment variables to determine the element rank. When the rank resolves to zero, `rialto_mse_sinks_init` returns before any `gst_element_register` call, so the sink factories are not registered at all — automatic selection falls back to standard elements and even explicit `gst_element_factory_make("rialtomsevideosink", ...)` fails.

Each MSE sink element transitions through the following states: **NULL** (element constructed, no resources allocated) → **READY** (delegate created, `ControlBackend` initialized, Rialto server `RUNNING` state awaited) → **PAUSED** (shared media player client attached via `MediaPlayerManager`; sources attached later when `GST_EVENT_CAPS` arrives) → **PLAYING** (Rialto pipeline play issued) → **PAUSED** → **READY** → **NULL** (flush, stop, source removal, delegate destroyed, media player client released).

```mermaid
sequenceDiagram
    participant GSTPipeline as GStreamer Pipeline
    participant Sink as RialtoMSEBaseSink
    participant Delegate as PullModePlaybackDelegate
    participant CtrlBackend as ControlBackend
    participant MPMgr as MediaPlayerManager
    participant MPClient as GStreamerMSEMediaPlayerClient
    participant BufPuller as BufferPuller
    participant RialtoClient as Rialto Client IPC

    GSTPipeline->>Sink: NULL_TO_READY state change
    Sink->>Delegate: Create delegate (PullModeVideoPlaybackDelegate etc.)
    Delegate->>CtrlBackend: createControlBackend()
    CtrlBackend->>RialtoClient: IControl::registerClient()
    RialtoClient-->>CtrlBackend: notifyApplicationState(RUNNING)
    Sink-->>GSTPipeline: READY

    GSTPipeline->>Sink: READY_TO_PAUSED state change
    Delegate->>MPMgr: attachMediaPlayerClient()
    MPMgr->>MPClient: create GStreamerMSEMediaPlayerClient
    MPClient->>RialtoClient: createMediaPlayerBackend() + load(MSE)
    RialtoClient-->>MPClient: Backend created
    GSTPipeline->>Sink: GST_EVENT_CAPS
    Delegate->>RialtoClient: attachSource(MediaSource)
    RialtoClient-->>Delegate: sourceId assigned
    GSTPipeline->>Sink: chain() - push GstBuffer
    Sink->>Delegate: handleBuffer() - queue GstSample
    RialtoClient->>MPClient: notifyNeedMediaData(sourceId, frameCount, requestId)
    MPClient->>BufPuller: postMessage(NeedDataMessage / PullBufferMessage)
    BufPuller->>Delegate: getFrontSample() - dequeue buffered sample
    BufPuller->>RialtoClient: addSegment(requestId, mediaSegment)
    BufPuller->>MPClient: postMessage(HaveDataMessage)
    MPClient->>RialtoClient: haveData(status, requestId)

    GSTPipeline->>Sink: PAUSED_TO_PLAYING state change
    Delegate->>RialtoClient: play()
    RialtoClient-->>Delegate: notifyPlaybackState(PLAYING)
    Sink-->>GSTPipeline: PLAYING

    loop Runtime - data feeding
        GSTPipeline->>Sink: chain() - push GstBuffer
        Sink->>Delegate: handleBuffer() - queue GstSample
        RialtoClient->>MPClient: notifyNeedMediaData(sourceId, frameCount, requestId)
        MPClient->>BufPuller: postMessage(NeedDataMessage / PullBufferMessage)
        BufPuller->>Delegate: getFrontSample() - dequeue buffered sample
        BufPuller->>RialtoClient: addSegment(requestId, mediaSegment)
        BufPuller->>MPClient: postMessage(HaveDataMessage)
        MPClient->>RialtoClient: haveData(status, requestId)
    end

    GSTPipeline->>Sink: PLAYING_TO_PAUSED / PAUSED_TO_READY / READY_TO_NULL
    Delegate->>RialtoClient: stop() / removeSource()
    MPMgr->>MPClient: destroyClientBackend()
    Sink-->>GSTPipeline: NULL
```

#### Runtime State Changes

During normal playback, the Rialto server periodically sends `notifyNeedMediaData` to request more frames. The delegate responds by dequeuing buffered samples already queued from the GStreamer pipeline and submitting them. Position and duration updates arrive via `notifyPosition` and `notifyDuration`, which post backend-queue messages that update internal caches (per-source position, `m_duration`) — they do not update the GStreamer segment or emit bus notifications. Position queries instead read the `PlaybackInfo` snapshot refreshed by `notifyPlaybackInfo`, while duration queries consult upstream first and then the backend cache.

**State Change Triggers:**

- `notifyPlaybackState(PLAYING/PAUSED/STOPPED/END_OF_STREAM)` posted to the MessageQueue triggers corresponding GStreamer state or EOS message propagation upstream.
- A flush event from the GStreamer pipeline (`GST_EVENT_FLUSH_START/STOP`) triggers `flush()` on the Rialto pipeline via `MediaPlayerClientBackendInterface::flush()`, and `FlushAndDataSynchronizer::waitIfRequired()` delays starting a new flush while the source is mid-flush; separately, the delegate suppresses sample pulling while the server flush is ongoing.
- `notifyBufferUnderflow` emits the `buffer-underflow-callback` GObject signal on the sink element, which the parent pipeline can handle to adjust buffering.
- `notifyPlaybackError(DECRYPTION)` posts a GStreamer error message to the pipeline bus; `OUTPUT_PROTECTION` instead posts an `HDCPProtectionFailure` application message.

**Context Switching Scenarios:**

- `PullModePlaybackDelegate::notifyApplicationState()` ignores `INACTIVE`; only `UNKNOWN` posts an error via `handleError()`. (`gst_element_lost_state` is invoked on a different path — during asynchronous source flushing, not on an INACTIVE notification.)
- A seek event (`GST_EVENT_SEEK`) updates the local segment and forwards a flushing seek upstream; the subsequent `FLUSH_STOP` invokes `flushServer()`, and the following `SEGMENT` event invokes `setSourcePosition()` to realign the server-side pipeline to the new position.
- Rate change events trigger `setPlaybackRate` on the media player backend.

---

### Call Flows

#### Initialization Call Flow

```mermaid
sequenceDiagram
    participant GSTPipeline as GStreamer Pipeline
    participant Sink as RialtoMSEVideoSink
    participant Delegate as PullModeVideoPlaybackDelegate
    participant MPMgr as MediaPlayerManager
    participant MPClient as GStreamerMSEMediaPlayerClient
    participant RialtoClient as Rialto Client Library

    GSTPipeline->>Sink: NULL_TO_READY
    Sink->>Delegate: new PullModeVideoPlaybackDelegate(element)
    Delegate->>Delegate: createControlBackend()
    Sink-->>GSTPipeline: READY
    GSTPipeline->>Sink: READY_TO_PAUSED
    Delegate->>MPMgr: attachMediaPlayerClient(gstBinParent, maxW, maxH)
    MPMgr->>MPClient: new GStreamerMSEMediaPlayerClient(backend)
    MPClient->>RialtoClient: createMediaPlayerBackend(maxW, maxH)
    MPClient->>RialtoClient: load(MSE, url)
    RialtoClient-->>MPClient: load acknowledged
    MPMgr-->>Delegate: client attached
    Note over Delegate: attachSource() occurs on GST_EVENT_CAPS
```

#### Request Processing Call Flow

Video buffers are pushed into the sink by the upstream GStreamer element through the pad chain function, and `PullModePlaybackDelegate` queues each buffer as a `GstSample`. When the video sink receives a `notifyNeedMediaData` callback from the Rialto server, it is enqueued on the `MessageQueue`. The worker thread dequeues it, posts a `PullBufferMessage` to the `BufferPuller` queue, which dequeues a buffered `GstSample` from the delegate, parses it using `VideoBufferParser`, and calls `addSegment` followed by `haveData` on the Rialto pipeline backend.

```mermaid
sequenceDiagram
    participant GSTPipeline as GStreamer Pipeline
    participant Delegate as PullModePlaybackDelegate
    participant RialtoServer as Rialto Server (IPC)
    participant MPClient as GStreamerMSEMediaPlayerClient
    participant MsgQueue as MessageQueue (Worker Thread)
    participant BufPuller as BufferPuller Thread
    participant BufParser as VideoBufferParser
    participant RialtoBackend as MediaPlayerClientBackend

    GSTPipeline->>Delegate: chain() - push GstBuffer
    Delegate->>Delegate: handleBuffer() - queue GstSample

    RialtoServer->>MPClient: notifyNeedMediaData(sourceId, frameCount, requestId)
    MPClient->>MsgQueue: postMessage(NeedDataMessage)
    MsgQueue->>MsgQueue: dequeue and handle
    MsgQueue->>BufPuller: requestPullBuffer(sourceId, frameCount, requestId)
    BufPuller->>Delegate: getFrontSample() - dequeue buffered sample
    Delegate-->>BufPuller: GstSample
    BufPuller->>BufParser: parseBuffer(sample, buffer, map, streamId)
    BufParser-->>BufPuller: MediaSegment (with timestamps, codec data, protection metadata)
    BufPuller->>RialtoBackend: addSegment(requestId, mediaSegment)
    BufPuller->>MsgQueue: postMessage(HaveDataMessage)
    MsgQueue->>RialtoBackend: haveData(HAVE_DATA, requestId)
    RialtoBackend-->>RialtoServer: segment submitted
```

---

## Internal Modules

| Module / Class                     | Description                                                                                                                                                                                                                                                                                                                                                                                    | Key Files                                                                                               |
| ---------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------- |
| `RialtoGStreamerMSEVideoSink`      | GStreamer sink element for video streams. Registers the `rialtomsevideosink` element. On NULL→READY creates a `PullModeVideoPlaybackDelegate`. Exposes GObject properties for window geometry, max resolution, immediate output, sync mode, and decode error reporting. Emits `first-video-frame-callback` signal.                                                                             | `RialtoGStreamerMSEVideoSink.cpp`, `RialtoGStreamerMSEVideoSink.h`                                      |
| `RialtoGStreamerMSEAudioSink`      | GStreamer sink element for audio streams. Registers the `rialtomseaudiosink` element. Selects between `PullModeAudioPlaybackDelegate` and `PushModeAudioPlaybackDelegate` at NULL→READY based on the `web-audio` GObject property (push mode when set). Implements `GstStreamVolume` interface. Exposes volume, mute, sync, low-latency, buffering, and audio-fade properties. Emits `first-audio-frame-callback` signal. | `RialtoGStreamerMSEAudioSink.cpp`, `RialtoGStreamerMSEAudioSink.h`                                      |
| `RialtoGStreamerMSESubtitleSink`   | GStreamer sink element for subtitle streams. Registers the `rialtomsesubtitlesink` element. Creates a `PullModeSubtitlePlaybackDelegate` on NULL→READY. Exposes mute, text-track-identifier, and window-id properties.                                                                                                                                                                         | `RialtoGStreamerMSESubtitleSink.cpp`, `RialtoGStreamerMSESubtitleSink.h`                                |
| `RialtoGStreamerWebAudioSink`      | GStreamer sink element for web audio streams. Registers the `rialtowebaudiosink` element. Uses `PushModeAudioPlaybackDelegate` backed by `GStreamerWebAudioPlayerClient`. Exposes volume and ts-offset properties.                                                                                                                                                                             | `RialtoGStreamerWebAudioSink.cpp`, `RialtoGStreamerWebAudioSink.h`                                      |
| `RialtoMSEBaseSink`                | Common base GStreamer element for all MSE sinks. Owns the sink pad and routes all GStreamer callbacks (chain, event, query, send_event, change_state) to the active `IPlaybackDelegate`. Manages queued properties before the delegate is initialized.                                                                                                                                         | `RialtoGStreamerMSEBaseSink.cpp`, `RialtoGStreamerMSEBaseSink.h`, `RialtoGStreamerMSEBaseSinkPrivate.h` |
| `PullModePlaybackDelegate`         | Base class for pull-mode delegates. Manages the GStreamer sample queue, handles upstream pad events (EOS, flush, segment, caps), drives the `MediaPlayerManager` attachment, and orchestrates the flush sequence with the Rialto server.                                                                                                                                                       | `PullModePlaybackDelegate.cpp`, `PullModePlaybackDelegate.h`                                            |
| `PullModeVideoPlaybackDelegate`    | Extends `PullModePlaybackDelegate` for video. Constructs a `MediaSourceVideo` from negotiated GstCaps. Manages video window rectangle, max resolution, immediate output, syncmode-streaming, and render-frame-step-on-preroll properties.                                                                                                                                                      | `PullModeVideoPlaybackDelegate.cpp`, `PullModeVideoPlaybackDelegate.h`                                  |
| `PullModeAudioPlaybackDelegate`    | Extends `PullModePlaybackDelegate` for audio. Constructs a `MediaSourceAudio` from negotiated GstCaps. Manages volume, mute, low-latency, sync, sync-off, stream-sync-mode, audio-fade, buffering-limit, and use-buffering properties.                                                                                                                                                         | `PullModeAudioPlaybackDelegate.cpp`, `PullModeAudioPlaybackDelegate.h`                                  |
| `PullModeSubtitlePlaybackDelegate` | Extends `PullModePlaybackDelegate` for subtitles. Constructs a `MediaSourceSubtitle` from negotiated GstCaps. Manages text-track-identifier, window-id, and mute properties.                                                                                                                                                                                                                   | `PullModeSubtitlePlaybackDelegate.cpp`, `PullModeSubtitlePlaybackDelegate.h`                            |
| `PushModeAudioPlaybackDelegate`    | Handles push-mode audio (web audio). Owns a `GStreamerWebAudioPlayerClient` and a `ControlBackend`. Responds to GStreamer buffer chain calls by writing audio data to the Rialto web audio pipeline.                                                                                                                                                                                           | `PushModeAudioPlaybackDelegate.cpp`, `PushModeAudioPlaybackDelegate.h`                                  |
| `GStreamerMSEMediaPlayerClient`    | Implements `IMediaPipelineClient`. Receives Rialto server callbacks (position, duration, playback state, need-data, QoS, underflow, flush) and dispatches them as messages on the `MessageQueue` worker thread. Owns the `MediaPlayerClientBackendInterface` and manages the set of `AttachedSource` entries.                                                                                  | `GStreamerMSEMediaPlayerClient.cpp`, `GStreamerMSEMediaPlayerClient.h`                                  |
| `GStreamerWebAudioPlayerClient`    | Implements `IWebAudioPlayerClient`. Opens and closes the Rialto web audio session and feeds PCM audio data by calling `writeBuffer()` on the Rialto web audio backend as incoming GStreamer buffers are queued and drained. Uses a timer to schedule periodic write attempts when the backend buffer is temporarily full.                                                                      | `GStreamerWebAudioPlayerClient.cpp`, `GStreamerWebAudioPlayerClient.h`                                  |
| `MediaPlayerManager`               | Manages the lifecycle of shared `GStreamerMSEMediaPlayerClient` instances, keyed by the top-level `GstBin` parent. Allows multiple sink elements in the same pipeline to share a single Rialto media pipeline session and coordinates control acquisition.                                                                                                                                     | `MediaPlayerManager.cpp`, `MediaPlayerManager.h`                                                        |
| `BufferParser`                     | Abstract base for per-media-type GstBuffer parsing. Extracts timestamp, duration, codec data, display offset, and DRM protection metadata from a `GstSample`, producing an `IMediaPipeline::MediaSegment`. Concrete subclasses: `AudioBufferParser`, `VideoBufferParser`, `SubtitleBufferParser`.                                                                                              | `BufferParser.cpp`, `BufferParser.h`                                                                    |
| `MessageQueue`                     | Thread-safe FIFO queue backed by a dedicated `std::thread`. Supports `postMessage` (asynchronous), `callInEventLoop` (synchronous, caller blocks until processed), and `scheduleInEventLoop` (asynchronous dispatch).                                                                                                                                                                          | `MessageQueue.cpp`, `MessageQueue.h`                                                                    |
| `FlushAndDataSynchronizer`         | Tracks per-source flush and data states. `waitIfRequired()` delays starting a new flush while the source is mid-flush or holding post-flush data; suppression of sample pulling during a server flush is handled separately by the delegate.                                                                                                                                                            | `FlushAndDataSynchronizer.cpp`, `FlushAndDataSynchronizer.h`                                            |
| `ControlBackend`                   | Wraps the Rialto `IControl` interface. Registers the sink as a control client, receives `notifyApplicationState` transitions from the Rialto server, and exposes a `waitForRunning()` method that blocks until the server is active.                                                                                                                                                           | `ControlBackend.h`                                                                                      |
| `GStreamerEMEUtils`                | Reads GStreamer protection metadata from a `GstBuffer` (CENC and WebM encryption formats) and populates a `BufferProtectionMetadata` struct with key ID, IV, subsample map, and cipher mode for downstream attachment to media segments.                                                                                                                                                       | `GStreamerEMEUtils.cpp`, `GStreamerEMEUtils.h`                                                          |
| `GStreamerMSEUtils`                | Utility functions for capabilities setup: translates GstCaps structures into Rialto-native types (`Layout`, `Format`, `StreamFormat`, `SegmentAlignment`, codec data, Dolby Vision profile). Also sets supported caps on element classes using `IMediaPipelineCapabilities`.                                                                                                                   | `GStreamerMSEUtils.cpp`, `GStreamerMSEUtils.h`                                                          |
| `LogToGstHandler`                  | Implements `IClientLogHandler` to redirect Rialto client library internal log messages to the GStreamer debug logging system.                                                                                                                                                                                                                                                                  | `LogToGstHandler.cpp`, `LogToGstHandler.h`                                                              |
| `Timer` / `TimerFactory`           | Provides one-shot and periodic timer functionality used by the web audio client to schedule periodic data writes. Each `Timer` runs its own `std::thread`.                                                                                                                                                                                                                                     | `Timer.cpp`, `Timer.h`                                                                                  |

---

## Component Interactions

rialto-gstreamer communicates upward with the GStreamer pipeline through standard GStreamer pad and element APIs, and downward with the Rialto server through the Rialto Client library interfaces over IPC.

### Interaction Matrix

| Target Component / Layer     | Interaction Purpose                                                                                                                     | Key APIs / Topics                                                                                                                                                                                                                               |
| ---------------------------- | --------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Rialto Client Library**    |                                                                                                                                         |                                                                                                                                                                                                                                                 |
| `IMediaPipeline`             | Load, play, pause, stop, seek, rate change, window set, and source management for MSE playback                                          | `load()`, `play()`, `pause()`, `stop()`, `attachSource()`, `removeSource()`, `allSourcesAttached()`, `addSegment()`, `haveData()`, `flush()`, `setSourcePosition()`, `setPlaybackRate()`, `setVideoWindow()`, `renderFrame()`, `switchSource()` |
| `IMediaPipelineClient`       | Receive server-driven notifications for playback state, position, duration, data requests, QoS, underflow, errors, and flush completion | `notifyPlaybackState()`, `notifyPosition()`, `notifyDuration()`, `notifyNeedMediaData()`, `notifyQos()`, `notifyBufferUnderflow()`, `notifyPlaybackError()`, `notifySourceFlushed()`, `notifyFirstFrameReceived()`, `notifyPlaybackInfo()`      |
| `IWebAudioPlayer`            | Open, write, play, pause, and close web audio sessions                                                                                  | `open()`, `writeBuffer()`, `play()`, `pause()`, `setVolume()`, `getVolume()`                                                                                                                                                                    |
| `IWebAudioPlayerClient`      | Receive server-driven notifications for web audio playback state transitions                                                            | `notifyState()`                                                                                                                                                                                                                                 |
| `IControl`                   | Register as a Rialto control client to receive application state transitions                                                            | `IControl::registerClient()`, `notifyApplicationState()`                                                                                                                                                                                        |
| `IMediaPipelineCapabilities` | Query supported MIME types at sink element class initialization to populate GstCaps; query supported audio-source properties to decide which conditional GObject properties to install                                                     | `getSupportedMimeTypes()`, `getSupportedProperties()`                                                                                                                                                                                                                       |
| **GStreamer Pipeline**       |                                                                                                                                         |                                                                                                                                                                                                                                                 |
| GStreamer upstream           | Receive media buffers pushed via the pad chain function and queue them for on-demand delegate consumption                               | `gst_pad_set_chain_function()`, `handleBuffer()`                                                                                                                                                                                                |
| GStreamer bus                | Post EOS, error, state-change, and QoS messages upstream                                                                                | `gst_element_post_message()`, `gst_message_new_eos()`, `gst_message_new_error()`                                                                                                                                                                |
| GStreamer pad events         | Handle CAPS, SEGMENT, EOS, FLUSH_START, FLUSH_STOP, SEEK events                                                                         | `gst_pad_event_default()`, `GST_EVENT_*`                                                                                                                                                                                                        |

### Events Published

| Event Name                   | GStreamer Signal / Message                                        | Trigger Condition                                                       | Subscriber Components            |
| ---------------------------- | ----------------------------------------------------------------- | ----------------------------------------------------------------------- | -------------------------------- |
| `first-video-frame-callback` | GObject signal on `RialtoGStreamerMSEVideoSink`                   | `notifyFirstFrameReceived` received from Rialto server for video source | GStreamer pipeline / application |
| `first-audio-frame-callback` | GObject signal on `RialtoGStreamerMSEAudioSink`                   | `notifyFirstFrameReceived` received from Rialto server for audio source | GStreamer pipeline / application |
| `buffer-underflow-callback`  | GObject signal on `RialtoMSEBaseSink`                             | `notifyBufferUnderflow` received from Rialto server                     | GStreamer pipeline / application |
| EOS message                  | `GstMessageEOS` on pipeline bus                                   | `notifyPlaybackState(END_OF_STREAM)` from Rialto server                 | GStreamer pipeline               |
| Error message                | `GstMessageError` on pipeline bus                                 | `notifyPlaybackError(DECRYPTION)` from Rialto server                    | GStreamer pipeline               |
| Application message          | `GstMessageApplication` (`HDCPProtectionFailure`) on pipeline bus | `notifyPlaybackError(OUTPUT_PROTECTION)` from Rialto server             | GStreamer pipeline               |
| Media metric message         | `GST_MESSAGE_ELEMENT` (`media-pipeline-metric`) from the sink owning the source | `notifyMetric()` from Rialto server for a subscribed metric type | Application |
| Missed metrics message       | `GST_MESSAGE_ELEMENT` (`media-pipeline-metrics-missed`) from the video sink, or another attached sink | `notifyEventsMissed()` from Rialto server | Application |

### IPC Flow Patterns

**Primary Request / Response Flow:**

GStreamer state transitions, property changes, and playback control operations are forwarded synchronously (via `callInEventLoop`) or asynchronously (via `postMessage`) through the `MessageQueue` worker thread to the Rialto Client library, which serializes them over IPC to the Rialto server.

```mermaid
sequenceDiagram
    participant GSTPipeline as GStreamer Pipeline
    participant Delegate as PullModePlaybackDelegate
    participant MsgQueue as MessageQueue Worker
    participant RialtoBackend as MediaPlayerClientBackend
    participant RialtoServer as Rialto Server

    GSTPipeline->>Delegate: PAUSED_TO_PLAYING state change
    Delegate->>MsgQueue: callInEventLoop(play)
    MsgQueue->>RialtoBackend: play()
    RialtoBackend->>RialtoServer: IPC play request
    RialtoServer-->>RialtoBackend: acknowledged
    RialtoBackend-->>MsgQueue: result
    MsgQueue-->>Delegate: play result
    Delegate-->>GSTPipeline: GST_STATE_CHANGE_ASYNC / SUCCESS
```

**Event Notification Flow:**

Most Rialto server callbacks arrive on the IPC receiver thread, are immediately wrapped as `Message` objects and posted to the backend `MessageQueue`, then processed on the worker thread to update pipeline state or propagate events into the GStreamer pipeline. Two exceptions bypass the queue: `notifyPlaybackInfo` updates a mutex-protected `PlaybackInfo` cache directly, and web audio `notifyState` calls delegate EOS/error/state handlers synchronously on the IPC thread. Data-request notifications (`notifyNeedMediaData`) additionally hand off to a per-source `BufferPuller` queue, which dequeues the buffered sample, parses it, and calls `addSegment` directly from its own thread, before the backend queue sends `haveData` back to the Rialto server.

```mermaid
sequenceDiagram
    participant RialtoServer as Rialto Server IPC Thread
    participant MPClient as GStreamerMSEMediaPlayerClient
    participant MsgQueue as MessageQueue Worker
    participant Delegate as PullModePlaybackDelegate
    participant GSTPipeline as GStreamer Pipeline

    RialtoServer->>MPClient: notifyPlaybackState(END_OF_STREAM)
    MPClient->>MsgQueue: postMessage(PlaybackStateMessage)
    MsgQueue->>MPClient: handle PlaybackStateMessage
    MPClient->>Delegate: handleEos()
    Delegate->>GSTPipeline: gst_element_post_message(EOS)
```

---

## Implementation Details

### Major HAL APIs Integration

Hardware-level operations are handled by the Rialto server. The Rialto Client APIs listed below represent the full southbound API surface of this component.

| Rialto Client API                           | Purpose                                                                                                  | Implementation File                                                         |
| ------------------------------------------- | -------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------- |
| `createMediaPlayerBackend()`                | Creates a media player session on the Rialto server with max video resolution constraints                | `GStreamerMSEMediaPlayerClient.cpp`                                         |
| `load(MSE, url)`                            | Loads the MSE media type into the Rialto pipeline                                                        | `GStreamerMSEMediaPlayerClient.cpp`                                         |
| `attachSource()`                            | Registers an audio, video, or subtitle source with the Rialto pipeline                                   | `PullModePlaybackDelegate.cpp`                                              |
| `removeSource()`                            | Detaches a source from the Rialto pipeline                                                               | `PullModePlaybackDelegate.cpp`                                              |
| `allSourcesAttached()`                      | Signals that all sources have been attached and the pipeline may proceed                                 | `GStreamerMSEMediaPlayerClient.cpp`                                         |
| `addSegment()`                              | Submits a parsed media segment (with timestamps and optional encryption metadata) to the Rialto pipeline | `GStreamerMSEMediaPlayerClient.cpp`                                         |
| `haveData()`                                | Notifies the Rialto server of the data submission status for a given `NeedData` request ID               | `GStreamerMSEMediaPlayerClient.cpp`                                         |
| `play()`                                    | Starts or resumes playback                                                                               | `GStreamerMSEMediaPlayerClient.cpp`                                         |
| `pause()`                                   | Pauses playback                                                                                          | `GStreamerMSEMediaPlayerClient.cpp`                                         |
| `stop()`                                    | Stops playback and resets the pipeline                                                                   | `GStreamerMSEMediaPlayerClient.cpp`                                         |
| `flush()`                                   | Issues a flush for a specific source, with optional time reset                                           | `PullModePlaybackDelegate.cpp`                                              |
| `setSourcePosition()`                       | Sets the decode position for a source after a seek or discontinuity                                      | `PullModePlaybackDelegate.cpp`                                              |
| `setPlaybackRate()`                         | Changes the playback rate for trick-play                                                                 | `PullModePlaybackDelegate.cpp`                                              |
| `setVideoWindow()`                          | Sets the video output rectangle                                                                          | `GStreamerMSEMediaPlayerClient.cpp`                                         |
| `renderFrame()`                             | Requests a single frame render (frame-step on preroll)                                                   | `PullModeVideoPlaybackDelegate.cpp`                                         |
| `setVolume()`                               | Sets audio volume with optional fade duration and ease type                                              | `PullModeAudioPlaybackDelegate.cpp`                                         |
| `getVolume()`                               | Queries current audio volume                                                                             | `PullModeAudioPlaybackDelegate.cpp`                                         |
| `setMute()`                                 | Sets mute state per source                                                                               | `PullModeAudioPlaybackDelegate.cpp`, `PullModeSubtitlePlaybackDelegate.cpp` |
| `getMute()`                                 | Queries mute state                                                                                       | `PullModeAudioPlaybackDelegate.cpp`                                         |
| `setLowLatency()`                           | Enables or disables low-latency audio mode                                                               | `PullModeAudioPlaybackDelegate.cpp`                                         |
| `setSync()` / `setSyncOff()`                | Controls audio/video synchronization behavior                                                            | `PullModeAudioPlaybackDelegate.cpp`                                         |
| `setStreamSyncMode()`                       | Sets the stream synchronization mode per source                                                          | `PullModeAudioPlaybackDelegate.cpp`                                         |
| `setBufferingLimit()` / `setUseBuffering()` | Controls media buffering limits and behavior                                                             | `PullModeAudioPlaybackDelegate.cpp`                                         |
| `setImmediateOutput()`                      | Enables or disables immediate output mode for the video decoder                                          | `PullModeVideoPlaybackDelegate.cpp`                                         |
| `setReportDecodeErrors()`                   | Enables or disables decode error reporting per source                                                    | `PullModeVideoPlaybackDelegate.cpp`                                         |
| `getStats()`                                | Retrieves rendered and dropped frame counts                                                              | `GStreamerMSEMediaPlayerClient.cpp`                                         |
| `getFrameCounters()`                        | Retrieves rendered, dropped and corrupted counters of a video source                                     | `GStreamerMSEMediaPlayerClient.cpp`                                         |
| `setMetricSubscription()`                   | Replaces the set of media metric types the Rialto server delivers for the pipeline                       | `PullModePlaybackDelegate.cpp`                                              |
| `getQueuedFrames()`                         | Retrieves the count of frames queued in the Rialto server pipeline                                       | `GStreamerMSEMediaPlayerClient.cpp`                                         |
| `getDuration()`                             | Queries the current media duration                                                                       | `GStreamerMSEMediaPlayerClient.cpp`                                         |
| `setTextTrackIdentifier()`                  | Sets the text track identifier for subtitle selection                                                    | `PullModeSubtitlePlaybackDelegate.cpp`                                      |
| `setSubtitleOffset()`                       | Sets a timing offset for subtitle streams                                                                | `PullModeSubtitlePlaybackDelegate.cpp`                                      |
| `processAudioGap()`                         | Notifies the server of an audio gap or discontinuity                                                     | `PullModeAudioPlaybackDelegate.cpp`                                         |
| `switchSource()`                            | Switches the codec or format of an attached source without tearing down the pipeline                     | `PullModePlaybackDelegate.cpp`                                              |
| `IControl::registerClient()`                | Registers the sink as a Rialto control client to receive application state notifications                 | `ControlBackend.h`                                                          |

### Key Implementation Logic

- **State / Lifecycle Management**: GStreamer state transition handling is split between the sink element (`change_state` virtual) and the delegate. The delegate's `changeState` method executes the Rialto-side transition (create backend, attach source, play, pause, stop) and returns a `GstStateChangeReturn` value. READY→NULL resets the delegate pointer in the base sink, releasing all Rialto session resources.
  - Sink element state dispatch: `RialtoGStreamerMSEBaseSink.cpp`
  - Rialto session lifecycle: `PullModePlaybackDelegate.cpp`, `GStreamerMSEMediaPlayerClient.cpp`

- **Event Processing**: Most Rialto server callbacks are received on the IPC thread and posted immediately to the backend `MessageQueue` as typed `Message` subclasses (`notifyPlaybackInfo` and web audio `notifyState` are exceptions — they update state or invoke delegate handlers directly on the IPC thread). For data requests, the backend worker thread hands off to the attached source's `BufferPuller`, whose own `IMessageQueue` dequeues the buffered sample, parses it, and calls `addSegment` directly from the `BufferPuller` thread; the outcome is then posted back to the backend queue, which sends `haveData` to the Rialto server. Other server callbacks (playback state, position, QoS, underflow) are processed serially on the backend worker thread without this additional hop.
  - Message dispatch: `GStreamerMSEMediaPlayerClient.cpp`
  - Buffer pull: `GStreamerMSEMediaPlayerClient.cpp` (`BufferPuller`, `PullBufferMessage`)

- **Error Handling Strategy**: Errors from Rialto Client calls are checked for boolean return values; failures are logged with `GST_ERROR_OBJECT` and, where appropriate, propagated as `GstMessageError` on the pipeline bus. `notifyPlaybackError(DECRYPTION)` maps to a GStreamer error message via the delegate's `handleError()`, while `notifyPlaybackError(OUTPUT_PROTECTION)` instead posts an `HDCPProtectionFailure` application message rather than failing the pipeline. State change failures return `GST_STATE_CHANGE_FAILURE`, which propagates naturally through the GStreamer state machine.

- **Logging & Diagnostics**: GStreamer debug categories are defined per sink element (`RialtoMSEBaseSinkDebug`, `RialtoMSEVideoSinkDebug`, `RialtoMSEAudioSinkDebug`, `RialtoMSESubtitleSinkDebug`, `RialtoWebAudioSinkDebug`) and a shared plugin-level category (`rialtoGStreamerCat`). Internal Rialto client library logs are redirected to the GStreamer logging system via `LogToGstHandler`. When the `USE_ETHANLOG` compile-time flag is set, `LogToGstHandler` selects a mutually exclusive set of logging macros, so logs go to EthanLog instead of the GStreamer log — not to both.

---

## Configuration

### Key Configuration Parameters

| Parameter            | Type                                    | Default           | Description                                                                                                                                                                                                                                               |
| -------------------- | --------------------------------------- | ----------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `RIALTO_SOCKET_PATH` | Environment variable (string)           | Not set           | Path to the Rialto server Unix domain socket. When set, sink elements are registered at maximum GStreamer rank, enabling automatic selection by the pipeline. When unset, the rank resolves to zero and `rialto_mse_sinks_init` returns before any `gst_element_register` call — the sink factories are not registered at all, so even explicit `gst_element_factory_make` fails. |
| `RIALTO_SINKS_RANK`  | Environment variable (unsigned integer) | Not set           | Overrides the GStreamer element rank for all registered sink elements. When set, takes precedence over the rank derived from `RIALTO_SOCKET_PATH`.                                                                                                        |
| `max-video-width`    | GObject property (uint)                 | 3840              | Maximum video width constraint passed to the Rialto server when creating the media player backend.                                                                                                                                                        |
| `max-video-height`   | GObject property (uint)                 | 2160              | Maximum video height constraint passed to the Rialto server when creating the media player backend.                                                                                                                                                       |
| `volume`             | GObject property (double)               | 1.0               | Audio volume level (0.0–1.0) forwarded to the Rialto audio pipeline.                                                                                                                                                                                      |
| `mute`               | GObject property (bool)                 | false             | Audio or subtitle mute state forwarded to the Rialto pipeline.                                                                                                                                                                                            |
| `low-latency`        | GObject property (bool)                 | false             | Audio sink. Enables low-latency audio mode for gaming (no audio decoding, no A/V sync). Installed only when reported by `IMediaPipelineCapabilities::getSupportedProperties(AUDIO)`.                                                                                                                                                                                             |
| `limit-buffering-ms` | GObject property (uint)                 | 750               | Audio sink. Buffering threshold in milliseconds (0-20000) used when `use-buffering` is enabled; forwarded via `setBufferingLimit()`. Installed unconditionally — its presence is not evidence of backend capability support.                                                                                                                                                                                |
| `use-buffering`      | GObject property (bool)                 | false             | Audio sink. Emit `GST_MESSAGE_BUFFERING` based on low-/high-percent thresholds; forwarded via `setUseBuffering()`.                                                                                                                                                                                                                 |
| `immediate-output`   | GObject property (bool)                 | true              | Enables immediate output mode for the video decoder in the Rialto server.                                                                                                                                                                                 |
| `syncmode-streaming` | GObject property (bool)                 | false             | Enables streaming sync mode for video playback.                                                                                                                                                                                                           |
| `rectangle`          | GObject property (string)               | `"0,0,1920,1080"` | Video display rectangle as `"x,y,width,height"` forwarded to the Rialto server.                                                                                                                                                                           |
| `frame-step-on-preroll` | GObject property (bool)            | false             | Video sink. Allow frame stepping on preroll into pause (issues `renderFrame()`).
| `report-decode-errors` | GObject property (bool, write-only) | false             | Video sink. Enable reporting of decode errors via `setReportDecodeErrors()`.
| `show-video-window`   | GObject property (bool, write-only) | true               | Video sink. Make the video window visible (`true`) or hidden (`false`).
| `queued-frames`       | GObject property (uint, read-only)  | 0                  | Video sink. Number of frames currently queued in the decoder.
| `is-master`           | GObject property (bool, read-only)  | true               | Video sink. Whether the platform is video master.
| `video_pts`           | GObject property (int64, read-only) | 0                  | Video sink. Current video PTS value.
| `maxVideoWidth` / `maxVideoHeight` | GObject property (uint) | 3840 / 2160         | Video sink. Deprecated aliases for `max-video-width` / `max-video-height`.
| `web-audio`           | GObject property (bool)             | false              | Audio sink. Enable web-audio mode; must be set before NULL→READY. When set, the sink uses `PushModeAudioPlaybackDelegate` instead of `PullModeAudioPlaybackDelegate`.
| `async`               | GObject property (bool)             | false              | Audio and subtitle sinks. Asynchronous mode.
| `gap`                 | GObject property (GstStructure, write-only) | —          | Audio sink. Signals an audio gap/discontinuity via `processAudioGap()`.
| `audio-fade`          | GObject property (string, write-only) | `"100,0,L"`      | Audio sink. Start audio fade: `vol[0-100],duration ms,easetype`.
| `fade-volume`         | GObject property (uint, read-only)  | 0                  | Audio sink. Current fade volume (0-100).
| `sync` / `sync-off`   | GObject property (bool)             | false              | Audio sink. Clock sync / free-running audio. Installed only when reported by `IMediaPipelineCapabilities::getSupportedProperties(AUDIO)`. `sync-off` must be set before PLAYING.
| `stream-sync-mode`    | GObject property (int)              | 0                  | Audio sink. Stream sync mode (0 = no frame sync, 1 = next frame sync after decode). Capability-gated like `sync`/`sync-off`.
| `single-path-stream`  | GObject property (bool)             | false              | Base sink. Single-path stream flag forwarded to the Rialto source configuration.
| `streams-number`      | GObject property (int)              | 1                  | Base sink. Deprecated fallback stream count used when coordinating `allSourcesAttached()`. The `streams-info` context takes precedence, followed by parent (playbin) properties; this property only supplies counts when neither is available, so setting it does not override explicit stream information.
| `has-drm`             | GObject property (bool)             | true               | Base sink. Whether the source stream carries DRM; feeds the `hasDrm` flag in the media source configuration.
| `stats`               | GObject property (pointer, read-only) | —                | Base sink. Pointer to a `GstStructure` with `rendered` and `dropped` (`guint64`). The video sink also adds `corrupted` when the server returns all three counters through `getFrameCounters()`; otherwise it falls back to `getStats()`.
| `media-metric-subscription` | GObject property (string)     | `""`             | Base sink. Comma separated media metric types to deliver for the whole pipeline: `video-frame-drop`, `audio-frame-drop`, `video-frame-repeat`, `audio-gap`, `video-decode-error`, `audio-decode-error`, `buffer-underflow`. Each set replaces the previous subscription; empty disables delivery. Set before the pipeline exists, it is applied on READY→PAUSED. See [Media metrics](#media-metrics).
| `enable-last-sample` / `last-sample` | GObject property (bool / boxed) | false / —   | Base sink. Enable the `last-sample` property; `last-sample` returns the most recent sample received by the sink.
| `text-track-identifier` | GObject property (string)         | not set (valid: CC[1-4], TEXT[1-4], SERVICE[1-64]) | Subtitle sink. Text track identifier forwarded to the Rialto pipeline.
| `window-id`           | GObject property (uint)             | 0                  | Subtitle sink. Window ID placeholder (0-256).
| `ts-offset`           | GObject property (int64)            | 0                  | Web audio sink. Timestamp offset; marked not supported - the web audio sink does not require source synchronisation.

### Runtime Configuration

Sink rank can be configured at runtime before the GStreamer pipeline is instantiated by setting environment variables:

```bash
# Enable Rialto sinks with automatic maximum rank
export RIALTO_SOCKET_PATH=/tmp/rialto-0

# Override sink rank explicitly
export RIALTO_SINKS_RANK=268435455
```

### Configuration Persistence

Properties set before the NULL→READY transition are queued in `m_queuedProperties` and applied to the delegate once it is initialized.

## Media metrics

Rialto V1.1 media metrics (SoC Media Metrics GStreamer Specification v0.7) are delivered to the application as element messages.

1. The application sets `media-metric-subscription` on any Rialto sink of the pipeline. The sink calls `IMediaPipeline::setMetricSubscription()`; nothing is delivered until then.
2. For each `notifyMetric()` the sink that owns `MetricEvent::sourceId` posts a `GST_MESSAGE_ELEMENT` named `media-pipeline-metric`. The fields use the v0.7 names and values, so applications parse it like a SoC producer message:

| Field | GType | Presence | Meaning |
|---|---|---|---|
| `metric` | `G_TYPE_UINT` | always | 1 video frame drop, 2 audio frame drop, 3 video decode error, 4 audio decode error, 5 video frame repeat, 6 audio gap, 7 buffer underflow |
| `observation-kind` | `G_TYPE_UINT` | always | 1 occurrence, 2 episode started, 3 episode resolved |
| `pts-ns` | `G_TYPE_INT64` | optional | Presentation timestamp; 0 is valid |
| `duration-ns` | `G_TYPE_UINT64` | resolved episodes | Monotonic episode duration measured by the producer |
| `count` | `G_TYPE_UINT64` | resolved repeat and audio gap | Number of repeated frames or gap units |
| `source-id` | `G_TYPE_INT` | always | Rialto source id |
| `observed-at-monotonic-ns` | `G_TYPE_UINT64` | always | Server acceptance time, monotonic clock |
| `observed-at-unix-ns` | `G_TYPE_INT64` | always | Server acceptance time, wall clock |

3. For `notifyEventsMissed()` a `media-pipeline-metrics-missed` element message with `count` (`G_TYPE_UINT`) is posted before the following metrics. Occurrence totals for that interval are incomplete; open episodes are not reset.

`report-decode-errors` is unchanged and still controls `setReportDecodeErrors()`.
