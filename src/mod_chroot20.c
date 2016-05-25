#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"

#include "apr.h"
#include "apr_pools.h"

#define MODULE_SIGNATURE "mod_chroot/0.3"
module AP_MODULE_DECLARE_DATA chroot_module;

typedef struct {
	char *chroot_dir;
} chroot_srv_config;

typedef struct {
	int initcount;
} chroot_ctx;

int chroot_init_now(server_rec *s) {
	apr_pool_t *pool=s->process->pool;
	chroot_ctx *c;

	apr_pool_userdata_get((void **)&c, "chroot_module_ctx", pool);
	if(c) {
		return (c->initcount++);
	}
	c = (chroot_ctx *)apr_palloc(pool, sizeof(*c));
	c->initcount=1;
	apr_pool_userdata_set(c, "chroot_module_ctx", apr_pool_cleanup_null,
			pool);
	return 0;
}

static void *chroot_create_srv_config(apr_pool_t *p, server_rec *s) {
	chroot_srv_config *cfg = apr_pcalloc(p, sizeof(chroot_srv_config));
	
	cfg->chroot_dir=NULL;
	return cfg;
}

static const char *cmd_chroot_dir(cmd_parms *cmd, void *dummy, const char *p1) {
	chroot_srv_config *cfg = (chroot_srv_config *)ap_get_module_config(cmd->server->module_config, &chroot_module);
	const char *err;

	if((err=ap_check_cmd_context(cmd, GLOBAL_ONLY))) return err;
	if(!ap_is_directory(cmd->pool, p1)) {
		return "Chroot to something that is not a directory";
	}
	cfg->chroot_dir=(char *)p1;
	return NULL;
}

static int chroot_init(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s) {
chroot_srv_config *cfg = (chroot_srv_config *)ap_get_module_config(s->module_config, &chroot_module);

	ap_add_version_component(p, MODULE_SIGNATURE);
	if(cfg->chroot_dir==NULL) return OK;
	if(chroot_init_now(s)==1) {
		if(chroot(cfg->chroot_dir)<0) {
			ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "could not chroot to %s.",cfg->chroot_dir);
			return !OK;
		}
		if(chdir("/")<0) {
			ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "could not chdir to '/'.");
			return !OK;
		}
		ap_log_error(APLOG_MARK, APLOG_NOTICE|APLOG_NOERRNO, 0, s, "mod_chroot: changed root to %s.",cfg->chroot_dir);
		return OK;
	}
	else return OK;
}

static void register_hooks(apr_pool_t *p) {
	ap_hook_post_config(chroot_init, NULL, NULL, APR_HOOK_REALLY_LAST);
}

static const command_rec chroot_cmds[] = {

	AP_INIT_TAKE1 (
		"ChrootDir",
		cmd_chroot_dir,
		NULL,
		RSRC_CONF,
		"path to a new root directory"
	),
	{ NULL }
};

module AP_MODULE_DECLARE_DATA chroot_module = {
   STANDARD20_MODULE_STUFF,
   NULL,			 /* create per-dir    config structures */
   NULL,		         /* merge  per-dir    config structures */
   chroot_create_srv_config,     /* create per-server config structures */
   NULL,                         /* merge  per-server config structures */
   chroot_cmds,                  /* table of config file commands       */
   register_hooks                /* register hooks                      */
};

