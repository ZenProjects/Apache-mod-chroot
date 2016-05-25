#ifndef mod_chroot_h
#define mod_chroot_h

#define CORE_PRIVATE
#define AP_MPM_WANT_SET_COREDUMPDIR
#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "scoreboard.h"
#include "unixd.h"
#include "ap_mpm.h"
#include "mpm_common.h"
#include "mod_core.h"
#include "apr_optional.h"
#include "http_core.h"
#include "http_main.h"
#include "http_request.h"

#include <dlfcn.h>
#include "ap_config.h"


#include "apr.h"
#include "apr_pools.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include <unistd.h>


#endif
