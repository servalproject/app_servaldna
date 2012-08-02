/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2012 Daniel O'Connor <daniel@servalproject.org>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief Look up a number via Serval DNA
 * 
 * \author Daniel O'Connor <daniel@servalproject.org>
 * \ingroup functions
 */

/*** MODULEINFO
     <support_level>core</support_level>
***/

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: XXX $")

#include "asterisk/module.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/utils.h"
#include "asterisk/app.h"
#include "asterisk/cli.h"
#include "app.h"

static int	servaldna_exec(struct ast_channel *chan, const char *data);
static char 	*servaldna_lookup(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
static int	unload_module(void);
static int	load_module(void);
static void	servaldna_query(const char *did, char **reply);

static char *instancepath = NULL;
static char config_file[] = "servaldna.conf";
static char app[] = "ServalDNA";
static struct ast_cli_entry cli_servaldna[] = {
    AST_CLI_DEFINE(servaldna_lookup,	"Lookup a number via Serval DNA"),
};

/*** DOCUMENTATION
	<application name="servaldna" language="en_US">
		<synopsis>
			Lookup a number via Serval DNA
		</synopsis>
		<syntax>
			<parameter name="number" required="true" />
		</syntax>
		<description>
		<para>Lookup <replaceable>number</replaceable> via Serval DNA to try and find a URL.
		</description>
	</application>
 ***/
static int
servaldna_exec(struct ast_channel *chan, const char *data) {
    char 	*reply, *argcopy;
    
    AST_DECLARE_APP_ARGS(arglist,
			 AST_APP_ARG(did);
	);
    
    ast_log(LOG_WARNING, "servaldna_exec called\n");

    if (ast_strlen_zero(data)) {
	ast_log(LOG_WARNING, "Argument required (number to lookup)\n");
	return -1;
    }

    argcopy = ast_strdupa(data);

    AST_STANDARD_APP_ARGS(arglist, argcopy);

    servaldna_query(arglist.did, &reply);

    ast_log(LOG_WARNING, "Lookup returned \'%s\'\n", reply);
	
    if (reply == NULL)
	return -1;
    
    pbx_builtin_setvar_helper(chan, "SDNA_DEST", reply);

    free(reply);
    return 0;
}

static char *
servaldna_lookup(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a) {
    char 	*reply;

    switch (cmd) {
        case CLI_INIT:
	    e->command = "servaldna lookup";
	    e->usage = 
	    "Usage: servaldna lookup <number>\n"
	    "       Lookup <number> using Serval DNA\n";
	    return NULL;
        case CLI_GENERATE:
	    return NULL;
    }
    
    if (a->argc != 3) {
        ast_cli(a->fd, "You did not provide an argument to servaldna lookup\n\n");
        return CLI_FAILURE;
    }

    servaldna_query(a->argv[2], &reply);

    if (reply == NULL) {
        ast_cli(a->fd, "Lookup failed\n");
        return CLI_FAILURE;
    }
	
    ast_cli(a->fd, "Lookup returned \'%s\'\n", reply);

    return CLI_SUCCESS;
}

static int 
unregister_cli(void) {
    ast_log(LOG_WARNING, "Serval unload module called\n");

    if (instancepath != NULL)
	free(instancepath);
    instancepath = NULL;
    
    ast_cli_unregister_multiple(cli_servaldna, ARRAY_LEN(cli_servaldna));
    ast_unregister_application(app);

    return 0;
}

int
register_cli(void) {
    struct ast_config *cfg;
    struct ast_flags config_flags = { 0 ? CONFIG_FLAG_FILEUNCHANGED : 0 };
    const char *tmp;
    
    ast_log(LOG_WARNING, "Registering Serval CLI and dialplan functions\n");
    
    if ((cfg = ast_config_load(config_file, config_flags)) == NULL) {
	ast_log(LOG_WARNING, "Unable to load config file\n");
	goto error;
    }

    if ((tmp = ast_variable_retrieve(cfg, "general", "instancepath")) == NULL) {
	ast_log(LOG_WARNING, "Can't find required instancepath entry in general category\n");
	goto error;
    }
    instancepath = strdup(tmp);
    ast_log(LOG_WARNING, "Using instance path %s\n", instancepath);
    
    if (ast_register_application_xml(app, servaldna_exec)) {
	ast_log(LOG_WARNING, "Unable to register function\n");
	goto error;
    }
    
    if (ast_cli_register_multiple(cli_servaldna, ARRAY_LEN(cli_servaldna))) {
	ast_log(LOG_WARNING, "Unable to register CLI functions\n");
	goto error;
    }
    
    return 0;

  error:
    ast_config_destroy(cfg);
    unload_module();
    return -1;
}

static int
load_module(void){
    ast_log(LOG_WARNING, "Serval load module called\n");
    
    register_cli();
    vomp_register_channel();
    return 0;
}

static int 
unload_module(void) {
    unregister_cli();
    return 0;
}

static void
servaldna_query(const char *did, char **reply) {
    *reply = strdup("foo");
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "Lookup numbers via Serval DNA",
				.load = load_module,
				.unload = unload_module,
				.load_pri = AST_MODPRI_CHANNEL_DRIVER,
);
