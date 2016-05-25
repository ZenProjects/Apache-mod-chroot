                                   mod_chroot

Major change between 0.x and 1.x version:

      Starting from version 0.3 mod_chroot supports apache 2.0.
      While most problems with Apache 1.3 are solved in 2.0 (no more module
      ordering hassle, no need to apply EAPI patches), architecture changes that
      appeared in 2.0 created one new problem: multi-processing modules (MPMs).
      MPMs are core Apache modules responsible for handling requests and
      dispatching them to child processes/threads.

      Unfortunately, MPMs are initialized after all "normal" Apache modules.
      This basically means that with mod_chroot, MPM initialization is done
      after a chroot(2) call; when control is handed to MPM, Apache is already
      inside a jail. And MPMs need to create some files during startup (at least
      one, a pidfile) - these have to be placed inside the jail. 

      Once chrooted, Apache cannot access anything located above ChrootDir. For
      that reason restarting Apache with 'apachectl reload', 'apachectl
      graceful' or 'kill -HUP apache_pid' will not work as expected. Apache will
      not be able to read its config file, open logs or load modules.
      
      Starting with the 1.0 of mod_chroot all this probleme are fixed. 

      - In first problem are resolved by placing the chroot in child_init, after the mpm initialisation,
        in that way they don't need to place pid file, scoreboard, Lock File inside the jail.

      - And the seconds probleme are command like "DocumentRoot" and "CoreDumpDirectory" that are tested a start of the server, 
        that make oblige to point to real directory outside the chroot and in the chroot... 
	This proleme is resolved by faking all map to storage transparantly like mod_alias (by setting ChrootFixRoot option).
	This also fake the CoreDumpDirectory directory.

      in that way using mod_chroot is simple like to load the module and set the chroot dir...
      the only constraint are to set document root and coredump directory in the path of the chroot dir...

      Ex. this are ok:
        ChrootDir /srv/www1
        DocumentRoot /srv/www1/htdocs
	CoreDumpDirectory /srv/www1/dump
      Ex. this are not ok:
        ChrootDir /srv/www1
        DocumentRoot /htdocs/www1
	CoreDumpDirectory /dump/www1

DNS lookups

   libresolv uses /etc/resolv.conf to find your DNS server. If this file
   doesn't exist, libresolv uses 127.0.0.1:53 as the DNS server. You can run
   a small caching server listening on 127.0.0.1 (which may be a good idea
   anyway), or use your operating system's firewall to transparently redirect
   queries to 127.0.0.1:53 to your real DNS server. Note that this is only
   necessary if you do DNS lookups - probably this can be avoided?

   Please also read the libraries section below.

Databases

   If your mySQL/PostgreSQL accepts connections on a Unix socket which is
   outside of your chroot jail, reconfigure it to listen on a loopback
   address (127.0.0.1).

PHP mail() function

   Under Unix, PHP requires a sendmail binary to send mail. Putting this file
   inside your jail may not be sufficient: you would probably need to move
   your mail queue as well. You have three options here:

     * install a SMTP-only sendmail clone like sSMTP or nbsmtp. You can then
       put a single binary inside your jail, and deliver mail via a
       smarthost.
     * don't use mail(). Use a class/function that knows how to send directly
       via SMTP (like Pear's Mail)
     * convince PHP developers to make SMTP support a configurable option
       under Unix, or write a patch yourself - remember to submit it to
       mod_chroot mailing list for others to use.

Shared libraries

   Shared libraries are libraries which are linked to a program at run-time.
   Nowadays, most programs require some shared libraries to run - libc.so is
   most common. You can see a list of shared libraries a program requires by
   running ldd /path/to/program. Loading of these libraries is done
   automagically by ld.so at startup. mod_chroot doesn't interfere with this
   mechanism.

   A program may also explicitly load a shared library by calling dlopen()
   and dlsym(). This might cause troubles in a chrooted environment - after a
   process is chrooted, libraries (usually stored in /lib) might be no longer
   accessible. This doesn't happen very often, but if it does - there is a
   solution: you can preload these libraries before chrooting. Apache has a
   handy directive for that: LoadFile. This is what people reported on the
   mailing list:

     * DNS lookups - GNU libc tries to load libnss_dns.so.2 when a first DNS
       lookup is done. Solution:

 LoadFile /lib/libnss_dns.so.2

     * Apache 2.0 with mpm_worker on Linux 2.6 - GNU libc tries to load
       libgcc_s.so.1 when pthread_cancel is called. Solution:

 LoadFile /lib/libgcc_s.so.1

Others?

   Did you have other problems with Apache+mod_chroot? Please send your
   experiences to the mailing list, I'll publish them here.
