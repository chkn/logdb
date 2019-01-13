# LogDB

A write-optimized, in-process key-value storage engine with multiple values per key.

**WARNING/DISCLAIMER:** This is a new project and probably has bugs. Do not use this to store any data you cannot afford to lose. As per the [license](License.md), there is no warranty of any kind. If you choose to use this project, you do so at your own risk.

Features:

- **Concurrent writes** - Writes to the database can be made concurrently by multiple threads and processes.
- **Atomic and durable** - Supports transactions for atomic writes. Writers do not need to block other writers to make a fully durable commit.
- **Single file database format** - A log file will be created alongside the database file while the database is open.

Caveats of current implementation:

- No indexing yet. The only mode for reading the database is iterating through all records.
- Reads do not interact with transactions. There is no way to read uncommitted writes.
- No compression nor compaction; the database file may have some wasted space. However, the write algorithm attempts to mitigate this.
- For writing, the size of the entire transaction (including nested transactions) must currently be less than 65KB. We will eventually eliminate this requirement.
- Only supports `put` and `iterate` (read) operations (no `update` nor `delete`).
- Should be robust against application crashes (and system-wide failures if `LOGDB_OPEN_NOSYNC` is not specified), however this is largely untested as of yet.
- Developed and tested on OSX and iOS only.
    - Uses POSIX APIs, so should be portable.
    - May need some changes to work correctly on big endian machines.
    - For Linux support we would also need to work around [https://bugzilla.kernel.org/show_bug.cgi?id=43178](https://bugzilla.kernel.org/show_bug.cgi?id=43178) (run `./premake5 configure` to test if your system is affected)

## So, what is it good for?

My original use case was logging, hence the name.


## Building

1. Git clone
2. Download Premake 5 from [http://premake.github.io](http://premake.github.io/) and put it in the root of the git checkout.
3. Change to that directory and run `./premake5 configure` to run the system checks.
4. If that succeeds, run one of the following:

### Command line build

	$ ./premake5 gmake && make

### Create Xcode project for Mac

	$ ./premake5 xcode4

### Create Xcode project for iOS

	$ ./premake5 --os=ios xcode4

### Build for Xamarin.iOS

	$ xbuild bindings/C#/LogDB.iOS.csproj

### Clean

	$ ./premake5 clean

### Create Nuget Package for C# bindings

	$ xbuild bindings/C#/Bindings.sln /p:Configuration=Release

## Running the Tests

1. Follow the instructions in the previous section to build.
2. From the command line, run `bin/Debug/Tests`. A [VS Code](https://code.visualstudio.com) launch configuration is also included to aid in debugging the tests.
3. Under the `stress` directory, there is a multiprocess and multithreaded stress test for concurrent writes. Run it with `StressTestProcs.sh` and then inspect the resulting DB for data consistency. For instance, here are some results I get on my 2016 MacBook Pro (3.3 GHz i7, 16GB RAM) writing 5,000 small records to a database:

```
$ cd stress

# 1 process, 1 thread writing 5000 records (resulting DB size: 88K)
$ time ./StressTestProcs.sh test.db 1 1 5000

real	0m0.589s
user	0m0.051s
sys	0m0.372s

# 1 process, 5 threads each writing 1000 records (resulting DB size: 236K)
$ time ./StressTestProcs.sh test.db 1 5 1000

real	0m1.127s
user	0m0.356s
sys	0m1.405s

# 5 processes, each with 1 thread writing 1000 records (resulting DB size: 107K)
$ time ./StressTestProcs.sh test.db 5 1 1000

real	0m0.899s
user	0m0.393s
sys	0m1.402s

# 5 processes, each with 5 threads each writing 200 records (resulting DB size: 111K)
$ time ./StressTestProcs.sh test.db 5 5 200

real	0m1.565s
user	0m1.248s
sys	0m3.619s

# After any of the above commands, run this to dump the test data
#  Each thread of each process should have written numbers from 0 to your specified maximum
$ ../bin/Debug/StressTestDump test.db | sort | less
```

The stress test is a pathological case for thread contention, with each thread constantly writing to the DB in a tight loop, which is why we see such performance degradation as we add more threads. However, given that multiple processes seem more performant than multiple threads, perhaps we can further optimize our in-process locking for steps 3-4 below.

## Design

In order to be fully concurrent, LogDB allows multiple writers to write to different parts of the database simultaneously. To make this work without data corruption, a separate file is used as a log. Here's how it works:

1. When the first connection is opened with `logdb_open`, an exclusive lock is taken on the database while a log is built in a separate file.

2. The log is basically a list of segments in the database file, and the number of bytes in those segments that contain valid data (all data is written contiguously within a segment). Each entry in the log has a fixed length.

3. When a thread or process wishes to _lease_ a segment of the database for writing, it starts at the last entry of the log and walks backward until it finds an entry with enough free space or has hit an arbitrary limit of entries to walk. If the writer does not find an entry with enough free space, it simply appends an entry to the log. Since the size of the entry is so small, this should be atomic on all OSes and file systems.

4. A write lock is taken, via `fcntl`, on the new entry written to the log in step 3 to prevent racing with other writers and to prevent readers from reading inconsistent data. Note that this is a granular lock on this log entry only-- other writers are not blocked. If the lock cannot be taken at this point, we repeat step 3, remembering how many log entries we've already walked.

5. After the data is written to the portion of the database that corresponds to the locked log entry and is `fsync`d to the database file, that log entry is changed from zero valid bytes to the actual size of the data written. This behavior is what enables the atomic transaction semantics.


## Implementation Notes

- Database files are locked with `flock` (more efficient whole-file locking on some OSes, e.g. Darwin), while log files are locked with `fcntl` (provides more granular locking).

