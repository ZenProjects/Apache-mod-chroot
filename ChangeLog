2004-04-06	Hobbit
	* mod_chroot/0.1 - initial release

2004-04-08	Hobbit
	* mod_chroot/0.2 - no getppid() heuristics when EAPI is available.

2004-11-31	Hobbit
	* mod_chroot/0.3 - with Apache 2.0 support
	
2005-01-09	Hobbit
	* getenv()/setenv() instead of flaky getppid().
	* fixed layout to make <IfModule> work as expected.
	* documentation updates.
	* released mod_chroot/0.4

2005-06-12	Hobbit
	* fixed a problem with some CGIs (mod_cgid, mod_fcgid)
	  being executed outside the chroot jail.
	  Patch by <clement.hermann*free.fr>, thanks!
	* updated documentation
	* released mod_chroot/0.5

2008-02-18	Mathieu CARBONNEAUX
	* released mod_chroot/1.0beta1
	* only apache 2.x
	* transparente chroot (ChrootFixRoot option)
	* chroot in child_init to fix all resiliante problem with apache 2.0 file open after chrooting...
	* fixed a problem restart apache chrooted in moving chrooting a
	  childinit phase. that fixed also most libraries problem, all module that are loader
	  with apache no need to include librarie in the chroot.
	* all file (pid, lockfile, scoreboard, coredump...) that are created in mpm phase are created outside the jail 
	  without probleme beceause the jail is setup at child_init phase...
	* fixe documentroot problem with chroot that need to exist in chroot
	  and outside the chroot : now when activate mod_chroot they set automaticly the chroot directory to the document root... 
	  to acheave that they transparantly rewrite all file path internaly in apache like mod_alias 
	  to fake all programme and module of apache. with that no more need to 
	  trick with document root in chroot and outside chroot...
	* fake also coredumpdirectory to permit core dump normaly if they are activated...

2008-05-05	Mathieu CARBONNEAUX
	* released mod_chroot/1.0beta2
	* simplification, fixing some hints...
	* clarifying...
