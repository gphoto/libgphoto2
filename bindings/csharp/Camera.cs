using System;
using System.Runtime.InteropServices;

namespace LibGPhoto2
{
	[StructLayout(LayoutKind.Sequential)]
	internal unsafe struct CameraPrivateLibrary
	{
	}
	
	[StructLayout(LayoutKind.Sequential)]
	internal unsafe struct CameraPrivateCore
	{
	}

	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct CameraText
	{
		[MarshalAs(UnmanagedType.ByValTStr, SizeConst=(32*1024))] string text;
		
		public string Text {
			get {
				return text;
			}
			set {
				text = value;
			}
		}
	}
	
#if false
	[StructLayout(LayoutKind.Sequential)]
	internal unsafe struct CameraFunctions
	{
		internal delegate ErrorCode _CameraExitFunc (_Camera *camera, HandleRef context);

		internal delegate ErrorCode _CameraGetConfigFunc (_Camera *camera, out IntPtr widget, HandleRef context);

		internal delegate ErrorCode _CameraSetConfigFunc (_Camera *camera, HandleRef widget, HandleRef context);

		internal delegate ErrorCode _CameraCaptureFunc (_Camera *camera, CameraCaptureType type, IntPtr path, HandleRef context);

		internal delegate ErrorCode _CameraCapturePreviewFunc (_Camera *camera, _CameraFile *file, HandleRef context);
		
		internal delegate ErrorCode _CameraSummaryFunc (_Camera *camera, IntPtr text, HandleRef context);
		
		internal delegate ErrorCode _CameraManualFunc (_Camera *camera, IntPtr text, HandleRef context);
		
		internal delegate ErrorCode _CameraAboutFunc (_Camera *camera, IntPtr text, HandleRef context);
		
		internal delegate ErrorCode _CameraPrePostFunc (_Camera *camera, HandleRef context);
                                             
		/* Those will be called before and after each operation */
		_CameraPrePostFunc pre_func;
		_CameraPrePostFunc post_func;

		_CameraExitFunc exit;

		/* Configuration */
		_CameraGetConfigFunc       get_config;
		_CameraSetConfigFunc       set_config;

		/* Capturing */
		_CameraCaptureFunc        capture;
		_CameraCapturePreviewFunc capture_preview;

		/* Textual information */
		_CameraSummaryFunc summary;
		_CameraManualFunc  manual;
		_CameraAboutFunc   about;
		
		/* Reserved space to use in the future without changing the struct size */
		IntPtr reserved1;
		IntPtr reserved2;
		IntPtr reserved3;
		IntPtr reserved4;
		IntPtr reserved5;
		IntPtr reserved6;
		IntPtr reserved7;
		IntPtr reserved8;
	}
#endif

	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct CameraFilePath
	{
		[MarshalAs(UnmanagedType.ByValTStr, SizeConst=128)] public string name;
		[MarshalAs(UnmanagedType.ByValTStr, SizeConst=1024)] public string folder;
	}

	public enum CameraCaptureType
	{
		Image,
		Movie,
		Sound
	}

	[StructLayout(LayoutKind.Sequential)]
	internal unsafe struct _Camera
	{
		public IntPtr port;
		public IntPtr fs;
		public IntPtr functions;

		//CameraPrivateLibrary  *pl; /* Private data of camera libraries    */
		//CameraPrivateCore     *pc; /* Private data of the core of gphoto2 */
		public IntPtr p1;
		public IntPtr pc;
		
		public IntPtr GetFS ()
		{
			return fs;
		}
	}
	
	public class Camera : Object 
	{
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_new (out IntPtr handle);

		public Camera()
		{
			IntPtr native;

			Error.CheckError (gp_camera_new (out native));
			
			this.handle = new HandleRef (this, native);
		}

		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_unref (HandleRef camera);

