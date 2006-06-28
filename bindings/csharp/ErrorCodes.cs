using System;
using System.Runtime.InteropServices;

namespace LibGPhoto2
{
	public enum ErrorCode
	{
        /* libgphoto2_port errors */
        GeneralError        = -1,
        BadParameters       = -2,
        NoMemory            = -3,
        Library             = -4,
        UnknownPort         = -5,
        NotSupported        = -6,
        IO                  = -7,
        Timout              = -10,
        SupportedSerial		= -20,
        SupportedUSB		= -21,
        Init                = -31,
        Read                = -34,
        Write               = -35,
        Update              = -37,
        SerialSpeed         = -41,
        USBClearHalt        = -51,
        USBFind             = -52,
        USBClaim            = -53,
        Lock                = -60,
        Hal                 = -70,

        /* libgphogphoto2 errors */
        CorruptedData       = -102,
        FileExists          = -103,
        ModelNotFound       = -105,
        DirectoryNotFound   = -107,
        FileNotFound        = -108,
        DirectoryExists     = -109,
        CameraBusy          = -110,
        PathNotAbsolute     = -111,
        Cancel              = -112,
        CameraError         = -113,
        OsFailure           = -114
	}

	public class Error
	{
		private static string GetErrorAsString(ErrorCode e)
		{
			IntPtr raw_message = gp_result_as_string(e);
			return Marshal.PtrToStringAnsi(raw_message);
		}

		private static string GetIOErrorAsString(ErrorCode e)
		{
			IntPtr raw_message = gp_port_result_as_string(e);
			return Marshal.PtrToStringAnsi(raw_message);
		}
		
		public static bool IsError (ErrorCode error_code)
		{
			return (error_code < 0);
		}
		
		public static GPhotoException ErrorException (ErrorCode error_code)
		{
			string message = "Unknown Error";
			int error_code_int = (int)error_code;
			
			if (error_code_int <= -102 && error_code_int >= -111)
				message = GetErrorAsString(error_code);
			else if (error_code_int <= -1 && error_code_int >= -60)
				message = GetIOErrorAsString(error_code);

			return new GPhotoException(error_code, message);
		}
		
		public static ErrorCode CheckError (ErrorCode error)
		{
			if (IsError (error))
				throw ErrorException (error);
			
			return error;
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern IntPtr gp_result_as_string (ErrorCode result);
		
		[DllImport ("libgphoto2_port.so")]
		internal static extern IntPtr gp_port_result_as_string (ErrorCode result);
	}
	
	public class GPhotoException : Exception
	{
		private ErrorCode error;
		
		public GPhotoException(ErrorCode error_code)
		: base ("Unknown Error.")
		{
			error = error_code;
		}
		
		public GPhotoException (ErrorCode error_code, string message)
		: base (message)
		{
			error = error_code;
		}
		
		public override string ToString()
		{
			return ("Error " + error.ToString() + ": " + base.ToString());
		}

		public ErrorCode Error {
			get {
				return error;
			}
		}
	}
}
