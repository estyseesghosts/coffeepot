# Local wireless between consoles (UDS)

Two 3DS consoles can talk to each other directly over Wi-Fi without a router. This is **UDS** ("user datagram service"; the system service is `nwm::UDS`), the mechanism behind local multiplayer and Download Play. One console **hosts** a small network, others **scan** for it and join, and everyone exchanges short **data frames** addressed to a node or to all. It is not a TCP socket, and nothing here is an internet connection: for that see [networking](networking.md). This page is a short, practical reference built from libctru's `uds.h`: the lifecycle, the roles, how to send and receive, which limits the API states, and what you must design yourself.

UDS is in libctru for every model. Nothing in the header or source distinguishes an Old 3DS from a New 3DS; whether their radios behave identically in practice is untested here.

Background: [CTR services](../software/services.md), [networking](networking.md) (internet sockets, HTTP), [application lifecycle](application-lifecycle.md).

## Before you start

- Checked against libctru 2.7.1 source (`services/uds.h`, `source/services/uds.c`) and the 3ds-examples `network/uds` program ([source versions](source-versions.md)). The example below compiles; **it has not been run on any console**, and it needs two consoles to mean anything.
- This is not DS Wireless: using UDS says nothing about compatibility with Nintendo DS local-wireless protocols.
- Use your own `wlancommID` (a 32-bit identifier that separates your application's networks from every other application's) and passphrase; the values in the example are placeholders.

## The model

| Role | What it does |
| ---- | ------------ |
| Host | `udsCreateNetwork`: broadcasts a beacon (the network name, max nodes, up to 200 bytes of application data) and accepts clients. Node id 1 |
| Client | `udsScanBeacons` to list networks for your `wlancommID`, then `udsConnectNetwork(..., UDSCONTYPE_Client, ...)`. Gets the next node id |
| Spectator | `UDSCONTYPE_Spectator`: receives data frames but may not send |
| Node id | Identifies a console in the network; `UDS_HOST_NETWORKNODEID` = 1, `UDS_BROADCAST_NETWORKNODEID` = 0xFFFF (all nodes). At most `UDS_MAXNODES` = 16 |
| Data channel | An 8-bit tag (non-zero) on each frame; a bind receives only frames with its channel |
| Bind | A receive queue for one (source node, channel) pair; `udsPullPacket` reads from it |

## Lifecycle and ownership

1. `udsInit(sharedmem_size, username)`: `sharedmem_size` is a multiple of 0x1000 and must be at least slightly larger than the total of all your bind receive buffers (3ds-examples use 0x3000). `username` is an optional UTF-8 name of up to 10 characters; if NULL the system's name is used. Under the hood libctru first starts the **NDM** service and calls `NDMU_EnterExclusiveState(NDM_EXCLUSIVE_STATE_LOCAL_COMMUNICATIONS)`, then opens `nwm::UDS` [documented: `uds.c`]. NDM is the system's network daemon manager: it has exclusive states for infrastructure, local communications, StreetPass and StreetPass data, and controls the background daemons (StreetPass/CEC, SpotPass/BOSS, system updates/NIM, friends) [documented: `ndm.h`]. What it actually suspends while in the local-communications state is not documented in libctru; presumably other network use such as background StreetPass and SpotPass `[hypothesis]` (not tested). `udsExit` leaves the state again, so a crash or early return that skips `udsExit` can leave the daemon in that state until the system resets it (not tested).
2. Host: `udsGenerateDefaultNetworkStruct(&ns, wlancommID, id8, maxNodes)`, then `udsCreateNetwork(&ns, passphrase, size, &bind, dataChannel, recvBufSize)`. Optional `udsSetApplicationData(buf, size)` (any time after creation) puts data in the beacon.
3. Client: `udsScanBeacons(buf, maxsize, &networks, &count, wlancommID, id8, NULL, false)` fills a **`malloc`ed array you must `free`**, then `udsConnectNetwork(&networks[i].network, passphrase, size, &bind, UDS_BROADCAST_NETWORKNODEID, UDSCONTYPE_Client, dataChannel, recvBufSize)`. Scan again if the count is 0. `udsGetNetworkStructApplicationData` reads a scanned network's application data; `udsGetNodeInfoUsername` the host's name.
4. Both: exchange frames (below), watch the connection status, then clean up in this order: `udsUnbind(&bind)`, `udsDestroyNetwork()` (host) or `udsDisconnectNetwork()` (client), `udsExit()`. `udsExit` also frees the shared memory it allocated.

