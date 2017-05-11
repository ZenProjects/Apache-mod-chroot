[![Build Status](https://travis-ci.org/ZenProjects/Apache-mod-chroot.svg?branch=master)](https://travis-ci.org/ZenProjects/Apache-mod-chroot)
[![CircleCI](https://circleci.com/gh/ZenProjects/Apache-mod-chroot.svg?style=shield)](https://circleci.com/gh/ZenProjects/Apache-mod-chroot)
[![License: GPL v3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](http://www.gnu.org/licenses/gpl-3.0)

# mod_chroot 

## What is it?

mod_chroot makes running Apache in a secure chroot environment easy. You
don't need to create a special directory hierarchy containing /dev, /lib,
/etc...

This project are major rewrite of mod_chroot by hobbit at http://core.segfault.pl/~hobbit/mod_chroot/ focused on apache 2 only (removed Apache 1 support).

*Note: From 2.2.10 of apache they include [ChrootDir](https://httpd.apache.org/docs/2.2/mod/mpm_common.html#chrootdir) that are similar but not so simple to use as this version of mod_chroot.*

## Why chroot?

For security.

chroot(2) changes the root directory of a process to a directory other
than "/". It means the process is locked inside a virtual filesystem root.
If you configure your chroot jail properly, Apache and its child processes
(think CGI scripts) won't be able to access anything except the jail.

A non-root process is not able to leave a chroot jail. Still it's not wise
to put device files, suid binaries or hardlinks inside the jail.

## Chroot - the hard way

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

## Chroot - the mod_chroot way

**mod_chroot** allows you to run Apache in a chroot jail with no additional
files. The **chroot()** system call is performed at the end of startup
procedure - *when all libraries are loaded and log files open*. 

## Major change between 0.x and 1.x version:

Starting from version 0.3 **mod_chroot** supports apache 2.0.
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
that reason restarting Apache with `apachectl reload`, `apachectl
graceful` or `kill -HUP apache_pid` will not work as expected. Apache will
not be able to read its config file, open logs or load modules.

Starting with the 1.0 of **mod_chroot** solve all this probleme. 

- In first problem are resolved by placing the **chroot** in **child_init phase**, after the mpm initialisation,
  in that way they don't need to place pid file, scoreboard, Lock File inside the jail.

- And the seconds probleme are command like `DocumentRoot` and `CoreDumpDirectory` that are tested at start of the server, 
  that make oblige to point to real directory outside the chroot and in the chroot... 

  This proleme is resolved by faking all map to storage transparantly like mod_alias (by setting `ChrootFixRoot` option).
  This also fake the `CoreDumpDirectory` directory.

in that way using **mod_chroot** is simple like to load the module and set the chroot dir...
the only constraint are to set document root and coredump directory in the path of the chroot dir...

Ex. this are ok:
```
ChrootDir         /srv/www1
DocumentRoot      /srv/www1/htdocs
CoreDumpDirectory /srv/www1/dump
```

Ex. this are not ok:
```
ChrootDir         /srv/www1
DocumentRoot      /htdocs
CoreDumpDirectory /dump
```

# Building 

## Requirement

in order to build this module you need:

- **apxs** (shipped as apxs2 by some distributors) and Apache
  headers. If you compiled Apache from source you already have these
  headers. If you use Debian, you need to install apache2-prefork-dev or
  apache2-threaded-dev.
- **Make**
- **C** compiler (ex:gcc).

*Warning: starting 1.0 they support only Apache 2.x.*

## Building mod_chroot

1 - Go to **mod_chroot** source directory and type:

```	
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

**mod_chroot** provides two configuration directive:

### ChrootDir: 

It can only be used in main server configuration. You can't put ChrootDir inside
a `<Directory>`, `<Files>`, `<Location>`, `<VirtualHost>` sections or `.htaccess`
files. 

They define the **chroot virtual path base**.

**Example:** *if you store your www files in **/var/www** and you want to
make it as `virtual root`, set **ChrootDir** this path.* 
      
#### You can use apache server root relative path.

**Example:** 
```
ChrootDir chroot
```

if the server root is /srv/www1 and ChrootDir is set to "chroot" while point to /srv/www1/chroot directory

#### you can use some hints to set the chroot directory to value with apache internal value:

- `DOCUMENT_ROOT` => set the **chroot virtual path base** to the globale apache **document root** value. 
  (to use this you must set **document root** before **ChrootDir** cmd).

- `SERVER_ROOT` => set the **chroot virtual path base** to the apache **server root**.
  (`-d` args of `httpd`, or `ServerRoot` apache directive)

**Example:** 
```
ChrootDir DOCUMENT_ROOT
```
to set the **chroot** to the apache **document root**.

### ChrootFixRoot: 

It can only be used in main server configuration. You can't put ChrootDir inside
a `<Directory>`, `<Files>`, `<Location>`, `<VirtualHost>` sections or `.htaccess`
files. 

The `DocumentRoot` (global config context) apache commande verify if directory exist 
before the chroot is activated, in that way you cannot use `DocumentRoot` (global config context) 
relative to **chroot** (`/www` in place `/var/chroot/www` if **chroot** is `/var/chroot`). 

**Note:** *if you not set global config context `DocumentRoot` apache set it to default value (define at compile time) 
and check it in the same way.*

**Exemple:** If you had `DocumentRoot` pointing to `/var/chroot/www` and `ChootDir` pointing to `/var/chroot`
Apache will be looking for `/var/chroot/www` inside of your chroot (`/var/chroot/www/var/chroot/www`)

And if you set `ChrootFixRoot` to `yes` (are `off` by default), you dont need to take care about that.

This options fake all apache map to file operation transparantly (like apache **mod_alias**),
and fake also the `CoreDumpDirectory` directory.

In that way you can use the real (with `DocumentRoot`/`Alias`/`UserDir`...) path in 
apache configuration file, and this option while translate all real path to relative 
path to chroot directory (`/var/chroot/www` is translated to `/www` if **chroot** dir is `/var/chroot`).

#### Example 1: if you setup your apache like that.
```
ServerRoot /srv/www1
DocumentRoot /srv/www1/chroot/htdocs
ChrootFixRoot on
# path relative to server root ==> chrootdir is set to /srv/www1/chroot
ChrootDir chroot 
```

all request to the server like `http://mysserver/path/to/mypage` while be transparantly translated to
   `/htdocs/path/to/mypage` because the chroot are `/srv/www1/chroot`.

#### Example 2: if you setup your apache like that.
```
ServerRoot /srv/www1
DocumentRoot /srv/www1/htdocs
ChrootFixRoot on
# using server root to set chrootdir.
ChrootDir SERVER_ROOT
```

all request to the server like `http://mysserver/path/to/mypage` while be transparantly translated to
   `/htdocs/path/to/mypage` because the chroot are `/srv/www1`.

#### Example 3: if you setup your apache like that.
```
ServerRoot /srv/www1
DocumentRoot /srv/www1/htdocs
ChrootFixRoot on
# using globale document root to set chrootdir.
ChrootDir DOCUMENT_ROOT
```

all request to the server like `http://mysserver/path/to/mypage` while be transparantly translated to
   `/path/to/mypage` because the chroot are `/srv/www1/htdocs`.

# CAVEATS

## Date Timezone database

Every time you use the **date function** in the chroot jail, you while get an error saying the **timezone db is corrupt**. Is why you need **localtime** and **zoneinfo** file in the chroot jail.

```
# mkdir -p /chroot/etc
# cp /etc/localtime /chroot/etc/localtime
# mkdir -p /chroot/usr/share
# cp /usr/share/zoneinfo /chroot/usr/share/zoneinfo
```

## DNS lookups

Libresolv uses /etc/resolv.conf to find your DNS server. If this file
doesn't exist, libresolv uses `127.0.0.1:53` as the DNS server. You can run
a small caching server listening on `127.0.0.1` (which may be a good idea
anyway), or use your operating system's firewall to transparently redirect
queries to `127.0.0.1:53` to your real DNS server. 

Note that this is only necessary if you do DNS lookups - *probably this can be avoided?*

You may need to copy `/etc/nsswitch.conf` and `/etc/resolv.conf` in **chroot** jail :
```
# mkdir -p /chroot/etc
# cp /etc/nsswitch.conf /chroot/etc/nsswitch.conf
# cp /etc/resolv.conf /chroot/etc/resolv.conf
```

Please also read the libraries section below because libc resolver load some additional dymanics library at first DNS resolution.

If you whant to use [**nscd**](https://linux.die.net/man/8/nscd) in the **chroot** jail you must map **nscd** socket in the chroot jail.

```
# mkdir -p /chroot/var/run
# mount -o bind /var/run/nscd /chroot/var/run/nscd
```

## Databases

If your mySQL/PostgreSQL accepts connections on a Unix socket which is outside of your **chroot** jail, reconfigure it to listen on a loopback address (127.0.0.1).

With mysql if you try to connect local db you must use `127.0.0.1` in place of `localhost` in connect string, without that mysql client try to use mysql unix sockets in place of using tcp, without that in the **chroot** jail you cannot connect to db because the socket are generaly outside of the **chroot**.

You can also map socket in the **chroot** jail with `mount bind` like that :
```
# mkdir -p /chroot_dir/path/to/mysql/sockets/
# mount -o bind /path/to/mysql/sockets/ /chroot_dir/path/to/mysql
```

## PHP mail() function

Under Unix, PHP requires a sendmail binary to send mail. Putting this file
inside your **chroot** jail may not be sufficient, you would probably need to move
your mail queue as well. 

To avoid that you have three options here:
* Don't use mail()! Use a class/function that knows how to send directly
  via SMTP :
  - [Pear's Mail](http://pear.php.net/package/Mail),
  - [Swiftmailer](http://swiftmailer.org/),
* Use [esmtp](https://pecl.php.net/package/esmtp) pecl module based on [libesmtp](https://launchpad.net/ubuntu/+source/libesmtp).
* Install a SMTP-only sendmail clone like [sSMTP](https://tracker.debian.org/pkg/ssmtp) or [mini_sendmail](http://acme.com/software/mini_sendmail/). 
  You can then put a single binary inside your jail, and deliver mail via a smarthost *(warning see PHP Exec function section beceause php need /bin/sh to execute sendmail clone in the chroot jail)*,

## PHP **exec** Functions

PHP `system()`, `exec()`, `shell_exec()`, ou `proc_open()` execution function uses `/bin/sh -c` internally. Beceause of that you need to include `/bin/sh` in chroot jail to use that function in PHP.

Try to use [busybox](https://busybox.net/) or [toybox](http://www.landley.net/toybox/) minimalist and staticly linked shell (no need other thing that the executable in chroot jail), or [dash](https://en.wikipedia.org/wiki/Almquist_shell) light posix shell (that need few dependancy in the chroot jail).

Instead of placing a static `sh` or copying libs there, [knzl](https://knzl.de/setting-up-a-chroot-for-php/) had hacked up a small wrapper that supports being called with `-c command` to launch sendmail. It doesn’t support escaping of parameters with quotation marks, but that’s not necessary. It’s called `sh.c`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
 
#define MAXARG 64
 
int main( int argc, char* const argv[] ) {
    char* args[ MAXARG ] = {};
 
    if( argc < 3 || strcmp( argv[1], "-c" ) != 0 ) {
        fprintf( stderr, "Usage: %s -c <cmd>\n", argv[0] );  
        return 1;
    }
 
    {
        char* token;
        int i = 0;  
        char* argStr = strdup( argv[2] );
        while( ( token = strsep( &argStr, " " ) ) != NULL ) {
            if( token && strlen( token ) )
                args[ i++ ] = token;
            if( i >= MAXARG )
                return 2;
        }
    }  
 
    return execvp( args[0], args );
}
```

Compile it by calling

```
# gcc sh.c -o sh -static
```

and place it as `bin/sh` in the chroot.

## PHP iconv

iconv() function need `/usr/lib/x86_64-linux-gnu/gconv` that must be in chroot, and also `/usr/lib/locale`. without that you may have `PHP Notice: iconv(): Wrong charset, conversion from `UTF-8' to `CP1252' is not allowed` in log.

## Shared libraries

Shared libraries are libraries which are linked to a program at run-time.
Nowadays, most programs require some shared libraries to run - libc.so is
most common. You can see a list of shared libraries a program requires by
running ldd /path/to/program. Loading of these libraries is done
automagically by ld.so at startup. **mod_chroot** doesn't interfere with this
mechanism.

A program may also explicitly load a shared library by calling dlopen()
and dlsym(). This might cause troubles in a chrooted environment - after a
process is chrooted, libraries (usually stored in /lib) might be no longer
accessible. This doesn't happen very often, but if it does - there is a
solution: you can preload these libraries before chrooting. Apache has a
handy directive for that: [LoadFile](https://httpd.apache.org/docs/2.4/mod/mod_so.html#loadfile). 

### For exemple:

* DNS lookups - GNU libc tries to load libnss_dns.so.2 and some time libnss_files.so.2 when a first DNS
lookup is done. Solution:

```
 LoadFile /lib/libnss_dns.so.2
 LoadFile /lib/libnss_files.so.2
```

* Apache 2.0 with mpm_worker on Linux 2.6 - GNU libc tries to load
  libgcc_s.so.1 when pthread_cancel is called. Solution:

```
 LoadFile /lib/libgcc_s.so.1
```

* Curl need to preload nss library and trusted certificat or may occur `Problem with the SSL CA cert (path? access rights?)` error :

```
  LoadFile /usr/lib64/libsoftokn3.so
  LoadFile /usr/lib64/libnsssysinit.so
  LoadFile /usr/lib64/libnsspem.so
```

and map trusted store in the chroot jail with mount bind :
```
# mkdir -p /chroot/etc/pki/tls
# mount -o bind /etc/pki/tls        /chroot/etc/pki/tls       
# mkdir -p /chroot/etc/pki/ca-trust
# mount -o bind /etc/pki/ca-trust/  /chroot/etc/pki/ca-trust
```

## To determine what librarie they need to preload before chroot or to include in the chroot jail

### Use `ldd` to see library linked with an executable:
```
# ldd /bin/sh
        linux-vdso.so.1 =>  (0x00007ffe65357000)
        libtinfo.so.5 => /lib64/libtinfo.so.5 (0x0000003444e00000)
        libdl.so.2 => /lib64/libdl.so.2 (0x0000003435200000)
        libc.so.6 => /lib64/libc.so.6 (0x0000003434e00000)
        /lib64/ld-linux-x86-64.so.2 (0x0000003434a00000)
```

### When executing : 
```
# strace -e trace=file /bin/sh 2>&1  | egrep "open|stat"
open("/etc/ld.so.cache", O_RDONLY)      = 3
open("/lib64/libtinfo.so.5", O_RDONLY)  = 3
open("/lib64/libdl.so.2", O_RDONLY)     = 3
open("/lib64/libc.so.6", O_RDONLY)      = 3
open("/dev/tty", O_RDWR|O_NONBLOCK)     = 3
open("/usr/lib/locale/locale-archive", O_RDONLY) = 3
open("/proc/meminfo", O_RDONLY|O_CLOEXEC) = 3
stat("/mnt/www/etc", {st_mode=S_IFDIR|0755, st_size=4096, ...}) = 0
stat(".", {st_mode=S_IFDIR|0755, st_size=4096, ...}) = 0
open("/usr/lib64/gconv/gconv-modules.cache", O_RDONLY) = 3
```

### Or on already running executable use `strace -p <pid>[,<pid>,...]` (a running process pool) :
```
#  strace  -p $(ps -ef | grep <process pool name> | awk '{a[i++]=$2}END{printf(a[0]);for(n=1;n<i;n++)printf(","a[n])}') 2>&1 | egrep "open|stat"
```
