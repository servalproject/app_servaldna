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


#include <stdio.h>
#include <string.h>
#include "asterisk.h"
#include "asterisk/lock.h"
#include "asterisk/channel.h"
#include "asterisk/config.h"
#include "asterisk/logger.h"
#include "asterisk/module.h"
#include "asterisk/pbx.h"
#include "asterisk/options.h"
#include "asterisk/lock.h"
#include "asterisk/sched.h"
#include "asterisk/io.h"
#include "asterisk/acl.h"
#include "asterisk/callerid.h"
#include "asterisk/file.h"
#include "asterisk/cli.h"
#include "asterisk/causes.h"
#include "asterisk/devicestate.h"
#include <asterisk/dsp.h>
#include <asterisk/ulaw.h>

#include "app.h"


static struct ast_channel *vomp_request(const char *type, format_t format, const struct ast_channel *requestor, const char *dest, int *cause);
static int vomp_call(struct ast_channel *ast, char *dest, int timeout);
static int vomp_hangup(struct ast_channel *ast);
static int vomp_answer(struct ast_channel *ast);
static struct ast_frame *vomp_read(struct ast_channel *ast);
static int vomp_write(struct ast_channel *ast, struct ast_frame *frame);
static int vomp_indicate(struct ast_channel *ast, int ind, const void *data, size_t datalen);
static int vomp_fixup(struct ast_channel *oldchan, struct ast_channel *newchan);

static const char desc[] = "Serval Vomp Channel Driver";
static const char type[] = "VOMP";
static const char tdesc[] = "Serval Vomp Channel Driver";

static pthread_t monitor_thread = AST_PTHREADT_NULL;

AST_MUTEX_DEFINE_STATIC(vomplock); 

static const struct ast_channel_tech vomp_tech = {
	.type         = type,
	.description  = tdesc,
	.capabilities = AST_FORMAT_ULAW | AST_FORMAT_ALAW | AST_FORMAT_SLINEAR,
	.properties = AST_CHAN_TP_WANTSJITTER | AST_CHAN_TP_CREATESJITTER,
	.requester    = vomp_request,
	.call         = vomp_call,
	.hangup       = vomp_hangup,
	.answer       = vomp_answer,
	.read         = vomp_read,
	.write        = vomp_write,
	.exception    = NULL,
	.indicate     = vomp_indicate,
	.fixup        = vomp_fixup,
};

struct vomp_channel {
	ast_mutex_t lock;
	int local_state;
	int remote_state;
	struct ast_channel   *owner;
};

int chan_id=0;


static struct vomp_channel *new_vomp_channel(){
	struct vomp_channel *vomp_state;
	vomp_state = (struct vomp_channel *)malloc(sizeof(struct vomp_channel));
	memset(vomp_state, 0, sizeof(struct vomp_channel));
	return vomp_state;
}

static struct ast_channel *new_channel(vomp_channel *vomp_state){
	struct ast_channel *ast;
	
	ast = ast_channel_alloc(1, AST_STATE_DOWN, NULL, NULL, NULL, NULL, NULL, NULL, 0, "VoMP/%08x", ast_atomic_fetchadd_int(&chan_id, +1));
	
	ast->tech=&vomp_tech;
	ast->tech_pvt=vomp_state;
	vomp_state->owner = ast;
	
	return ast;
}


// function stubs for dealing with remote state changes


struct ast_channel * remote_dial(char *ext){
	if (ast_exists_extension(NULL, context, ext, 1, NULL)) {
		struct vomp_channel *vomp_state = new_vomp_channel();
		struct ast_channel *ast = new_channel(vomp_state);
		if (ast_pbx_start(ast)) {
			ast_hangup(ast);
			return NULL;
		}
		return ast;
	}
	return NULL;
}

void remote_pickup(struct ast_channel *ast){
	if (0){
		struct ast_frame f;
		memset(&f,0,sizeof(struct ast_frame));
		f.frametype = AST_FRAME_CONTROL;
		f.subclass = AST_CONTROL_ANSWER;
		ast_queue_frame(ast, &f);
	}else{
		ast_setstate(ast, AST_STATE_UP);		
	}
}

void remote_hangup(struct ast_channel *ast){
	ast_queue_hangup(ast);
}



// functions for handling asterisk state changes

// create a channel for a new outgoing call
static struct ast_channel *vomp_request(const char *type, format_t format, const struct ast_channel *requestor, const char *dest, int *cause)
{
	ast_log(LOG_WARNING, "vomp_request %s/%s\n", type, dest);
	struct vomp_channel *vomp_state=new_vomp_channel();
	return new_channel(vomp_state);
}

static int vomp_hangup(struct ast_channel *ast)
{
	ast_log(LOG_WARNING, "vomp_hangup %s\n", ast->name);
	
	// TODO local_state --> CallEnded
	struct vomp_channel *vomp_state = (struct vomp_channel *)ast->tech_pvt;
	
	free(ast->tech_pvt);
	ast->tech_pvt = NULL;
	return 0;
}

static int vomp_fixup(struct ast_channel *oldchan, struct ast_channel *newchan)
{
	struct vomp_channel *p = newchan->tech_pvt;
	ast_log(LOG_WARNING, "vomp_fixup %s %s\n", oldchan->name, newchan->name);
	p->owner = newchan;
	return 0;
}

static int vomp_indicate(struct ast_channel *ast, int ind, const void *data, size_t datalen)
{
	ast_log(LOG_WARNING, "vomp_indicate %d condition on channel %s\n", 
			ind, ast->name);
	
	switch(ind){
		case AST_CONTROL_RINGING:
			// TODO local_state --> RingingIn
			break;
			
		case AST_CONTROL_BUSY:
		case AST_CONTROL_CONGESTION:
			// TODO hangup
			break;
			
		default:
			return -1;
	}
	return 0;
}

static int vomp_call(struct ast_channel *ast, char *dest, int timeout)
{
	ast_log(LOG_WARNING, "vomp_call %s %s\n", ast->name, dest);
	
	// Who are we calling?????
	
	return 0;
}

static int vomp_answer(struct ast_channel *ast)
{
	ast_log(LOG_WARNING, "vomp_answer %s\n", ast->name);
	
	// TODO local_state --> InCall
	
	return 0;
}

static struct ast_frame *vomp_read(struct ast_channel *ast)
{
	ast_log(LOG_WARNING, "vomp_read %s - this shouldn't happen\n", ast->name);
	
	return &ast_null_frame;
}

static int vomp_write(struct ast_channel *ast, struct ast_frame *frame)
{
	//struct vomp_channel *p = ast->tech_pvt;
	
	ast_log(LOG_WARNING, "vomp_write %s\n", ast->name);
	
	// TODO send audio
	
	if (frame->frametype != AST_FRAME_VOICE){
		
	}
	
	return 0;
}

// module load / unload

int vomp_register_channel(void)
{
    ast_log(LOG_WARNING, "Registering Serval channel driver\n");
	
	if (ast_channel_register(&vomp_tech)) {
		ast_log(LOG_ERROR, "Unable to register channel class %s\n", type);
		return AST_MODULE_LOAD_FAILURE;
	}
	return 0;
}

int vomp_unregister_channel(void){
    ast_log(LOG_WARNING, "Unregistering Serval channel driver\n");
	ast_channel_unregister(&vomp_tech);
	return 0;
}