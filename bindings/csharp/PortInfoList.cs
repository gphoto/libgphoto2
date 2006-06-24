using System;
using System.Runtime.InteropServices;

namespace LibGPhoto2
{
	public class PortInfoList : Object 
	{
		[DllImport ("libgphoto2_port.so")]
		internal static extern ErrorCode gp_port_info_list_new (out IntPtr handle);
		
		public PortInfoList()
		{
			IntPtr native;

			Error.CheckError (gp_port_info_list_new (out native));

			this.handle = new HandleRef (this, native);
		}
		
		[DllImport ("libgphoto2_port.so")]
		internal static extern ErrorCode gp_port_info_list_free (HandleRef handle);
		
		protected override void Cleanup ()
		{
			Error.CheckError (gp_port_info_list_free (this.Handle));
		}
		
		[DllImport ("libgphoto2_port.so")]
		internal static extern ErrorCode gp_port_info_list_load (HandleRef handle);

		public void Load ()
		{
			ErrorCode result = gp_port_info_list_load (this.Handle);

			if (Error.IsError (result))
				throw Error.ErrorException (result);
		}
		
		[DllImport ("libgphoto2_port.so")]
		internal static extern ErrorCode gp_port_info_list_count (HandleRef handle);

		public int Count()
		{
			return (int) Error.CheckError (gp_port_info_list_count (this.Handle));
		}
		
		[DllImport ("libgphoto2_port.so")]
		internal unsafe static extern ErrorCode gp_port_info_list_get_info (HandleRef handle, int n, out _PortInfo info);

		public PortInfo GetInfo (int n)
		{
			PortInfo info = new PortInfo ();
			unsafe {
				Error.CheckError (gp_port_info_list_get_info (this.handle, n,  out info.Handle));
			}
			return info;
		}
		
		[DllImport ("libgphoto2_port.so")]
		internal static extern ErrorCode gp_port_info_list_lookup_path (HandleRef handle, [MarshalAs(UnmanagedType.LPTStr)]string path);

		public int LookupPath (string path)
		{
			return (int) Error.CheckError (gp_port_info_list_lookup_path(this.handle, path));
		}
		
		[DllImport ("libgphoto2_port.so")]
		internal static extern ErrorCode gp_port_info_list_lookup_name (HandleRef handle, string name);

		public int LookupName(string name)
		{
			return (int) Error.CheckError (gp_port_info_list_lookup_name (this.Handle, name));
		}
		
		[DllImport ("libgphoto2_port.so")]
		internal unsafe static extern ErrorCode gp_port_info_list_append (HandleRef handle, _PortInfo info);

		public int Append (PortInfo info)
		{
			unsafe {
				return (int) Error.CheckError (gp_port_info_list_append (this.Handle, info.Handle));
			}
		}
	}
}
