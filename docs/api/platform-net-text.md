# Platform, networking and text

## Platform (`flint/platform`)

| Function | Notes |
|---|---|
| `SystemUptime()` | `std::expected<nanoseconds, error_code>`: time since boot, including suspend (Linux `CLOCK_BOOTTIME`) |
| `CurrentThreadId()` | OS thread id (Linux TID) |
| `SetCurrentThreadName(name)` / `CurrentThreadName()` | The name is kept in full for flint and truncated to 15 characters for the OS |
| `IsTerminal(stream)` / `SupportsAnsiColour(stream)` | `StandardStream::Output` or `Error`; honours `NO_COLOR` and `FORCE_COLOR` |
| `CurrentProcessId()`, `HostName()` | |
| `FileWatcher::Create()` | Watches files for hot reload (ADR-0014): `Watch(path)`, `Unwatch(path)`, and `Poll()`, which returns the changed files and never blocks. inotify on Linux, so saving by rename is seen; `CreatePolling()` compares modification times everywhere |

## Networking (`flint/net`)

```cpp
auto listener = flint::net::TcpListener::Listen("127.0.0.1", /*port=*/0);   // 0: OS picks; see GetPort()
auto client   = flint::net::TcpClient::Connect("127.0.0.1", listener->GetPort(), std::chrono::seconds{1});
client->SendAll("hello\n");
auto accepted = listener->Accept(std::chrono::seconds{1});                   // expected<optional<TcpClient>>
std::array<std::byte, 1024> buffer{};
auto count = (*accepted)->Receive(buffer, std::chrono::milliseconds{100});   // 0 on timeout
```

Errors are `NetError{code, systemError}` with `Describe()`. POSIX only for now; Windows returns `NotSupported`.

## Text (`flint/text`)

```cpp
flint::text::EqualsIgnoreCase("Info", "INFO");            // ASCII-only
flint::text::ToLowerAscii("Topaz");                       // "topaz"
flint::text::ParseInteger<std::uint16_t>("9020");         // expected<uint16_t, NumberError>
flint::text::ParseInteger<std::uint32_t>("ff", 16);
flint::text::ParseFloating<double>("2.5e3");
flint::text::AppendNumber(out, 3.25);                     // shortest round-trip text
```
