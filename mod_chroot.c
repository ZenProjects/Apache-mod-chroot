#include "mod_chroot.h"

#define MODULE_SIGNATURE "mod_chroot/1.0.1"
module AP_MODULE_DECLARE_DATA chroot_module;

typedef struct {
	server_rec *s;
	apr_pool_t *pool;
	char *chroot_dir;
	uid_t old_user_id;
	char *ap_document_root;
	int flag_fixroot;
} chroot_srv_config;

// function to fix directory to be in chroot if necessary
// used to fix document root or coredumpdirectory
static int chroot_dirtofix(apr_pool_t *p, server_rec *s, char **directory_to_fix, char *chroot_dir, int flag)
{
   char *my_dir_to_fix=*directory_to_fix;
   long chroot_len=strlen(chroot_dir);

   if (my_dir_to_fix==NULL) 
   {
     ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "chroot_dirtofix: directory to fix is null...");
     return HTTP_BAD_REQUEST;
   }
   ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
           "chroot_dirtofix[%d]: actual directory to fix:<%s>.",getpid(),my_dir_to_fix);

   // when chroot_dir is egual to docroot => new docroot is "/"
   if (strcmp(chroot_dir,*directory_to_fix)==0)
   {
     my_dir_to_fix=apr_pstrdup(p,"/");
     ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
             "chroot_dirtofix[%d]: equal, directory to fix changed to <%s>.",getpid(),my_dir_to_fix);
   }
   // when docroot is a sub directory of the chroot => the new docroot is the docroot sub directory part of chroot 
   // if chroot is /srv/www and docroot is /srv/www/htdocs the new doc is /htdocs
   else if (strncmp(chroot_dir,*directory_to_fix,chroot_len)==0)
   {
     my_dir_to_fix=apr_pstrdup(p, (char*)&my_dir_to_fix[chroot_len]);
     ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
             "chroot_dirtofix[%d]: are in chroot, directory to fix changed to <%s>.",getpid(),my_dir_to_fix);
   }
   else if (flag)
   {
     my_dir_to_fix=apr_pstrdup(p, "/");
     ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
             "chroot_dirtofix[%d]: are outside the chroot, directory to fix forced to <%s>.",getpid(),my_dir_to_fix);
   }
   *directory_to_fix=my_dir_to_fix;
   return OK;
}

/* create module serveur context: init module globale value */
static void *chroot_create_srv_config(apr_pool_t *p, server_rec *s) {
	chroot_srv_config *cfg = apr_pcalloc(p, sizeof(chroot_srv_config));
	
	cfg->pool=p;
	cfg->s=s;
	cfg->old_user_id=-1;
	cfg->ap_document_root=NULL;
	cfg->chroot_dir=NULL;
	cfg->flag_fixroot=0;
	return cfg;
}

/*******************************************************************************************************
 all path transmited to the script are generated from apache documentroot.
 the first objective is to avoid the double path globale documentroot (because apache check if documentroot is directory when read conf) tweak:
   without the fix: if chroot are /srv/chroot/www and documentroot are /srv/chroot/www ou must have /srv/chroot/www/srv/chroot/www directory the script see the documentroot set to /srv/chroot/www.
   with the fix: if chroot are /srv/chroot/www and documentroot are /srv/chroot/www your documentroot are realy /srv/chroot/www directory, and the script in the jail see documentroot set to /www (not /srv/chroot/www).
 the objective is to make transparent to the script executed in the jail in fixing transparently apache documentroot
 and also fix filename in case of alias/userdir/vhost_alias/vhost... and all server variable are set in the chroot...
 this fix are activable with "ChrootFixRoot" option.
 *******************************************************************************************************/

/* fix filename if is in chroot path after mod_alias, mod_userdir, mod_vhost_alias as done her jobs... */
/* without that this modules dont work correctly if not use double path work around */
static int chroot_map_to_storage(request_rec *r)
{
   if (r->proxyreq) {
       /* someone has already set up the proxy, it was possibly ourselves
	* in proxy_detect
	*/
       return DECLINED;
   }
   if (r->uri[0] != '/' && r->uri[0] != '\0') {
      return DECLINED;
   }

   chroot_srv_config *cfg = (chroot_srv_config *)ap_get_module_config(r->server->module_config, &chroot_module);
   if (cfg->flag_fixroot)
   {
     if (cfg->chroot_dir!=NULL)
     {
       long chroot_len=strlen(cfg->chroot_dir);
   ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, 
          "chroot_map_to_storage[%d]: filename:%s uri:%s unparsed_uri:%s canonical_filename:%s server_root:%s.",
                 getpid(),r->filename,r->uri,r->unparsed_uri,r->canonical_filename,ap_server_root);
       if (strncmp(r->filename,cfg->chroot_dir,chroot_len)==0)
       {
	  r->filename=apr_pstrdup(r->pool,(char*)&r->filename[chroot_len]);
	  ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "chroot_map_to_storage: file are in chroot, fixit to:<%s>.",r->filename);
       }
     }
   }
   return DECLINED; // let a chance to do default map_to_storage
}

