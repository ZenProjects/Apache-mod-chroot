#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_main.h"
#include "http_log.h"

#ifdef EAPI
#include "ap_ctx.h"
#include "http_conf_globals.h"
#endif

module MODULE_VAR_EXPORT chroot_module;
#define MODULE_SIGNATURE "mod_chroot/0.2"

typedef struct {
	char *chroot_dir;
} chroot_srv_config;

#ifdef EAPI
typedef struct {
	int initcount;
} chroot_ctx_rec;
#endif

int chroot_init_now() {
#ifndef EAPI
	return (getppid()==1);
#else
chroot_ctx_rec *m;

	m=(chroot_ctx_rec *)ap_ctx_get(ap_global_ctx, "chroot_module");
	if(m->initcount==0) {
		m->initcount=1;
		return 0;
	} else return 1;
#endif
}

void chroot_init_ctx() {
#ifdef EAPI
chroot_ctx_rec *m;
pool *p;

	m=ap_ctx_get(ap_global_ctx, "chroot_module");
	if(m==NULL) {
		p=ap_make_sub_pool(NULL);
		m=ap_palloc(p, sizeof(chroot_ctx_rec));
		m->initcount=0;
		ap_ctx_set(ap_global_ctx, "chroot_module", m);
	}
	return;
#endif
}
	

void chroot_init(server_rec *s, pool *p) {
chroot_srv_config *cfg = (chroot_srv_config *)ap_get_module_config(s->module_config, &chroot_module);

	ap_add_version_component(MODULE_SIGNATURE);
	if(cfg->chroot_dir==NULL) return;
	chroot_init_ctx();
	if(chroot_init_now()) {
		if(chroot(cfg->chroot_dir)<0) {
		 ap_log_error(APLOG_MARK, APLOG_ERR, s, "chroot to %s failed.",
			cfg->chroot_dir);
		 return;
		}
		if(chdir("/")<0) {
		 ap_log_error(APLOG_MARK, APLOG_ERR, s, "chdir to / failed.");
		 return;
		}
#ifndef EAPI
		ap_log_error(APLOG_MARK, APLOG_NOTICE | APLOG_NOERRNO, s,
		"mod_chroot: changed root to %s (getppid() magic).",
		cfg->chroot_dir);
#else
		ap_log_error(APLOG_MARK, APLOG_NOTICE | APLOG_NOERRNO, s,
		"mod_chroot: changed root to %s (EAPI mode).",
		cfg->chroot_dir);
#endif
	}
}

static void *chroot_create_srv_config(pool *p, server_rec *s) {
chroot_srv_config *cfg = (chroot_srv_config *)ap_pcalloc(p, sizeof(chroot_srv_config));
if(cfg==NULL) return NULL;
	chroot_init_ctx();
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