		protected override void Cleanup ()
		{
			gp_camera_unref(this.Handle);
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_set_abilities (HandleRef camera, CameraAbilities abilities);

		public void SetAbilities (CameraAbilities abilities)
		{
		        Error.CheckError (gp_camera_set_abilities(this.Handle, abilities));
		}
		
		[DllImport ("libgphoto2.so")]
		internal unsafe static extern ErrorCode gp_camera_get_abilities (HandleRef camera, out CameraAbilities abilities);

		public CameraAbilities GetAbilities ()
		{
			CameraAbilities abilities = new CameraAbilities ();
			
			Error.CheckError (gp_camera_get_abilities(this.Handle, out abilities));

			return abilities;
		}
		
		[DllImport ("libgphoto2.so")]
		internal unsafe static extern ErrorCode gp_camera_set_port_info (HandleRef camera, _PortInfo info);

		public void SetPortInfo (PortInfo portinfo)
		{
			unsafe {
				Error.CheckError (gp_camera_set_port_info (this.Handle, portinfo.Handle));
			}
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_get_port_info (HandleRef camera, out _PortInfo info);

		public PortInfo GetPortInfo ()
		{
			PortInfo portinfo = new PortInfo ();
			unsafe { 
				Error.CheckError (gp_camera_get_port_info (this.Handle, out portinfo.Handle));				
			}
			return portinfo;
		}
		

		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_get_port_speed (HandleRef camera);

		public int GetPortSpeed ()
		{
			return (int) Error.CheckError (gp_camera_get_port_speed (this.Handle));
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_set_port_speed (HandleRef camera, int speed);

		public void SetPortSpeed (int speed)
		{
			Error.CheckError (gp_camera_set_port_speed (this.Handle, speed));
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_init (HandleRef camera, HandleRef context);

		public void Init (Context context)
		{
			Error.CheckError (gp_camera_init (this.Handle, context.Handle));
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_exit (HandleRef camera, HandleRef context);
		
		public void Exit (Context context)
		{
			Error.CheckError (gp_camera_exit (this.Handle, context.Handle));
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_capture (HandleRef camera, CameraCaptureType type, out CameraFilePath path, HandleRef context);
		
		public CameraFilePath Capture (CameraCaptureType type, Context context)
		{
			CameraFilePath path;

			Error.CheckError (gp_camera_capture (this.Handle, type, out path, context.Handle));

			return path;
		}
		
		[DllImport ("libgphoto2.so")]
		internal unsafe static extern ErrorCode gp_camera_capture_preview (HandleRef camera, HandleRef file, HandleRef context);
		
		public CameraFile CapturePreview (Context context)
		{
			CameraFile file = new CameraFile();
			
			Error.CheckError (gp_camera_capture_preview (this.Handle, file.Handle, context.Handle));

			return file;
		}
		
#if UNUSED
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_get_config (HandleRef camera, out IntPtr window, HandleRef context);
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_set_config (HandleRef camera, out IntPtr window, HandleRef context);
#endif		

		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_folder_list_files (HandleRef camera, string folder, HandleRef list, HandleRef context);
		
		public CameraList ListFiles (string folder, Context context)
		{
			CameraList file_list = new CameraList ();
			
			Error.CheckError (gp_camera_folder_list_files(this.Handle, folder, file_list.Handle, context.Handle));

			return file_list;
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_folder_list_folders (HandleRef camera, string folder, HandleRef list, HandleRef context);

		public CameraList ListFolders (string folder, Context context)
		{
			CameraList file_list = new CameraList();

			Error.CheckError (gp_camera_folder_list_folders (this.Handle, folder, file_list.Handle, context.Handle));

			return file_list;
		}
		
		[DllImport ("libgphoto2.so")]
		internal unsafe static extern ErrorCode gp_camera_folder_put_file (HandleRef camera, [MarshalAs(UnmanagedType.LPTStr)] string folder, HandleRef file, HandleRef context);
		
		public void PutFile (string folder, CameraFile file, Context context)
		{
			Error.CheckError (gp_camera_folder_put_file(this.Handle, folder, file.Handle, context.Handle));
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_folder_delete_all (HandleRef camera, string folder, HandleRef context);
		
		public void DeleteAll (string folder, Context context)
		{
			Error.CheckError (gp_camera_folder_delete_all (this.Handle, folder, context.Handle));
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_folder_make_dir (HandleRef camera, string folder,  string name, HandleRef context);
		
		public void MakeDirectory (string folder, string name, Context context)
		{
			Error.CheckError (gp_camera_folder_make_dir (this.Handle, folder, name, context.Handle));
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_folder_remove_dir (HandleRef camera, string folder, string name, HandleRef context);
		
		public void RemoveDirectory (string folder, string name, Context context)
		{
			Error.CheckError (gp_camera_folder_remove_dir(this.Handle, folder, name, context.Handle));
		}
		

		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_file_get (HandleRef camera, string folder, string file, CameraFileType type, HandleRef camera_file, HandleRef context);
		
		public CameraFile GetFile (string folder, string name, CameraFileType type, Context context)
		{
			CameraFile file = new CameraFile();
			
			Error.CheckError (gp_camera_file_get(this.Handle, folder, name, type, file.Handle, context.Handle));

			return file;
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_file_delete (HandleRef camera, string folder, string file, HandleRef context);

		public void DeleteFile (string folder, string name, Context context)
		{
			unsafe
			{
				Error.CheckError (gp_camera_file_delete(this.Handle, folder, name, context.Handle));
			}
		}
		
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_file_get_info (HandleRef camera, string folder, string file, out CameraFileInfo info, HandleRef context);
		
		public CameraFileInfo GetFileInfo (string folder, string name, Context context)
		{
			CameraFileInfo fileinfo;
			unsafe
			{
				Error.CheckError (gp_camera_file_get_info(this.Handle, folder, name, out fileinfo, context.Handle));
			}

			return fileinfo;
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_file_set_info (HandleRef camera, string folder, string file, CameraFileInfo info, HandleRef context);
		
		public void SetFileInfo (string folder, string name, CameraFileInfo fileinfo, Context context)
		{
			unsafe
			{
				Error.CheckError (gp_camera_file_set_info(this.Handle, folder, name, fileinfo, context.Handle));
			}
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_get_manual (HandleRef camera, out CameraText manual, HandleRef context);
		
		public CameraText GetManual (Context context)
		{
			CameraText manual;
			unsafe
			{
				Error.CheckError (gp_camera_get_manual(this.Handle, out manual, context.Handle));
			}
			return manual;
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_get_summary (HandleRef camera, out CameraText summary, HandleRef context);
		
		public CameraText GetSummary (Context context)
		{
			CameraText summary;

			Error.CheckError (gp_camera_get_summary(this.Handle, out summary, context.Handle));

			return summary;
		}
		
		[DllImport ("libgphoto2.so")]
		internal static extern ErrorCode gp_camera_get_about (HandleRef camera, out CameraText about, HandleRef context);

		public CameraText GetAbout (Context context)
		{
			CameraText about;
			
			Error.CheckError (gp_camera_get_about(this.Handle, out about, context.Handle));

			return about;
		}
		
		public CameraFilesystem GetFS()
		{
			CameraFilesystem fs;
			unsafe {
				_Camera *obj = (_Camera *)this.Handle.Handle;
				fs = new CameraFilesystem((IntPtr)obj->GetFS ());
			}
			return fs;
		}
	}
}
