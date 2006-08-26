using System;
using System.Runtime.InteropServices;

namespace LibGPhoto2 {
	public abstract class Object : System.IDisposable {
	    protected HandleRef handle;
	    private bool disposed = false;
		
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
		
		public void Dispose ()
		{
		    Dispose (true);
			System.GC.SuppressFinalize (this);
		}
		
        private void Dispose (bool disposing)
        {
            if (!disposed) {
                if (disposing) {
                    // clean up anything that's managed
                }
                // clean up any unmanaged objects
                Cleanup ();
                disposed = true;
            } else {
                Console.WriteLine ("Saved us from doubly disposing an object!");
            }
        }
        
		~Object ()
		{
			Dispose (false);
		}
	}
}
