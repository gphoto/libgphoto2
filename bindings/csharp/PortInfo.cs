using System;
using System.Runtime.InteropServices;

namespace LibGPhoto2
{
	[StructLayout(LayoutKind.Sequential)]
	internal unsafe struct _PortInfo
	{
		internal PortType type;
		[MarshalAs(UnmanagedType.ByValTStr, SizeConst=64)] internal string name;
		[MarshalAs(UnmanagedType.ByValTStr, SizeConst=64)] internal string path;

		/* Private */
		[MarshalAs(UnmanagedType.ByValTStr, SizeConst=1024)] internal string library_filename;
	}
	
	public class PortInfo 
	{
		internal _PortInfo Handle;

		internal PortInfo () {
		}
		
		public string Name {
			get {
				unsafe {
					return Handle.name;
				}
			}
		}
		
		public string Path {
			get {
				unsafe {
					return Handle.path;
				}
			}
		}
	}
}
