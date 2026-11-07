# In-Game Metrics

The client shows live measurements in the bottom-left corner of the screen: frame rate, process resources, network
traffic and lag. They answer two needs of the project: benchmarking the engine, and giving real numbers about bandwidth
and responsiveness.

## Architecture

Whoever measures a value does not know how it is shown, and the other way around. Everything goes through one registry
resource, the `metrics::MetricsBoard`:

```text
metrics::MetricsPlugin ─────────── FPS, Frame, Entities, CPU, RAM ─┐
ecs::net::NetworkMetricsPlugin ─── Down, Up, Packets, RTT, Missed ─┼─► MetricsBoard ─► metrics::MetricsOverlayPlugin
The game (PlayerControlPlugin) ─── Position ───────────────────────┘
```

| Target | Content | Depends on |
|---|---|---|
| `engine_metrics` | `MetricsBoard`, `RollingStats`, `ProcessUsage`, `MetricsPlugin` | `engine_core` |
| `engine_network` | `NetworkMetricsPlugin`, `ReplicationStats`, `TransportStats` | `engine_metrics` |
| `engine_metrics_overlay` | `MetricsOverlayPlugin` | `engine_metrics`, `engine_ui`, `engine_hierarchy` |

No target above contains graphics-library code: the overlay draws with the `ui` components, through
`engine::IScreenDrawService`. This is why it is tested without a window, and why a headless program can collect metrics
too.

Producers refresh their lines every `metrics::REFRESH_PERIOD` (0.5 s) of real time. The overlay only rewrites its text
when `MetricsBoard::version()` changes, so most frames allocate nothing.

## What each line means

| Line | Meaning | Yellow | Red |
|---|---|---|---|
| `FPS` | 1 / average frame time, over the last 120 frames | below 55 | below 30 |
| `Frame` | Average and longest frame time, over the last 120 frames | as `FPS` | as `FPS` |
| `Entities` | Entities alive in the registry, HUD elements included | | |
| `CPU` | CPU time of the process over real time, like `top`: 100 % is one full core | | |
| `RAM` | Physical memory held by the process right now | | |
| `Down`, `Up` | Bytes per second received and sent by the transport (1 KB = 1000 bytes) | | |
| `Packets` | TCP frames and datagrams per second, in each direction | | |
| `RTT` | Smoothed time for an input to be acknowledged by the server | above 100 ms | above 250 ms |
| `Missed` | Share of the server steps with no snapshot, over the last period | above 1 % | above 10 % |
| `Position` | World position of the local ship; added by the game, once in game | | |

The bar above the lines shows the average frame time, and is full at 33 ms (30 FPS).

The `FPS` warning starts at 55 and not 60, because a game capped at 60 FPS averages 59.x and would always be yellow.

### Traffic

`Down` and `Up` count the transport framing (TCP frame header, datagram header), but not the IP, TCP and UDP headers, so
what crosses the wire is a little more. Keepalives and handshakes are included.

### RTT

The protocol has no ping message, and needs none. The client notes when it sends each input, and every snapshot carries
`last_processed_input`, the last input the server applied. The sample is the time between the two, smoothed with a
weight of 0.1 for each new sample.

This is the responsiveness the player feels, not a pure network ping: it also includes the wait for the next server step
and for the next client frame. **It never reaches zero:** expect about 33 ms on loopback for a client running at 60 FPS.
A lost input is sent again with the next ones, so loss raises the value as well.

It stays at `--` until the first input is acknowledged, and keeps its last value if the connection stops answering.

### Missed

The tick of each snapshot is compared to the previous one: a jump of 3 means 2 steps had no snapshot. A snapshot arriving
late was counted as missed when the next one came, so it is removed from the count when it finally shows up.

!!! note "This is not exactly packet loss"
    A late server catches up several steps in one frame and sends only one snapshot for all of them, which also shows up
    here. For the player the effect is the same: updates did not come. A snapshot split in several parts counts as
    received as soon as one part arrives.

### Server on Windows

`RTT` and `Missed` also depend on when the server wakes up, not only on the network. Its loops sleep 1 ms between two
passes, which Windows turns into about 15.6 ms by default: steps would then run up to 15 ms late, adding about 8 ms to
`RTT` on average, and any wake-up later than one step would merge two steps into one snapshot and count as `Missed`.
This is why `server/src/main.cpp` holds an `ecs::time::PreciseSleepScope` for the whole run (see
[Engine Clock](clock.md#sleeping-between-two-passes)). The client needs nothing: its raylib window asks for the same
timer period.

## Adding a metric from a game

Any plugin can write to the board. A new line goes at the end, and lines never change order:

```cpp
if (registry.hasResource<metrics::MetricsBoard>()) {
    registry.getResource<metrics::MetricsBoard>().set("Bosses", std::format("{}", count));
}
```

Pass a `metrics::MetricLevel` as third argument to colour the line. Use a `metrics::RefreshTimer` to avoid writing a new
value at every frame: `client/src/net/PlayerControlPlugin.cpp` shows both.

## Showing or hiding the overlay

The overlay is always shown for now: `client/src/main.cpp` always adds the plugin, and there is no key or option to hide
it. The game settings will add that switch, around the entities of `MetricsOverlayPlugin` (`panel()` is their parent).

## Demonstrating lag

`tc netem` adds delay and loss to the local network. To leave the rest of the machine alone, and to run without root, do
it in a separate network namespace:

```sh
unshare -rn sh -c '
    ip link set lo up
    tc qdisc add dev lo root netem delay 50ms loss 5%
    ./r-type_server 4242 & server=$!
    ./r-type_client 127.0.0.1 4242
    kill $server
'
```

The delay applies to each packet, so `delay 50ms` means a round trip of 100 ms more. To apply it to two real machines,
use `sudo tc qdisc add dev <interface> root netem ...`, and `sudo tc qdisc del dev <interface> root` to remove it.

### Measures

One player in a real room, real server and real transport over loopback on Linux, after 8 seconds of game. This was
measured with the windowless client of `tests/server/EndToEndTests.cpp`, which runs far more than 60 frames per second:
this is why its RTT is lower than the 33 ms of the real client.

| `netem` | RTT | Missed | Down | Up | Packets |
|---|---|---|---|---|---|
| none | 17 ms | 0.0 % | 10.2 KB/s | 1.1 KB/s | 60 in, 86 out /s |
| `delay 50ms` | 104 ms | 0.0 % | 10.2 KB/s | 2.3 KB/s | 60 in, 180 out /s |
| `delay 100ms` | 207 ms | 0.0 % | 10.2 KB/s | 2.3 KB/s | 60 in, 180 out /s |
| `delay 50ms loss 5%` | 144 ms | 6.7 % | 9.5 KB/s | 2.3 KB/s | 56 in, 180 out /s |

`Up` grows as soon as there is lag: each input is sent again with the next ones until it is acknowledged, up to
`INPUT_RESEND_COUNT` times, so the client ends up sending 3 datagrams per step. `Missed` covers the last 0.5 s and varies
from one period to the next: over that whole last run, 26 steps out of 474 were missed, which is 5.5 %.

The real client, at 60 FPS, reads an RTT of 33 ms without `netem` and 133 ms with `delay 50ms`: the same 100 ms of added
round trip, on top of its higher starting point.
