gphoto-m4
=========

`gphoto-m4` is the gPhoto projects' collection of m4 macros for use
with the autoconf/automake based build systems.

It has been designed to be used in

  * `gphoto2`
  * `libgphoto2`
  * `libgphoto2_port` (located in `libgphoto2/`)
  * `gtkam`

Some macros are re-used ones from `libexif`.

Since the gPhoto project moved from SVN to git, we have not figured
out yet how to properly include the `gphoto-m4` files into the
respective software's `gphoto-m4/` subdirectory (either `git subtree`
or `git submodule` come to mind).

So for the time being, we manually update and synchronize the files.


Use of `gphoto-m4` after the switch to git from SVN
---------------------------------------------------

Since the gPhoto project moved from SVN to git, we have not figured
out yet how to properly include the `gphoto-m4` files into the
respective software's `gphoto-m4/` subdirectory. The options are:

  1. Manually update (and hopefully synchronize) the files.

     Advantages:

       * No special commands needed for users or developers.

     Disadvantages:

       * The manual work for the maintainers is exhausting and error
         prone.

  2. Use `git submodule`.

     Advantages:

       * Defined mechanism for syncing and updating the `gphoto-m4/*`
         files.

     Disadvantages:

       * Requires special git commands from everybody (users,
         developers, and maintainers) all the time (e.g. `git clone
         --recursive` instead of `git clone`).

         This is the showstopper for `git submodule`.

  3. Use `git subtree`.

     Advantages:

       * Defined mechanism for syncing and updating the `gphoto-m4/*`
         files.

       * Requires no special git commands from users or developers.

       * Not even people actually messing with the files in
         `gphoto-m4/*` strictly need special commands.

         Only the maintainers who do the syncing and updating of the
		 `gphoto-m4/*` files need the special commands.

     Disadvantages:

       * Requires special knowledge of special commands instead of
         just copying files around. The concept is less complex than
         copying files round, but it does require special commands.

       * No rebasing possible across pulls to `gphoto-m4/`. Not really
         necessary anyway, though.

       * Pushes of changes to from, say, `gphoto2/gphoto-m4` to
         upstream `gphoto-m4` create a lot of commit history noise in
         the `gphoto-m4` repository by including the complete history
         of the `gphoto2` repository.

         Note this cannot be avoided by using `git subtree split`:
         That is executed internally by `git subtree push`.

         So it appears that using `git subtree push` will push all
         projects' commit history into the `gphoto-m4` repo.

         That is if not a showstopper, then at least very ugly.

For the time being, we manually update and synchronize the files.


Manually syncing and updating files
-----------------------------------

This section has not been written yet.


Using `git submodule`
---------------------

This section has not been written yet.


Using `git subtree`
-------------------


### Using `gphoto-m4` as git subtree ###

This section describes how to use `gphoto-m4` when it has been set up
as a `git subtree` (e.g. in `gphoto2` and `libgphoto2`).

Add a remote for `gphoto-m4`:

    git remote add origin-gphoto-m4 git@github.com:gphoto/gphoto-m4.git

Pull changes to origin-gphoto-m4 into local `gphoto-m4`:

    git subtree pull --prefix gphoto-m4 origin-gphoto-m4 master --squash

Now we can push local changes to 'gphoto-m4/*' back to
`origin-gphoto-m4` as follows:

    git subtree push --prefix gphoto-m4 origin-gphoto-m4 master

FIXME: Add more typical uses cases.


### Setting up `gphoto-m4` as git subtree ###

This section describes how we initially set up `gphoto-m4` as a git
subtree for `gphoto2`. This is only required once by one person, then
never needs to be done by anybody else ever again.

    git subtree add --prefix gphoto-m4 origin-gphoto-m4 master --squash