/* translate_name hook: ap_document_root are replace a each request, 
   this fix this documentroot a each request before other module use 
   translate_name hook this fix $_SERVER["DOCUMENT_ROOT"] in php to the chroot
*/
static int chroot_translate_name(request_rec *r)
{
   if (r->proxyreq) {
       /* someone has already set up the proxy, it was possibly ourselves
	* in proxy_detect
	*/
       return DECLINED;
   }
   if (r->uri[0] != '/' && r->uri[0] != '\0') {
      return DECLINED;
   }

   chroot_srv_config *cfg = (chroot_srv_config *)ap_get_module_config(r->server->module_config, &chroot_module);
   ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, 
          "chroot_translate_name[%d]: filename:%s uri:%s unparsed_uri:%s canonical_filename:%s.",
                 getpid(),r->filename,r->uri,r->unparsed_uri,r->canonical_filename);
   if (cfg->flag_fixroot)
   {
     if (cfg->chroot_dir!=NULL)
     {
       core_server_config *corecfg = ap_get_module_config(r->server->module_config, &core_module);
       // correct documentroot if the option is set
       if (cfg->flag_fixroot)
       {
	 if (chroot_dirtofix(r->server->process->pconf, r->server, (char**)&corecfg->ap_document_root, cfg->chroot_dir,FALSE)!=OK) 
	 {
	    ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, 
	          "chroot_translate_name[%d]: could not fix ap_document_root <%s>.",getpid(),(char * )ap_document_root);
	    return DECLINED;
	 }
	 ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, 
	     "chroot_translate_name[%d]: new translated documentroot set to:<%s>",
	      getpid(),corecfg->ap_document_root);
       }
     }
   }
   return DECLINED;
}


/************************************************************************
 *   pre_mpm & child_init hack to make possible to chroot in child_init *
 ************************************************************************
 the idea is to chroot in child_init hook to minimise the impact of the chroot.
 but to child_init hook is call after unixd_setup_child as setted the user_id.
 and is only possible to call chroot whith uid 0.
 to make possible to are uid 0 in child_init hook (without modify apache code), 
 is to hack the unixd_config.user_id (save old value and set to uid "root") before forking child but after user_id is set.
 the pre_mpm hook is the hook more near from unixd_setup_child call and juste before child are forked.
 there no other hook before child_init than pre_mpm.
*/
/* pre_mpm hook: save the user_id and set it to 0  */
static int chroot_pre_mpm(apr_pool_t *p, ap_scoreboard_e sb_type) 
{
  chroot_srv_config *cfg = (chroot_srv_config *)ap_get_module_config(ap_server_conf->module_config, &chroot_module);

  // save user defined in apache configuration
  if (cfg->old_user_id==-1) {
#if MODULE_MAGIC_NUMBER_MAJOR > 20051115
     cfg->old_user_id=ap_unixd_config.user_id;
#else
     cfg->old_user_id=unixd_config.user_id;
#endif
     ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, "chroot_pre_mpm: store old uid=%d.",cfg->old_user_id);
  }

  if (cfg->chroot_dir!=NULL)
  {
    core_server_config *corecfg = ap_get_module_config(ap_server_conf->module_config, &core_module);
    // set user_id = 0 to fake unixd_setup_child made before child_init hook 
    // to make possible to do chroot in child_init hook after
#if MODULE_MAGIC_NUMBER_MAJOR > 20051115
    ap_unixd_config.user_id=0;
#else
    unixd_config.user_id=0;
#endif
    ap_add_version_component(p, MODULE_SIGNATURE); // add the mod_chroot in the signature...
    ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, 
             "chroot_pre_mpm: configured chroot dir are <%s>.",cfg->chroot_dir);
    ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, 
             "chroot_pre_mpm: configured documentroot are <%s>.",corecfg->ap_document_root);
    ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, 
             "chroot_pre_mpm: configured server_root are <%s>.",ap_server_root);
    ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, 
             "chroot_pre_mpm: new unixd_config.user_id are set, uid root hack are in place");
  }

  return OK;
}

/* child_init hook: the perfect place to make chroot... 
   with the hack you are root(uid=0) in this phase...
   and your are abel to chroot and after re-establish saved uid/gid to make impossible to not go out of the chroot jail...
   */
