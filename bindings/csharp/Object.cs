using System;
using System.Runtime.InteropServices;

namespace LibGPhoto2 {
	public abstract class Object : System.IDisposable {
		protected HandleRef handle;
		
		public HandleRef Handle {
			get {
				return handle;
			}
		}
		
		public Object () {}

		public Object (IntPtr ptr)
		{
			handle = new HandleRef (this, ptr);
		}
		
		protected abstract void Cleanup ();
		
		public void Dispose () {
			Cleanup ();
			System.GC.SuppressFinalize (this);
		}
		
		~Object ()
		{
			Cleanup ();
		}
	}
}