`udsCreateNetwork` and `udsConnectNetwork` both create the first bind for you if you pass a non-NULL context, and undo it themselves on failure [documented: `uds.c`]. The passphrase is a raw byte buffer: the content and length are your choice, but both sides must use the same bytes (the example includes the terminating NUL in its length).

## Sending and receiving

- `udsSendTo(destNode, channel, flags, buf, size)`: `size` at most `UDS_DATAFRAME_MAXSIZE` = 0x5C6 (1478) bytes. `UDS_SENDFLAG_Default` for normal frames. Spectators cannot send. The header notes that some errors from `udsSendTo` can be ignored and the frame is not sent when that happens; result `0xC86113F0` is the one its `UDS_CHECK_SENDTO_FATALERROR` macro treats as not fatal. (The macro compares a signed `Result` with an unsigned constant and warns under `-Wextra`; the example writes the test out.)
- `udsPullPacket(&bind, buf, size, &actualSize, &srcNode)`: non-blocking; `actualSize` is 0 and `srcNode` 0 when nothing is waiting. Frames from a node that has just disconnected are still returned until the bind's buffer holds none of its frames [documented: `uds.h`].
- Or wait: `udsWaitDataAvailable(&bind, nextEvent, wait)` blocks on (or checks) the bind's event.
- `udsBind` takes `recv_buffer_size >= 0x5F4`; `UDS_DEFAULT_RECVBUFSIZE` = 0x2E30. The receive buffers live in the shared memory you gave `udsInit`; too small a buffer drops frames (how it signals that is not documented).

Delivery guarantees are not stated anywhere in the header. Treat the link as unreliable and unordered until you have measured it: add a sequence number to your frames, detect duplicates and gaps, and do not assume frames from different nodes arrive in the same order on every console. A reasonable minimal application header is `version | type | sequence | tick | length | payload` (a design suggestion, not something libctru provides; `[hypothesis]` that it is sufficient, to be tested with real packet loss).

## Connection state

`udsGetConnectionStatus(&st)` returns `status`, your node id, `total_nodes`, `max_nodes` and `node_bitmask` (bit 0 = node 1, the host, bit 1 = node 2, and so on). `udsWaitConnectionStatusEvent(nextEvent, wait)` signals when that data changed, which is how you learn that a node joined or left (the 3ds-examples print the whole struct). `udsGetNodeInformation(nodeId, &info)` gives a node's name. The host can close the door with `udsSetNewConnectionsBlocked`, remove nodes with `udsEjectClient` / `udsEjectSpectator`, and allow spectators again with `udsAllowSpectators`.

## Complete example: host or join, send a counter

On one console press Y to host a two-node network; on the other press X to scan and join. Press A to send an incrementing counter to the other console, whose last received value is shown. START leaves cleanly.

