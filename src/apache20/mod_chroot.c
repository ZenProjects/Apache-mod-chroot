// ADDED: mca
#define CORE_PRIVATE
#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "scoreboard.h"
#include "unixd.h"
#include "ap_mpm.h"
#include "mpm.h"
#include "http_core.h"
#include "mod_core.h"
#include "apr_optional.h"
#include "http_request.h"

#include <dlfcn.h>
#include "ap_config.h"


#include "apr.h"
#include "apr_pools.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include <unistd.h>

#define MODULE_SIGNATURE "mod_chroot/0.6"
module AP_MODULE_DECLARE_DATA chroot_module;

typedef struct {
	char *chroot_dir;
	uid_t old_user_id;
	const char *ap_document_root;
} chroot_srv_config;

typedef struct {
	int initcount;
} chroot_ctx;

/* create module serveur context */
static void *chroot_create_srv_config(apr_pool_t *p, server_rec *s) {
	chroot_srv_config *cfg = apr_pcalloc(p, sizeof(chroot_srv_config));
	
	cfg->old_user_id=0;
	cfg->ap_document_root=NULL;
	cfg->chroot_dir=NULL;
	return cfg;
}

/* set chroot dir value */
static const char *cmd_chroot_dir(cmd_parms *cmd, void *dummy, const char *p1) {
	chroot_srv_config *cfg = (chroot_srv_config *)ap_get_module_config(cmd->server->module_config, &chroot_module);
	const char *err;

	if((err=ap_check_cmd_context(cmd, GLOBAL_ONLY))) return err;
	if(strncasecmp("auto",p1,4)!=0&&!ap_is_directory(cmd->pool, p1)) {
		return "Chroot to something that is not a directory";
	}
	cfg->chroot_dir=(char *)p1;
	return NULL;
}

/* post config check */
static int post_config_check(server_rec *s) {
	apr_pool_t *pool=s->process->pool;
	chroot_ctx *c;

	apr_pool_userdata_get((void **)&c, "chroot_module_ctx", pool);
	if(c) { return (c->initcount); }
	c = (chroot_ctx *)apr_palloc(pool, sizeof(*c));
	c->initcount=1;
	apr_pool_userdata_set(c, "chroot_module_ctx", apr_pool_cleanup_null,
			pool);
	return 0;
}

/* pre_mpm hook */
static int chroot_pre_mpm(apr_pool_t *p, ap_scoreboard_e sb_type) 
{
  chroot_srv_config *cfg = (chroot_srv_config *)ap_get_module_config(ap_server_conf->module_config, &chroot_module);

  // save user defined in apache
  if (cfg->old_user_id==0) cfg->old_user_id=unixd_config.user_id;

  ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, "mod_chroot: old uid is %d.",unixd_config.user_id);

  // set user_id = 0 to fake unixd_setup_child made before child_init hook 
  // to make possible to do chroot in child_init hook after
  unixd_config.user_id=0;

  ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, "mod_chroot: new uid is set, fake root uid in place");

  return OK;
}

/* post_config hook */
static int chroot_post_config(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s)
{
   ap_add_version_component(p, MODULE_SIGNATURE);


   if(post_config_check(s)==1) {
     chroot_srv_config *cfg = (chroot_srv_config *)ap_get_module_config(s->module_config, &chroot_module);
     core_server_config *corecfg = ap_get_module_config(s->module_config, &core_module);

     ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, "mod_chroot: new uid is set, fake root uid in place");

     ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, "mod_chroot: chroot is <%s>.",cfg->chroot_dir);
     ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, "mod_chroot: documentroot is <%s>.",corecfg->ap_document_root);

     if (cfg->chroot_dir==NULL||strncasecmp("auto",cfg->chroot_dir,4)==0)
     {
	cfg->chroot_dir=apr_pstrdup(p,corecfg->ap_document_root);
	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, "mod_chroot: chroot is set to <%s>.",cfg->chroot_dir);
     }
     
     // make correction of the apache document root if necessary
     // 
     long docroot_len=strlen(corecfg->ap_document_root);
     long chroot_len=strlen(cfg->chroot_dir);

     // when chroot_dir is egual to docroot => new docroot is "/"
     if (strcmp(cfg->chroot_dir,corecfg->ap_document_root)==0)
     {
       cfg->ap_document_root=apr_pstrdup(p,"/");
       ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, "mod_chroot: documentroot changed to <%s>.",cfg->ap_document_root);
     }
     // when docroot is a sub directory of the chroot => the new docroot is the docroot sub directory part of chroot 
     // if chroot is /srv/www and docroot is /srv/www/htdocs the new doc is /htdocs
     else if (strncmp(cfg->chroot_dir,corecfg->ap_document_root,chroot_len)==0)
     {
       cfg->ap_document_root=apr_pstrdup(p, (cfg->chroot_dir)+docroot_len);
       ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, "mod_chroot: documentroot changed to <%s>.",cfg->ap_document_root);
     }
     // if chroot is outside of the docroot
     else
     {
	ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "mod_chroot: Error document root directory outside of chroot directory chroot:<%s> documentroot:<%s>.",cfg->chroot_dir,corecfg->ap_document_root);
        return DECLINED;
     }
   }

   return OK;
}

/* translate_name hook */
static int chroot_translate_name(request_rec *r)
{
   apr_table_t *e= r->subprocess_env;
   
   chroot_srv_config *cfg = (chroot_srv_config *)ap_get_module_config(r->server->module_config, &chroot_module);
   core_server_config *corecfg = ap_get_module_config(r->server->module_config, &core_module);

   ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "mod_chroot: force documentroot to <%s>.",corecfg->ap_document_root);

   // correct docroot in server config
   corecfg->ap_document_root=cfg->ap_document_root;
   // correct server side env DOCUMENT_ROOT
   apr_table_setn(e, "DOCUMENT_ROOT", cfg->ap_document_root);
   return OK;
}

/* child_init hook */
static void chroot_child_init(apr_pool_t *p, server_rec *s) 
{
    chroot_srv_config *cfg = (chroot_srv_config *)ap_get_module_config(s->module_config, &chroot_module);

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, "mod_chroot: chroot to '%s'",cfg->chroot_dir);

    // chroot child
    if(chroot(cfg->chroot_dir)<0) {
	    ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "could not chroot to <%s>.",cfg->chroot_dir);
	    return;
    }
    // go on /
    if(chdir("/")<0) {
	    ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "could not chdir to '/'.");
	    return;
    }
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, "mod_chroot: chrooted to <%s> - ok.",cfg->chroot_dir);

    // make the real unixd_setup_child
    unixd_config.user_id=cfg->old_user_id;
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, "mod_chroot: set uid to %d.",unixd_config.user_id);
    if (unixd_setup_child()) {
	exit(APEXIT_CHILDFATAL);
    }
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, "mod_chroot: unixd_setup_child occured");

}

static void register_hooks(apr_pool_t *p) {
        static const char * const chrootPre[] = { 
		"http_core.c", 
		NULL 
	};
        ap_hook_pre_mpm(chroot_pre_mpm, chrootPre, NULL, APR_HOOK_REALLY_LAST);	
        ap_hook_child_init(chroot_child_init, NULL, NULL, APR_HOOK_REALLY_FIRST);	
	ap_hook_post_config(chroot_post_config, chrootPre, NULL, APR_HOOK_REALLY_LAST);	
	ap_hook_translate_name(chroot_translate_name, NULL, NULL, APR_HOOK_MIDDLE);	
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

