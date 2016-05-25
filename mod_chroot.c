#include <httpd.h>

#if defined(APACHE_RELEASE) && APACHE_RELEASE>10300000 \
	&& APACHE_RELEASE<10399999
#include "src/apache13/mod_chroot.c"
#define MODCHROOT_APACHE_VERSION 13
// The line below makes apxs bootstrap symbol name detection work
// module MODULE_VAR_EXPORT chroot_module = {
#endif

#if !defined(MODCHROOT_APACHE_VERSION) && defined(AP_SERVER_BASEVERSION)
#include "src/apache20/mod_chroot.c"
// The line below makes apxs bootstrap symbol name detection work
// module AP_MODULE_DECLARE_DATA chroot_module = {
#define MODCHROOT_APACHE_VERSION 20
#endif

#if !defined(MODCHROOT_APACHE_VERSION)
#error Could not detect your Apache version. Please report a bug.
#endif

