                                   mod_chroot

What is it?

   mod_chroot makes running Apache in a secure chroot environment easy. You
   don't need to create a special directory hierarchy containing /dev, /lib,
   /etc...

   mod_chroot is now included in

     * FreeBSD
     * DarwinPorts
     * PLD Linux
     * Gentoo Linux
     * Debian testing/unstable
     * NetBSD

   Many thanks to all package maintainers!

Why chroot?

   For security.

   chroot(2) changes the root directory of a process to a directory other
   than "/". It means the process is locked inside a virtual filesystem root.
   If you configure your chroot jail properly, Apache and its child processes
   (think CGI scripts) won't be able to access anything except the jail.

   A non-root process is not able to leave a chroot jail. Still it's not wise
   to put device files, suid binaries or hardlinks inside the jail.

chroot - the hard way

   There are many documents about running programs inside a chroot jail. Some
   daemons (tinydns, dnscache, vsftpd) support it out of the box. For others
   (like Apache) you need to carefully build a "virtual root", containing
   every file the program may need. This usually includes:

     * C library
     * various other libraries (libssl? libm? libmysqlclient?)
     * resolver configuration files (/etc/nsswitch.conf, /etc/resolv.conf)
     * user files (/etc/passwd, /etc/group)
     * separate directory for log files
     * additional modules needed by the program (for Apache: mod_php and
       other modules)

   Creating this structure is great fun. Run the program, read the error
   message, copy the missing file, start over. Now think about upgrading -
   you have to keep your "virtual root" current - if there is a bug in
   libssl, you need to put a new version in two places. Scared enough? Read
   on.

chroot - the mod_chroot way

   mod_chroot allows you to run Apache in a chroot jail with no additional
   files. The chroot() system call is performed at the end of startup
   procedure - when all libraries are loaded and log files open. There are
   still some things you have to keep in mind - see below.

   Installation and configuration is covered by INSTALL.

Caveats

   Running Apache (and CGI/Perl/PHP) inside a chroot jail can be tricky. Read
   CAVEATS for known problems and solutions.

   Starting 1.0 they support only Apache 2.0.
   It has been tested with Apache 2.0.49/59 and 2.2.4 under Linux 2.6.  It should
   work under older versions of Apache 2.0 as well. 

Download

   All published version of mod_chroot are available at
   http://core.segfault.pl/~hobbit/mod_chroot/dist. Please use the latest
   one.

Contact

   Mail addresses:

     * modchroot-bugs@core.segfault.pl - report bugs here.
     * modchroot@core.segfault.pl - mod_chroot mailing list. Questions,
       feature requests, announcements should go here.
       Send an empty e-mail to modchroot-subscribe@core.segfault.pl to
       subscribe. Users who are not subscribed are not allowed to post.

   mod_chroot mailing list is also available via GMane (as
   gmane.comp.apache.mod-chroot.general). GMane also has a nice archive.

Prior art

   I needed a simple module just to perform chroot at startup. Before I
   started coding, I found mod_security which does this, among others. I
   didn't need URL normalization and other mod_security features so I decided
   to create my own module. My code is similar to mod_security, with some
   sanity checks added in chroot are move in child_init hook to make no need 
   to have pidfile,scoreboard,lockfile in chroot jail. mod_security is developed by Ivan Ristic.
