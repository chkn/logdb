# LogDB

A write-optimized, in-process key-value storage engine with multiple values per key.

Features:

- **Write-optimized** - Writes to the database can be made concurrently by multiple threads and processes.
- **Atomic and durable** - Supports transactions for atomic writes. Writers do not need to block to make a fully durable commit.
- **Single file database format** - Log and index files will be created alongside the database file while the database is open.

Caveats of current implementation:

- Slower reads.
- No compression, and database file may have a some wasted space.
- Only supports `insert` and `select` operations (no `update` nor `delete`).
- Should be robust against application crashes, but possible data corruption on system-wide failure (e.g. power loss).
- Developed and tested on OSX and iOS only. Theoretically should build on other POSIX systems, but untested.

## So, what is it good for?

Original use case is logging, hence the name.


## Build

1. Git clone
2. Download Premake 5 from [http://premake.github.io](http://premake.github.io/) and put it in the root of the git checkout.
3. Change to that directory and `./premake5 gmake && make`

## Design

In order to be fully concurrent, LogDB allows multiple writers to write to different parts of the database simultaneously. To make this work without data corruption, a separate file is used as an index and rollback log. Here's how it works:

1. When the first connection is opened with `logdb_open`, an exclusive lock is taken on the database while a log is built in a separate file.

2. The log is basically a list of segments in the database file, and the number of bytes in that segment that contain valid data (all data is written contiguously within a segment). Each entry in the log has a fixed length, so these can also be written concurrently.

3. When a thread or process wishes to _lease_ a section of the database for writing, it finds the next available segment in the index and tries to acquire an exclusive lock on that part of the index file via `fcntl`. If the lock cannot be acquired, it means there was a race with another writer. This step is repeated until a lock can be acquired.

## Implementation Notes

- Database files are locked with `flock` (more efficient whole-file locking), while index files are locked with `fcntl` (provides more granular locking).

