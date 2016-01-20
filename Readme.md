# LogDB

A key-value storage engine with multiple values per key. It is optimized for blazing fast, concurrent writes.

Features:

- Fast, concurrent writes.

Caveats:

- Slower reads.
- Possible data corruption on system-wide failure (e.g. power loss).
- Developed/tested on OSX and iOS only. Theoretically should build on other POSIX systems, but untested.


## Build

1. Git clone
2. Download Premake 5 from [http://premake.github.io](http://premake.github.io/) and put it in the root of the git checkout.
3. Change to that directory and `./premake5 gmake && make`

## Design

In order to be fully concurrent, LogDB allows multiple writers to write to different parts of the database simultaneously. To make this work without data corruption, a separate file is used as an index and rollback log. Here's how it works:

1. When the first connection is opened with `logdb_open`, an exclusive lock is taken on the database while an index is built in a separate file.

2. The index is basically a list of segments in the database file, and the number of bytes in that segment that contain valid data (all data is written contiguously). Each entry in the index has a fixed length, so these can also be written concurrently.

3. When a thread or process wishes to reserve a section of the database for writing, it finds the next available segment in the index and tries to acquire an exclusive lock on that part of the index file via `fcntl`. If the lock cannot be acquired, it means there was a race with another writer. This step is repeated until a lock can be acquired.

## Implementation Notes

- Database files are locked with `flock` (more efficient whole-file locking), while index files are locked with `fcntl` (provides more granular locking).