```c
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Two-console demo. Y: host a 2-node network. X: scan for it and join as a client.
// A: send a counter to the other console. START: leave and exit.

#define WLANCOMM_ID   0x00C0FFEEu          // pick your own; separates your app's networks from others
#define DATA_CHANNEL  1                    // must be non-zero (uds.h)
#define MAX_NODES     2
#define SCAN_BUF_SIZE 0x4000u              // value used by 3ds-examples network/uds
static const char passphrase[] = "coffeepot-demo-passphrase";

typedef enum { ROLE_NONE, ROLE_HOST, ROLE_CLIENT } Role;

int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	// 0x3000 is the value 3ds-examples uses: >= total recv buffer size, page aligned.
	Result rc = udsInit(0x3000, NULL);
	if (R_FAILED(rc)) {
		printf("udsInit failed: %08lX\n", (unsigned long)rc);
		gfxExit();
		return 1;
	}

	Role role = ROLE_NONE;
	udsBindContext bind;
	u32 counter = 0;
	printf("Y = host, X = join, A = send, START = quit\n");

	while (aptMainLoop()) {
		hidScanInput();
		u32 down = hidKeysDown();
		if (down & KEY_START) break;

		if (role == ROLE_NONE && (down & KEY_Y)) {
			udsNetworkStruct ns;
			udsGenerateDefaultNetworkStruct(&ns, WLANCOMM_ID, 0, MAX_NODES);
			rc = udsCreateNetwork(&ns, passphrase, sizeof passphrase, &bind, DATA_CHANNEL, UDS_DEFAULT_RECVBUFSIZE);
			printf("create: %08lX\n", (unsigned long)rc);
			if (R_SUCCEEDED(rc)) role = ROLE_HOST;
		} else if (role == ROLE_NONE && (down & KEY_X)) {
			void *scanBuf = malloc(SCAN_BUF_SIZE);
			udsNetworkScanInfo *nets = NULL;
			size_t total = 0;
			rc = scanBuf ? udsScanBeacons(scanBuf, SCAN_BUF_SIZE, &nets, &total, WLANCOMM_ID, 0, NULL, false) : -1;
			printf("scan: %08lX, %u network(s)\n", (unsigned long)rc, (unsigned)total);
			if (R_SUCCEEDED(rc) && total > 0) {
				rc = udsConnectNetwork(&nets[0].network, passphrase, sizeof passphrase, &bind,
				                       UDS_BROADCAST_NETWORKNODEID, UDSCONTYPE_Client,
				                       DATA_CHANNEL, UDS_DEFAULT_RECVBUFSIZE);
				printf("connect: %08lX\n", (unsigned long)rc);
				if (R_SUCCEEDED(rc)) role = ROLE_CLIENT;
			}
			free(nets);        // allocated by udsScanBeacons (uds.h)
			free(scanBuf);
		}

		if (role != ROLE_NONE) {
			if (down & KEY_A) {
				counter++;
				rc = udsSendTo(UDS_BROADCAST_NETWORKNODEID, DATA_CHANNEL, UDS_SENDFLAG_Default, &counter, sizeof counter);
				if (R_FAILED(rc) && (u32)rc != 0xC86113F0u)   // the check UDS_CHECK_SENDTO_FATALERROR does, without its sign-compare warning
				 printf("send: %08lX\n", (unsigned long)rc);
			}
			u8 frame[UDS_DATAFRAME_MAXSIZE];
			size_t got = 0;
			u16 from = 0;
			rc = udsPullPacket(&bind, frame, sizeof frame, &got, &from);   // got == 0: nothing pending
			if (R_SUCCEEDED(rc) && got >= sizeof(u32)) {
				u32 v;
				memcpy(&v, frame, sizeof v);
				printf("\x1b[10;1Hfrom node %u: %lu   \n", from, (unsigned long)v);
			}
		}
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}

	if (role != ROLE_NONE) {
		udsUnbind(&bind);
		if (role == ROLE_HOST) udsDestroyNetwork(); else udsDisconnectNetwork();
	}
	udsExit();
	gfxExit();
	return 0;
}
```

Expected: after Y on one console and X on the other, `connect: 00000000` on the joiner, and A on either console updates the other's `from node N` line. This is the behaviour the API describes; no run was made.

## Sleep, HOME and failure