static void chroot_child_init(apr_pool_t *p, server_rec *s) 
{
    chroot_srv_config *cfg = (chroot_srv_config *)ap_get_module_config(s->module_config, &chroot_module);
    if (cfg->chroot_dir!=NULL)
    {
      // go on chroot_dir of the chroot
      if(chdir(cfg->chroot_dir)<0) {
	      ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "chroot_child_init[%d]: could not chdir to '%s'.",getpid(),cfg->chroot_dir);
	      cfg->flag_fixroot=0;
	      exit(APEXIT_CHILDFATAL);
      }
      // chroot in the child
      if(chroot(cfg->chroot_dir)<0) {
	      ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "chroot_child_init[%d]: could not chroot to <%s>.",getpid(),cfg->chroot_dir);
	      cfg->flag_fixroot=0;
	      exit(APEXIT_CHILDFATAL);
      }
      // go on / of the chroot
      if(chdir("/")<0) {
	      ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "chroot_child_init[%d]: could not chdir to '/' in the chroot.",getpid());
	      cfg->flag_fixroot=0;
	      exit(APEXIT_CHILDFATAL);
      }
      ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, s, "chroot_child_init[%d]: the child are chrooted to <%s>.",getpid(),cfg->chroot_dir);

      // fix coredump dir only if fixroot are set
      if (cfg->flag_fixroot)
      {
        core_server_config *corecfg = ap_get_module_config(ap_server_conf->module_config, &core_module);
	// fix ap_coredump_dir  if CoreDumpDirectory are set
	if (ap_coredumpdir_configured==1)
	{
	   char *szap_coredump_dir = apr_pstrdup(s->process->pconf,ap_coredump_dir);
	   if (chroot_dirtofix(s->process->pconf, s, (char**)&szap_coredump_dir, cfg->chroot_dir,TRUE)!=OK) 
	   {
	      ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "chroot_child_init[%d]: could not fix CoreDumpDirectory <%s>.",getpid(),szap_coredump_dir);
	      exit(APEXIT_CHILDFATAL);
	   }
	   ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, s, 
	            "chroot_child_init[%d]: fixing CoreDumpDirectory from <%s> to <%s>.",
		        getpid(),ap_coredump_dir,szap_coredump_dir);
	   apr_cpystrn(ap_coredump_dir, szap_coredump_dir, sizeof(ap_coredump_dir));
	}
	else
	{
	   // if CoreDumpDirectory is not set
	   char *szap_coredump_dir = apr_pstrdup(s->process->pconf,ap_server_root);
	   if (chroot_dirtofix(s->process->pconf, s, (char**)&szap_coredump_dir, cfg->chroot_dir,TRUE)!=OK) 
	   {
	      ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "chroot_child_init[%d]: could not fix CoreDumpDirectory <%s>.",getpid(),szap_coredump_dir);
	      exit(APEXIT_CHILDFATAL);
	   }
	   ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, s, 
	            "chroot_child_init[%d]: fixing CoreDumpDirectory from <%s> to <%s>.",
		        getpid(),ap_coredump_dir,szap_coredump_dir);
	   apr_cpystrn(ap_coredump_dir, szap_coredump_dir, sizeof(ap_coredump_dir));
	}

        // fix ap_server_root and ap_document_root
	if (chroot_dirtofix(s->process->pconf, s, (char**)&ap_server_root, cfg->chroot_dir,TRUE)!=OK)
	{
	  ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "chroot_child_init[%d]: could not fix ap_server_root <%s>.",getpid(),ap_server_root);
	  exit(APEXIT_CHILDFATAL);
	}

	if (chroot_dirtofix(s->process->pconf, s, (char**)&corecfg->ap_document_root, cfg->chroot_dir,FALSE)!=OK)
	{
	  ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "chroot_child_init[%d]: could not fix documentroot <%s>.",getpid(),corecfg->ap_document_root);
	  exit(APEXIT_CHILDFATAL);
	}

      }

      // restore the real user_id and call unixd_setup_child to apply it
#if MODULE_MAGIC_NUMBER_MAJOR > 20051115
      ap_unixd_config.user_id=cfg->old_user_id;
      if (ap_unixd_setup_child()) {
#else
      unixd_config.user_id=cfg->old_user_id;
      if (unixd_setup_child()) {
#endif
	  exit(APEXIT_CHILDFATAL);
      }
      ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, s, "chroot_child_init[%d]: uid restored to %d.",getpid(),cfg->old_user_id);

    }

}

/************************************************************************
 *   apache command function                                            *
 ************************************************************************/

