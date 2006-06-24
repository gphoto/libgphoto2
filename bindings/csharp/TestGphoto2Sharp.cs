/**
 * Short test using libgphoto2-sharp. Lists all cameras.
 *
 */

using LibGPhoto2;

class Gphoto2SharpTest {

	static string basename(string filename) {
		char [] chars = { '/', '\\' };
		string [] components = filename.Split(chars);
		return components[components.Length-1];
	}
	
	public static int Main() {
		System.Console.WriteLine("Testing libgphoto2-sharp...");
		Context ctx = new Context();
		CameraAbilitiesList al = new CameraAbilitiesList();
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
			CameraAbilities abilities = al.GetAbilities(i);
			string camlib_basename = basename(abilities.library);
			System.Console.WriteLine("{0,3}  {3,-20}  {1,-20}  {2}",
					i,
					abilities.id,
					abilities.model,
					camlib_basename);
		}
											
		// Return non-0 when test fails
		return 0;
	}
}
