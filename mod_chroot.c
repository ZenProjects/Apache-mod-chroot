#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_main.h"
#include "http_log.h"

module MODULE_VAR_EXPORT chroot_module;
#define MODULE_SIGNATURE "mod_chroot/0.1"

typedef struct {
	char *chroot_dir;
} chroot_srv_config;

void chroot_init(server_rec *s, pool *p) {
chroot_srv_config *cfg = (chroot_srv_config *)ap_get_module_config(s->module_config, &chroot_module);

	ap_add_version_component(MODULE_SIGNATURE);
	if(cfg->chroot_dir==NULL) return;
	if(getppid()==1) {
		if(chroot(cfg->chroot_dir)<0) {
		 ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, s, 
		  "mod_chroot: chroot to %s failed: %s", cfg->chroot_dir, strerror(errno));
		 return;
		}
		if(chdir("/")<0) {
		 ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, s,
		  "mod_chroot: chdir to / failed: %s", strerror(errno));
		 return;
		}
		ap_log_error(APLOG_MARK, APLOG_NOTICE | APLOG_NOERRNO, s, "mod_chroot: changed root to %s",
		  cfg->chroot_dir);
	}
}

static void *chroot_create_srv_config(pool *p, server_rec *s) {
chroot_srv_config *cfg = (chroot_srv_config *)ap_pcalloc(p, sizeof(chroot_srv_config));
if(cfg==NULL) return NULL;

	cfg->chroot_dir=NULL;
	return cfg;
}

static const char *cmd_chroot_dir(cmd_parms *cmd, void *dummy, char *p1) {
	    chroot_srv_config *cfg = (chroot_srv_config *)ap_get_module_config(cmd->server->module_config, &chroot_module);
	    	if(!ap_is_directory(p1))
			return "Chroot to something that is not a directory";
	        cfg->chroot_dir = p1;
		return NULL;
}


static const command_rec chroot_cmds[] = {
	{
	 "ChrootDir",
	 cmd_chroot_dir,
	 NULL,
	 RSRC_CONF,
	 TAKE1,
	 "path to a new root directory"
	},
	{ NULL }
};

	
module MODULE_VAR_EXPORT chroot_module = {
    STANDARD_MODULE_STUFF,
    chroot_init,
    NULL,			/* create per-dir    config structures */
    NULL,		        /* merge  per-dir    config structures */
    chroot_create_srv_config,   /* create per-server config structures */
    NULL,                       /* merge  per-server config structures */
    chroot_cmds,                   /* table of config file commands       */
    NULL,                       /* [#8] MIME-typed-dispatched handlers */
    NULL,                       /* [#1] URI to filename translation    */
    NULL,                       /* [#4] validate user id from request  */
    NULL,                       /* [#5] check if the user is ok _here_ */
    NULL,                       /* [#3] check access by host address   */
    NULL,                       /* [#6] determine MIME type            */
    NULL,	                /* [#7] pre-run fixups                 */
    NULL,                       /* [#9] log a transaction              */
    NULL,                       /* [#2] header parser                  */
    NULL,                       /* child_init                          */
    NULL,                       /* child_exit                          */
    NULL                        /* [#0] post read-request              */
};

