# LogDB

A write-optimized, in-process key-value storage engine with multiple values per key.

Features:

- **Write-optimized** - Writes to the database can be made concurrently by multiple threads and processes.
- **Atomic and durable** - Supports transactions for atomic writes. Writers do not need to block other writers to make a fully durable commit.
- **Single file database format** - A log file will be created alongside the database file while the database is open.

Caveats of current implementation:

- Slower reads.
- No compression, and database file may have a some wasted space.
- Only supports `insert` and `select` operations (no `update` nor `delete`).
- Should be robust against application crashes, but possible data corruption on system-wide failure (e.g. power loss).
- Developed and tested on OSX and iOS only.
    - Uses mostly POSIX APIs, but some Darwin-specific APIs may have snuck in.
    - May need some changes to work correctly on big endian machines.
    - For Linux support we would also need to work around [https://bugzilla.kernel.org/show_bug.cgi?id=43178](https://bugzilla.kernel.org/show_bug.cgi?id=43178)

## So, what is it good for?

Original use case is logging, hence the name.


## Build

1. Git clone
2. Download Premake 5 from [http://premake.github.io](http://premake.github.io/) and put it in the root of the git checkout.
3. Change to that directory and run `./premake5 configure` to run the system checks.
4. If that succeeds, run `./premake5 gmake && make`

## Design

In order to be fully concurrent, LogDB allows multiple writers to write to different parts of the database simultaneously. To make this work without data corruption, a separate file is used as a log. Here's how it works:

1. When the first connection is opened with `logdb_open`, an exclusive lock is taken on the database while a log is built in a separate file.

2. The log is basically a list of segments in the database file, and the number of bytes in those segments that contain valid data (all data is written contiguously within a segment). Each entry in the log has a fixed length.

3. When a thread or process wishes to _lease_ a segment of the database for writing, it starts at the last entry of the log and walks backward until it finds an entry with enough free space or has hit an arbitrary limit of entries to walk. If the writer does not find an entry with enough free space, or was unable to acquire a lock on the entry (see step 4), it simply appends an entry to the log. Since the size of the entry is so small, this should be atomic on all OSes and file systems.

4. A write lock is taken, via `fcntl`, on the new entry written to the log in step 3 to prevent racing with other writers and to prevent readers from reading inconsistent data. Note that this is a granular lock on this log entry only-- other writers are not blocked.

5. After the data is written to the portion of the database that corresponds to the locked log entry and is `fsync`d to the database file, that log entry is changed from zero valid bytes to the actual size of the data written. This behavior is what enables the atomic transaction semantics.

## Still TO DO

- Recovery when a log is left because the application previously crashed or failed to properly close the database for another reason.
- Support for transactions larger than 65KB
- Garbage collection/defragmentation



## Implementation Notes

- Database files are locked with `flock` (more efficient whole-file locking), while index files are locked with `fcntl` (provides more granular locking).

