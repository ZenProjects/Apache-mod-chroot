# mod_chroot

## What is it?

   mod_chroot makes running Apache in a secure chroot environment easy. You
   don't need to create a special directory hierarchy containing /dev, /lib,
   /etc...

## Why chroot?

   For security.

   chroot(2) changes the root directory of a process to a directory other
   than "/". It means the process is locked inside a virtual filesystem root.
   If you configure your chroot jail properly, Apache and its child processes
   (think CGI scripts) won't be able to access anything except the jail.

   A non-root process is not able to leave a chroot jail. Still it's not wise
   to put device files, suid binaries or hardlinks inside the jail.

## chroot - the hard way

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

## chroot - the mod_chroot way

   mod_chroot allows you to run Apache in a chroot jail with no additional
   files. The chroot() system call is performed at the end of startup
   procedure - when all libraries are loaded and log files open. 

## Requirement

   in order to build this module you need:
   - apxs (shipped as apxs2 by some distributors) and Apache
     headers. If you compiled Apache from source you already have these
     headers. If you use Debian, you need to install apache2-prefork-dev or
     apache2-threaded-dev.
   - Make
   - C compiler (ex:gcc).

   Starting 1.0 they support only Apache 2.0.
   It has been tested with Apache 2.0.49/59 and 2.2.4 under Linux 2.6.  It should
   work under older versions of Apache 2.0 as well. 

## Building mod_chroot

   1 - Go to mod_chroot source directory and type:

````
	# ./configure --with-apxs=/path/to/apxs
	# make
	# make install
```

   2 - add in your httpd.conf the loading of the module:

```
       <IfModule !mod_chroot.c>
       LoadModule chroot_module modules/mod_chroot.so
       </IfModule>
```

## Configuration 

mod_chroot provides two configuration directive:

### ChrootDir: 

It can only be used in main server configuration. You can't put ChrootDir inside
a <Directory>, <Files>, <Location>, <VirtualHost> sections or .htaccess
files. 

Define the chroot virtual path base.

Example: if you store your www files in /var/www and you want to
make it as "virtual root", set ChrootDir this path. 
      
#### You can use apache server root relative path.

Example: 
```
  ChrootDir chroot
```

if the server root is /srv/www1 and ChrootDir is set to "chroot" while point to /srv/www1/chroot directory

#### you can use some hints to set the chroot directory to value with apache internal value:

- "DOCUMENT_ROOT" => set the chroot to the globale apache document root value. 
  (to use this you must set documentroot before ChrootDir cmd).

- "SERVER_ROOT" => set the chroot to the apache server root.
  (-d args of httpd, or ServerRoot apache directive)

Example: 
```
ChrootDir DOCUMENT_ROOT
```
to set the chroot to the apache document root.

### ChrootFixRoot: 

It can only be used in main server configuration. You can't put ChrootDir inside
a <Directory>, <Files>, <Location>, <VirtualHost> sections or .htaccess
files. 

The DocumentRoot (global config context) apache commande verify if directory exist 
before the chroot is activated, in that way you cannot use DocumentRoot (global config context) 
relative to chroot (/www in place /var/chroot/www if chroot is /var/chroot). 

Note: If you not set global config context DocumentRoot apache set it to default value (define at compile time) 
and check it in the same way.

Exemple: If you had DocumentRoot pointing to "/var/chroot/www" and ChootDir pointing to "/var/chroot"
Apache will be looking for "/var/chroot/www" inside of your chroot (/var/chroot/www/var/chroot/www)

And if you set ChrootFixRoot to "yes" (are off by default), you dont need to take care about that.

This options fake all apache map to file operation transparantly (like apache mod_alias),
and fake also the CoreDumpDirectory directory.

In that way you can use the real (with DocumentRoot/Alias/UserDir...) path in 
apache configuration file, and this option while translate all real path to relative 
path to chroot directory (/var/chroot/www is translated to /www if chroot dir is /var/chroot).

#### Example1: if you setup your apache like that.
```
	ServerRoot /srv/www1
	DocumentRoot /srv/www1/chroot/htdocs
	ChrootFixRoot on
	# path relative to server root ==> chrootdir is set to /srv/www1/chroot
	ChrootDir chroot 
```

all request to the server like http://mysserver/path/to/mypage while be transparantly translated to
   /htdocs/path/to/mypage because the chroot are /srv/www1/chroot.

#### Example2: if you setup your apache like that.
```
	ServerRoot /srv/www1
	DocumentRoot /srv/www1/htdocs
	ChrootFixRoot on
	# using server root to set chrootdir.
	ChrootDir SERVER_ROOT
```

all request to the server like http://mysserver/path/to/mypage while be transparantly translated to
   /htdocs/path/to/mypage because the chroot are /srv/www1.

#### Example3: if you setup your apache like that.
```
	ServerRoot /srv/www1
	DocumentRoot /srv/www1/htdocs
	ChrootFixRoot on
	# using globale document root to set chrootdir.
	ChrootDir DOCUMENT_ROOT
```

all request to the server like http://mysserver/path/to/mypage while be transparantly translated to
   /path/to/mypage because the chroot are /srv/www1/htdocs.