/* set chroot dir value */
static const char *cmd_chroot_dir(cmd_parms *cmd, void *dummy, const char *p1) 
{
    chroot_srv_config *cfg = (chroot_srv_config *)ap_get_module_config(cmd->server->module_config, &chroot_module);
    const char *err;
    char *newchroot_dir=NULL;

    // check if the command are used in global context
    if((err=ap_check_cmd_context(cmd, GLOBAL_ONLY))) return err;

    // if none deactive chroot_dir
    if(strncasecmp("none",p1,4)==0) {
      cfg->chroot_dir=NULL;
      return NULL;
    }

    // check if htdocs mode or serverroot mode to set the document root or serverroot as chroot dir...
    if (strcasecmp("DOCUMENT_ROOT",p1)==0)
    {
       core_server_config *corecfg = ap_get_module_config(cmd->server->module_config, &core_module);
       cfg->ap_document_root=apr_pstrdup(cfg->pool,corecfg->ap_document_root);
       if (cfg->ap_document_root==NULL) return "ChrootDir: ChrootDir cmd must be set after DocumentRoot cmd";
       cfg->chroot_dir=apr_pstrdup(cfg->pool,cfg->ap_document_root);
       if (cfg->chroot_dir[strlen(cfg->chroot_dir)-1]=='/') cfg->chroot_dir[strlen(cfg->chroot_dir)-1]='\0';
       return NULL;
    }
    else if (strcasecmp("SERVER_ROOT",p1)==0)
    {
       cfg->chroot_dir=apr_pstrdup(cfg->pool,ap_server_root);
       if (cfg->chroot_dir[strlen(cfg->chroot_dir)-1]=='/') cfg->chroot_dir[strlen(cfg->chroot_dir)-1]='\0';
       return NULL;
    }
    else
    {
      // if is relatif to serverroot concat it with
      if (p1[0]!='/')
	newchroot_dir=apr_pstrcat(cmd->pool,ap_server_root,"/",p1,NULL);
      else 
	newchroot_dir=(char*)p1;


      // check the value
      if(!ap_is_directory(cmd->pool, newchroot_dir)) {
	      return apr_pstrcat(cmd->pool,"ChrootDir: Chroot to something that is not a directory <",newchroot_dir,">.",NULL);
      }

      cfg->chroot_dir=newchroot_dir;
      if (cfg->chroot_dir[strlen(cfg->chroot_dir)-1]=='/') cfg->chroot_dir[strlen(cfg->chroot_dir)-1]='\0';
      return NULL;
    }
}

// set fixroot value
static const char * cmd_chroot_fixroot(cmd_parms *cmd, void *dummy, int flag) 
{
	chroot_srv_config *cfg = (chroot_srv_config *)ap_get_module_config(cmd->server->module_config, &chroot_module);
	const char *err;

	// check if the command are used in global context
	if((err=ap_check_cmd_context(cmd, GLOBAL_ONLY))) return err;

        cfg->flag_fixroot=flag;
	return NULL;
}

/******************************
 * module apache structure... * 
 * and hook                   *
 ******************************/

static void register_hooks(apr_pool_t *p) {
        static const char * const chrootTrans[] = { 
		"mod_proxy.c",
		"mod_rewrite.c", 
		NULL 
	};
        ap_hook_pre_mpm(chroot_pre_mpm, NULL, NULL, APR_HOOK_REALLY_LAST);	
        ap_hook_child_init(chroot_child_init, NULL, NULL, APR_HOOK_REALLY_FIRST);	
	ap_hook_translate_name(chroot_translate_name, chrootTrans, NULL, APR_HOOK_REALLY_FIRST);	
	ap_hook_map_to_storage(chroot_map_to_storage, NULL, NULL, APR_HOOK_MIDDLE);	
}

static const command_rec chroot_cmds[] = {

	AP_INIT_TAKE1 ( "ChrootDir", cmd_chroot_dir, NULL, RSRC_CONF,
		"path to the directory to chroot"),
	AP_INIT_FLAG( "ChrootFixRoot", cmd_chroot_fixroot, NULL, RSRC_CONF,
	        "work like old mod_chroot by default,set to on to activate transparent filename and documentroot path fixing"),

	{ NULL }
};

module AP_MODULE_DECLARE_DATA chroot_module = {
   STANDARD20_MODULE_STUFF,
   NULL,			 /* create per-dir    config structures */
   NULL,		         /* merge  per-dir    config structures */
   chroot_create_srv_config,     /* create per-server config structures */
   NULL,		         /* merge  per-server config structures */
   chroot_cmds,                  /* table of config file commands       */
   register_hooks                /* register hooks                      */
};

