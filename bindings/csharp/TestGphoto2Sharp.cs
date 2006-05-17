/**
 * Short test using libgphoto2-sharp. Lists all cameras.
 *
 */

using LibGPhoto2;

class Gphoto2SharpTest {
	public static int Main() {
		System.Console.WriteLine("Testing libgphoto2-sharp...");
		CameraAbilitiesList al;
		Context ctx;
		ctx = new Context();
		al = new CameraAbilitiesList();
		al.Load(ctx);

		int count = al.Count();
		
		if (count < 0) {
			System.Console.WriteLine("CameraAbilitiesList.Count() error: " + count);
			return(1);
		} else if (count == 0) {
			System.Console.WriteLine("no camera drivers (camlibs) found in camlib dir");
			return(1);
		}

		for (int i = 0; i < count; i++) {
			CameraAbilities abilities;
			string camlib_basename;
			abilities = al.GetAbilities(i);
			camlib_basename = /* basename( */ abilities.library/*)*/;
			System.Console.WriteLine("#" + i + " " + abilities.id + " " + abilities.model + " " + camlib_basename);
		}
											
		// Return non-0 when test fails
		return 0;
	}
}
