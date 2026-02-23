## What's New in v1.26.3

### Lazy Thread Start for Async/Batched Sinks

Background processing threads in `AsyncSink` and `BatchedSink` now start lazily on the first log call instead of at construction time. This fixes intermittent CI timeouts where loggers created but never used would block indefinitely during destruction waiting for a thread join.

**Before:** Creating a logger always spawned a background thread, even if no messages were ever logged. Destroying such a logger could hang for the full test timeout (120s).

**After:** No thread is started until the first message is enqueued. Destroying an unused logger completes instantly.

This is a transparent fix — no API changes, no behavior differences for loggers that actually write messages.
