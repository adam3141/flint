# 0007. Network logging as TCP JSON Lines

- Status: Accepted
- Date: 2026-11-24

## Context

A network sink lets logs be watched live from another machine or terminal, or collected centrally. The options were TCP JSON Lines, UDP datagrams and syslog. The owner chose TCP JSON Lines.

## Decision

- `NetworkSink` connects to `host:port` (default `127.0.0.1:9020`) and sends the same JSON object per line as the JSON file sink.
- Sending happens on the sink's own thread. While it is disconnected, the sink keeps a bounded buffer (dropping the oldest lines and counting them) and reconnects with exponential back-off from 250 ms to 5 s.
- `Flush` waits at most the connect timeout and never blocks while disconnected, so a missing collector cannot stall the logger.
- `flint-logcat` is the matching collector. It accepts several clients, decodes records and prints them with the console formatter. `--raw` passes the JSON through for `jq`.
- `flint::net` provides the small TCP client and listener this needs. Only POSIX is implemented; on Windows the calls return `NotSupported` until Winsock support is added.

## Consequences

- netcat, Vector, Logstash and Fluent Bit can consume the stream without flint.
- TCP means a slow collector applies back-pressure only to the sink's buffer, never to the application.
