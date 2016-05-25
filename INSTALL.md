# mod_chroot

## Requirement

   in order to build this module you need:
   - apxs (shipped as apxs2 by some distributors) and Apache
     headers. If you compiled Apache from source you already have these
     headers. If you use Debian, you need to install apache2-prefork-dev or
     apache2-threaded-dev.
   - Make
   - C compiler (ex:gcc).

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

"DOCUMENT_ROOT" => set the chroot to the globale apache document root value.
             (to use this you must set documentroot before ChrootDir cmd).

"SERVER_ROOT" => set the chroot to the apache server root.
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

