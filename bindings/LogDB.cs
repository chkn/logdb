
using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace LogDB {

	public enum OpenFlags {
		Existing = 0,
		Create = 1,
		NoSync = 2
	}

	public class LogDBException : Exception {
		public LogDBException (string message): base (message)
		{
		}
	}

	public sealed class LogDBConnection : IEnumerable<KeyValuePair<LogDBBuffer,LogDBBuffer>>, IDisposable {

		IntPtr handle;

		public LogDBConnection (string path, OpenFlags flags)
		{
			handle = Native.logdb_open (path, flags);
			if (handle == IntPtr.Zero)
				throw new LogDBException ("logdb_open");
		}

		public IEnumerator<KeyValuePair<LogDBBuffer,LogDBBuffer>> GetEnumerator ()
		{
			var native = Native.logdb_iter_all (handle);
			if (native == IntPtr.Zero)
				throw new LogDBException ("logdb_iter_all");
			return new LogDBIter (native);
		}
		IEnumerator IEnumerable.GetEnumerator ()
		{
			return GetEnumerator ();
		}

		public void BeginTransaction ()
		{
			if (Native.logdb_begin (handle) != 0)
				throw new LogDBException ("logdb_begin");
		}

		public void Put (LogDBBuffer key, LogDBBuffer value)
		{
			if (Native.logdb_put (handle, key.Handle, value.Handle) != 0)
				throw new LogDBException ("logdb_put");
		}

		public void CommitTransaction ()
		{
			if (Native.logdb_commit (handle) != 0)
				throw new LogDBException ("logdb_commit");
		}

		public void RollbackTransaction ()
		{
			if (Native.logdb_rollback (handle) != 0)
				throw new InvalidOperationException ("No active transaction");
		}

		~LogDBConnection ()
		{
			Dispose (false);
		}
		public void Dispose ()
		{
			Dispose (true);
			GC.SuppressFinalize (this);
		}
		void Dispose (bool disposing)
		{
			if (handle != IntPtr.Zero) {
				// FIXME: Worry about failure here?
				Native.logdb_close (handle);
				handle = IntPtr.Zero;
			}
		}
	}

	sealed class LogDBIter : IEnumerator<KeyValuePair<LogDBBuffer,LogDBBuffer>> {

		IntPtr handle;
		KeyValuePair<LogDBBuffer,LogDBBuffer>? current;

		public KeyValuePair<LogDBBuffer,LogDBBuffer> Current {
			get {
				if (handle == IntPtr.Zero)
					throw new ObjectDisposedException ("LogDBIter");
				if (!current.HasValue) {
					var kptr = Native.logdb_iter_current_key (handle);
					if (kptr == IntPtr.Zero)
						throw new LogDBException ("logdb_iter_current_key");
					var vptr = Native.logdb_iter_current_value (handle);
					if (vptr == IntPtr.Zero)
						throw new LogDBException ("logdb_iter_current_value");
					current = new KeyValuePair<LogDBBuffer,LogDBBuffer> (
						new LogDBBuffer (kptr),
						new LogDBBuffer (vptr)
					);
				}
				return current.Value;
			}
		}
		object IEnumerator.Current {
			get { return Current; }
		}

		internal LogDBIter (IntPtr native)
		{
			if (native == IntPtr.Zero)
				throw new ArgumentException ("native is null ptr");
			handle = native;
		}

		public bool MoveNext ()
		{
			if (handle == IntPtr.Zero)
				throw new ObjectDisposedException ("LogDBIter");
			current = null;
			return Native.logdb_iter_next (handle) == 1;
		}

		public void Reset ()
		{
			throw new NotSupportedException ();
		}

		~LogDBIter ()
		{
			Dispose (false);
		}
		public void Dispose ()
		{
			Dispose (true);
			GC.SuppressFinalize (this);
		}
		void Dispose (bool disposing)
		{
			if (handle != IntPtr.Zero) {
				Native.logdb_iter_free (handle);
				handle = IntPtr.Zero;
			}
		}
	}

	public sealed class LogDBBuffer : IDisposable {
		
		IntPtr handle;

		public IntPtr Handle {
			get { return handle; }
		}

		public uint Length {
			get {
				if (handle == IntPtr.Zero)
					throw new ObjectDisposedException ("LogDBBuffer");
				return Native.logdb_buffer_length (handle);
			}
		}

		public IntPtr Data {
			get {
				if (handle == IntPtr.Zero)
					throw new ObjectDisposedException ("LogDBBuffer");
				return Native.logdb_buffer_data (handle);
			}
		}

		/// <summary>
		/// Wraps and retains the given pointer.
		/// </summary>
		internal LogDBBuffer (IntPtr native)
		{
			if (native == IntPtr.Zero)
				throw new ArgumentException ("native is null pointer");
			Native.logdb_buffer_retain (native);
			handle = native;
		}

		public LogDBBuffer (IntPtr data, uint length, Action<IntPtr> disposer)
		{
			handle = Native.logdb_buffer_new_direct (data, length, disposer);
			if (handle == IntPtr.Zero)
				throw new LogDBException ("logdb_buffer_new_direct");
		}

		public LogDBBuffer (IntPtr data, uint length)
		{
			handle = Native.logdb_buffer_new_copy (data, length);
			if (handle == IntPtr.Zero)
				throw new LogDBException ("logdb_buffer_new_copy");
		}

		/// <summary>
		/// Creates a <c>LogDBBuffer</c> with a copy of the given bytes.
		/// </summary>
		public LogDBBuffer (byte[] data)
		{
			unsafe {
				fixed (byte* ptr = &data[0]) {
					handle = Native.logdb_buffer_new_copy (new IntPtr (ptr), (uint)data.Length);
					if (handle == IntPtr.Zero)
						throw new LogDBException ("logdb_buffer_new_copy");
				}
			}
		}

		public void Append (LogDBBuffer other)
		{
			if (Native.logdb_buffer_append (handle, other.handle) == IntPtr.Zero)
				throw new LogDBException ("logdb_buffer_append");
		}

		~LogDBBuffer ()
		{
			Dispose (false);
		}
		public void Dispose ()
		{
			Dispose (true);
			GC.SuppressFinalize (this);
		}
		void Dispose (bool disposing)
		{
			if (handle != IntPtr.Zero) {
				Native.logdb_buffer_free (handle);
				handle = IntPtr.Zero;
			}
		}
	}

	static class Native {
		const string Library = "LogDB";

		[DllImport (Library)]
		public static extern IntPtr logdb_open (string path, OpenFlags flags);

		[DllImport (Library)]
		public static extern int logdb_close (IntPtr connection);

		[DllImport (Library)]
		public static extern IntPtr logdb_iter_all (IntPtr connection);

		[DllImport (Library)]
		public static extern int logdb_iter_next (IntPtr iter);

		[DllImport (Library)]
		public static extern IntPtr logdb_iter_current_key (IntPtr iter);

		[DllImport (Library)]
		public static extern IntPtr logdb_iter_current_value (IntPtr iter);

		[DllImport (Library)]
		public static extern void logdb_iter_free (IntPtr iter);
		
		[DllImport (Library)]
		public static extern IntPtr logdb_buffer_new_direct (IntPtr data, uint length, Action<IntPtr> disposer);

		[DllImport (Library)]
		public static extern IntPtr logdb_buffer_new_copy (IntPtr data, uint length);

		[DllImport (Library)]
		public static extern uint logdb_buffer_length (IntPtr buffer);

		[DllImport (Library)]
		public static extern IntPtr logdb_buffer_data (IntPtr buffer);

		[DllImport (Library)]
		public static extern IntPtr logdb_buffer_append (IntPtr buffer1, IntPtr buffer2);

		[DllImport (Library)]
		public static extern void logdb_buffer_retain (IntPtr buffer);

		[DllImport (Library)]
		public static extern void logdb_buffer_free (IntPtr buffer);

		[DllImport (Library)]
		public static extern int logdb_begin (IntPtr connection);

		[DllImport (Library)]
		public static extern int logdb_put (IntPtr connection, IntPtr keybuf, IntPtr valbuf);

		[DllImport (Library)]
		public static extern int logdb_commit (IntPtr connection);

		[DllImport (Library)]
		public static extern int logdb_rollback (IntPtr connection);
	}
}