A console that sleeps or goes to HOME while hosting or joined may drop out of the network; the header and source say nothing about it (> Unknown). Design for it: handle the connection-status event, treat a lost host as the end of the session, and and prefer to leave and re-create the session after waking rather than assume the old one survived (resuming it is untested). Do the teardown from the main loop, not from inside an APT hook ([application lifecycle](application-lifecycle.md)).

## Technical details

| Item | Value | Source |
| ---- | ----- | ------ |
| Service | `nwm::UDS` (plus `ndm:u` for the exclusive state) | `uds.c` |
| Max nodes / frame / appdata | 16 / 0x5C6 bytes / 0xC8 bytes | `uds.h` |
| Default receive buffer | 0x2E30 (minimum 0x5F4) | `uds.h` |
| Beacon OUI | `001F32`, type `0x15` | `uds.h` (`udsNetworkStruct`) |
| Network attributes | `UDSNETATTR_DisableConnectSpectators` (bit 0), `_DisableConnectClients` (bit 1), `_x4` (bit 2, unknown), `_Default` (bit 15, unknown) | `uds.h` |
| Send flags | `UDS_SENDFLAG_Default` (bit 0, unknown meaning), `UDS_SENDFLAG_Broadcast` (bit 1: send via the broadcast destination MAC even for a unicast node id) | `uds.h` |
| Scan | `udsScanBeacons` allocates `sizeof(udsNetworkScanInfo) * count`; the `connected` argument must be true if you are already on a network | `uds.c`, `uds.h` |
| Shared memory | allocated and freed by libctru (`udsExit` calls `free`) | `uds.c` |

`udsNetworkStruct` is stored big-endian per the header comment; use the accessor functions rather than reading it directly.

## Pitfalls

- **Forgetting to free the `udsScanBeacons` array.** It is `malloc`ed by libctru.
- **Wrong cleanup order or skipping `udsUnbind`.** Symptom: the next `udsInit` or bind fails or leaks. Fix: unbind, leave, exit.
- **Using data channel 0.** The header requires a non-zero channel.
- **Sending a frame larger than 0x5C6 bytes.** Symptom: send fails. Fix: split it; add a length and sequence number.
- **Assuming reliable, ordered delivery.** Add your own sequence numbers and resync messages.
- **Letting a UDS call block the main loop.** `udsScanBeacons` and connect/create can take a while; do them from a worker thread or show progress ([threading](threading.md)).
- **Using the placeholder `wlancommID`.** Two unrelated programs with the same ID will see each other's networks. Choose a unique value.

## Open questions

- Latency distribution, packet loss, maximum stable packet rate, reconnect behaviour and throughput, for Old 3DS pairs, New 3DS pairs and mixed pairs; the experiment that would settle them is a ping-pong program that stamps each frame with a tick and a sequence number, run at several distances.
- What the system suspends during the NDM exclusive state, and how long `udsInit` takes.
- Behaviour across sleep and HOME while hosting or joined.
- How a receive-buffer overflow is reported.
- Whether the beacon "application data" of 200 bytes is enough for a game's lobby or whether a spectator-style handshake (as Download Play does) is needed.

## Verification status

| Item | Status |
| ---- | ------ |
| Example | Builds with devkitARM r68-1 (GCC 16.1.0), libctru 2.7.1-1, `-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -O2 -Wall -Wextra`, no warnings; linked with `3dsx.specs` |
| Old 3DS (two consoles) | not run |
| New 3DS (two consoles) | not run |
| Mixed pair | not run |
| Emulator | not run |

## Sources

- [libctru `uds.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/uds.h) and [`uds.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/uds.c) — structures, limits, init, ownership, scan/bind behaviour.
- [3ds-examples `network/uds`](https://github.com/devkitPro/3ds-examples/blob/be2001fee08cf8ec7d3366e095e83f6f75da8942/network/uds/source/uds.c) — scan, host, connect, send, receive flow.
- [3dbrew: NWM Services](https://www.3dbrew.org/wiki/NWM_Services) — referenced by the header for protocol detail; not re-read for this page.
