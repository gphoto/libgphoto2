using System;
using System.Runtime.InteropServices;

namespace LibGPhoto2
{
	public class CameraList : Object 
	{
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_list_new (out IntPtr list);

		public CameraList ()
		{
			IntPtr native;
			Error.CheckError (gp_list_new (out native));
					  
			this.handle = new HandleRef (this, native);
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_list_unref (HandleRef list);

		protected override void Cleanup ()
		{
			gp_list_unref (handle);
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_list_count (HandleRef list);
		
		public int Count ()
		{
			ErrorCode result = gp_list_count (handle);

			if (Error.IsError (result))
				throw Error.ErrorException (result);

			return (int)result;
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_list_set_name (HandleRef list, int index, string name);

		public void SetName (int n, string name)
		{
			ErrorCode result = gp_list_set_name(this.Handle, n, name);

			if (Error.IsError (result))
				throw Error.ErrorException (result);
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_list_set_value (HandleRef list, int index, string value);

		public void SetValue (int n, string value)
		{
			ErrorCode result = gp_list_set_value (this.Handle, n, value);

			if (Error.IsError (result))
				throw Error.ErrorException (result);
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_list_get_name (HandleRef list, int index, out string name);

		public string GetName (int index)
		{
			string name;

			Error.CheckError (gp_list_get_name(this.Handle, index, out name));

			return name;
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_list_get_value (HandleRef list, int index, out string value);

		public string GetValue (int index)
		{
			string value;

			Error.CheckError (gp_list_get_value(this.Handle, index, out value));

			return value;
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_list_append (HandleRef list, string name, string value);

		public void Append (string name, string value)
		{
			Error.CheckError (gp_list_append(this.Handle, name, value));
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_list_populate (HandleRef list, string format, int count);

		public void Populate (string format, int count)
		{
			Error.CheckError (gp_list_populate(this.Handle, format, count));
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_list_reset (HandleRef list);

		public void Reset ()
		{
			Error.CheckError (gp_list_reset(this.Handle));
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_list_sort (HandleRef list);

		public void Sort ()
		{
			Error.CheckError (gp_list_sort(this.Handle));
		}
		
		public int GetPosition(string name, string value)
		{
			for (int index = 0; index < Count(); index++)
			{
				if (GetName(index) == name && GetValue(index) == value)
					return index;
			}
			
			return -1;
		}
	}
}
