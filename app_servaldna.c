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

static char app[] = "servaldna";
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
	ast_verbose("servaldna_exec called\n");
	
	return 0;
}


static char *
servaldna_lookup(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a) {
    ast_cli(a->fd, "servaldna_lookup called\n");
    
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

    return CLI_SUCCESS;
}

static struct ast_cli_entry cli_servaldna[] = {
    AST_CLI_DEFINE(servaldna_lookup,	"Lookup a number via Serval DNA"),
};

static int 
unload_module(void) {
    ast_verbose("Serval unload module called\n");

    ast_cli_unregister_multiple(cli_servaldna, ARRAY_LEN(cli_servaldna));
    ast_unregister_application(app);

    return 0;
}

static int
load_module(void) {
    ast_verbose("Serval load module called\n");
    
    if (ast_register_application_xml(app, servaldna_exec)) {
	ast_verbose("Unable to register function\n");
	goto error;
    }
    
    if (ast_cli_register_multiple(cli_servaldna, ARRAY_LEN(cli_servaldna))) {
	ast_verbose("Unable to register CLI functions\n");
	goto error;
    }
    
    return 0;

  error:
    unload_module();
    return -1;
}
    
AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Lookup numbers via Serval DNA");
