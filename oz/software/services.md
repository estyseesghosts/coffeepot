# Graphics and system services

Switch 2 keeps Horizon's service model: a program talks to a system service over an IPC session, and the service decides whether the caller is allowed. What changed on Switch 2 is the set of services and the graphics stack behind them. This page covers the Switch 2 service landscape and the graphics services in particular.

## Where it fits

The kernel, the process model and the system calls are on the [system and security page](system.md). The graphics interface that sits on top of these services is on the [GPU page](../hardware/gpu.md). The service landscape is the third layer: it is what a homebrew program actually tries to open.

## How it works

Most Horizon services are unchanged in shape from the original Switch. A program opens a session to a service by name, then issues commands with a handle and a request structure; the service enforces the caller's capabilities from the requesting process's [NPDM](../glossary.md#npdm).

Switch 2 adds services that did not exist on the original Switch, and SwitchBrew documents them separately from the original Switch's service list. Several are exclusive to Switch 2.

## Technical details

### Services documented as exclusive to Switch 2

[Reverse-engineered] SwitchBrew has separate pages for a group of services that exist only on Switch 2. The ones relevant to graphics and hardware are:

| Service | Purpose |
| ------- | ------- |
| `vi2:v`, `vi2:m1`, `vi2:m2`, `vi2:nv` | Display and video input on Switch 2; the successor to the original Switch's display service |
| `vic:u` | Video input and related camera handling |
| `syncpt:s`, `syncpt:v` | Synchronization points for the system and for video work |
| `gsv` | The graphics service, described below |
| `adraw` | An additional drawing or addressable render path |
| `t23x` firewalls | Per-GPU firewall rules controlling which processing units may reach the GPU |
| `bch`, `bct`, `mb1-bct` | Boot chain hash and boot configuration tables, read from the boot chain |
| `psc-bl` | The PSC-BL bootloader stage |
| `da9092` | An audio codec component |
| `max77851` | The power management and audio amplifier device |
| `camera`, `capture2` | Camera and screen capture services |
| `dt`, `ts`, `bpmp` | Device tree, thermal and power management services |
| `imf`, `lrp`, `speech` | Imaging, lighting and speech services |
| `pro controller`, `setting applet`, `system settings` | Controller, and system settings applets |

The list is a documentation boundary, not a capability statement. A service being documented does not mean a homebrew process may open it: service access control is enforced per process, and an ordinary game or sandbox program will be refused most of these.

### The graphics service

[Reverse-engineered] The `gsv` service is the graphics front end. It is a TIPC interface, so its command numbers are offset by 16, and it multiplexes several interfaces into one session acquired through the session manager.

Its documented commands fall into groups:

| Group | Commands |
| ----- | -------- |
| User session | `BootWithTransferMemory`, `BootWithRemoteMemory`, `SetCurrentProcessHandle`, `CreateContext`, `CreateSyncptContext`, `InitializeSyncptContext`, `CreateDisplayContext`, `DestroyDisplayContext`, `InitializeDisplayContext`, `CreateGpuContext`, `InitializeGpuContext`, `DestroyGpuContext`, `GetImageInfo` |
| GPU sessions | `CreateGpuSessionWithTransferMemory`, `CreateGpuSessionWithRemoteMemory` |
| Video | `CreateVideoDecoderContext`, `InitializeVideoDecoderContext`, `DestroyVideoDecoderContext`, `InitializeVideoDecoderContextForSystem`, `InitializeVideoDecoderContext2`, `CreateVideoEncoderContext`, `InitializeVideoEncoderContext`, `DestroyVideoEncoderContext`, `InitializeVideoEncoderContextForSystem` |
| Drawing | `CreateAdrawContext`, `InitializeAdrawContext`, `GetAdrawFrameBufferHandle`, `GetAdrawImageHandle` |
| Driver | `CreateNvDrv2UserSessionWithTransferMemory`, `CreateNvDrv2UserSessionWithRemoteMemory`, `GetNvDrv2UserTransferMemoryUsage` |
| Synchronization | `SyncptSession` with `Ping`; `GetEventIdRange`, `CreateEvent`; `RequestKUserWorkerSession`, `GetKUserWorkerSession` |
| Debug | `CreateDebugSyncDevice`, `DestroyDebugSyncDevice`, `CreateDebugSyncCounter`, `DestroyDebugSyncCounter`, `ReportDebugSyncCounterValue` |

The `KUserWorkerSession` exposes `CreateGpuMemory` and `DestroyGpuMemory`, which is where GPU-visible memory is allocated. The `DisplaySession` and `VideoDecoderCreatorSession` and `VideoEncoderCreatorSession` groups cover presentation and the hardware video paths.

[Reverse-engineered] Eglantine's Switch 2 notes describe separate interfaces for `gsv`, `gpu` and `vi2`, and identify work on GPU memory registration, synchronization points and Ampere command processing. They also record the interface-level detail that GPU memory carries attributes, and that the switch's GPU is shared with the display path, so synchronization points have to order work across both.

> Unknown: The `gsv` service is documented by community reverse engineering, and its interface version is tied to the SDK build that was examined. A command number from one SDK build is not evidence about another.

### GameChat and the camera path

[Documented] GameChat is the Switch 2's system-level voice, video and screen-sharing feature, described on the [devices page](../hardware/devices.md). It is not a service a game opens directly; it is built out of several.

[Reported] Eglantine's notes show the pieces a retail title touches while GameChat is present. The `chat:u` service is registered from the friends process, and a title that runs `nn::chat::Initialize` at boot waits for it. GameChat's own module, program ID `2162-0001`, initializes through `nn::chat::gsv::Initialize`, which sends the `gsv` command pair `1201` then `1202`; the second must return a session, and on that session a TIPC command returns an identifier that must not exceed `0x3ff`.

[Reported] The same notes record the camera path: `nn::camera::sdk::CreateCameraContext` sends `gsv` `601` then `602`, and the second must return the camera context session. GameChat's local camera video storage is three 384 × 216 images in format `0x38`, and their buffer size is negotiated with `gsv` `1291` and `1211`.

These are observations from an emulator implementation, which is a primary source for what the developers observed and implemented but not proof of hardware behaviour. A program that runs only because the emulator stubs `gsv 1201` has not established what the real service returns.

## Homebrew notes

Treat "the service exists" and "my process may use it" as separate questions, and log the answer to both. When a session request fails, record the service name, the process type and the returned result before changing anything: an unavailable service, a denied capability and a changed protocol all present as a failed call.

GPU memory is the usual first thing a native program reaches for and the usual first thing that fails. Establish allocation, visibility and synchronization from the runtime you are actually in, using its own examples, before writing a renderer; see [graphics and performance](../engineering/graphics-and-performance.md).

## Sources

- [SwitchBrew VI2 services](https://switchbrew.org/wiki/Switch_2:_VI2_services) — the services SwitchBrew lists as exclusive to Switch 2.
- [SwitchBrew GSV services](https://switchbrew.org/wiki/Switch_2:_GSV_services) — the graphics service interface and its command groups.
- [SwitchBrew T23X firewalls](https://switchbrew.org/wiki/Switch_2:_T23X_Firewalls) — per-GPU firewall rules.
- [SwitchBrew services API](https://switchbrew.org/wiki/Services_API) — how Horizon services are opened and called.
- [Nintendo GameChat guide and FAQ](https://en-americas-support.nintendo.com/app/answers/detail/a_id/68390) — the user-visible behaviour of the chat, camera and screen-share services.
- [Eglantine NX2 notes](https://github.com/Eglantine-Community/Eglantine/blob/nx2/NX2_NOTES.md) — observed `gsv`, `gpu` and `vi2` interactions and GPU memory behaviour.
