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
#include <time.h>
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
	int local_state;
	int remote_state;
	int chan_id;
	char ext[32];
	int channel_start;
	int call_start;
	struct ast_channel *owner;
};

int chan_id=0;
// id for the monitor thread
pthread_t thread;

static struct ao2_container *channels;

long long gettime_ms()
{
	struct timeval nowtv;
	gettimeofday(&nowtv, NULL);
	return nowtv.tv_sec * 1000LL + nowtv.tv_usec / 1000;
}

static void vomp_channel_destructor(void *obj){
	// noop...
}

static struct vomp_channel *new_vomp_channel(){
	struct vomp_channel *vomp_state;
	vomp_state = ao2_alloc(sizeof(struct vomp_channel), vomp_channel_destructor);
	
	// allocate a unique number for this channel
	vomp_state->chan_id = ast_atomic_fetchadd_int(&chan_id, +1);
	vomp_state->channel_start = gettime_ms();
	
	ao2_link(channels, vomp_state);
	ao2_ref(vomp_state, -1);
	
	return vomp_state;
}

static struct ast_channel *new_channel(struct vomp_channel *vomp_state, char *context, char *ext){
	struct ast_channel *ast;

	if (vomp_state->owner)
		return vomp_state->owner;
	
	ao2_lock(vomp_state);
	
	ast = ast_channel_alloc(1, AST_STATE_DOWN, NULL, NULL, NULL, context, ext, NULL, 0, "VoMP/%08x", vomp_state->chan_id);
	
	ast->nativeformats = AST_FORMAT_SLINEAR16;
	ast->readformat = AST_FORMAT_SLINEAR16;
	ast->writeformat = AST_FORMAT_SLINEAR16;
	ast->tech=&vomp_tech;
	ast->tech_pvt=vomp_state;
	vomp_state->owner = ast;
	
	ao2_unlock(vomp_state);
	return ast;
}


// functions for handling incoming vomp events


// remote party would like to initiate a call to ext
struct vomp_channel * remote_init(char *ext){
	ast_log(LOG_WARNING, "remote_init\n");
	// first, test the dial plan
	if (ast_exists_extension(NULL, "s", ext, 1, NULL)) {
		struct vomp_channel *vomp_state = new_vomp_channel();
		strncpy(vomp_state->ext, ext, sizeof(vomp_state->ext));
		return vomp_state;
	}
	return NULL;
}

// we can agree on codec's and we've both setup our call state
// the remote party would like us to initiate the call now.
void remote_ring(struct vomp_channel *vomp_state){
	ast_log(LOG_WARNING, "remote_ring\n");
	struct ast_channel *ast = new_channel(vomp_state, "s", vomp_state->ext);
	// do we need to set the state to AST_STATE_RINGING now?
	if (ast_pbx_start(ast)) {
		ast_hangup(ast);
	}
	// we should be able to rely on asterisk to indicate success / failure through other methods.
}

void remote_pickup(struct vomp_channel *vomp_state){
	if (!vomp_state->owner) return;
	ast_log(LOG_WARNING, "remote_pickup\n");
	
	ast_indicate(vomp_state->owner, -1);
	ast_queue_control(vomp_state->owner, AST_CONTROL_ANSWER);
}

void remote_hangup(struct vomp_channel *vomp_state){
	if (!vomp_state->owner) return;
	ast_log(LOG_WARNING, "remote_hangup\n");
	ast_queue_hangup(vomp_state->owner);
}

void remote_audio(struct vomp_channel *vomp_state, char *buff, int len){
	// currently assumes 16bit pcm
	if (!vomp_state->owner) return;
	ast_log(LOG_WARNING, "remote_audio\n");
	struct ast_frame f = {
		.frametype = AST_FRAME_VOICE,
		.subclass.codec = AST_FORMAT_SLINEAR16,
		.src = "vomp_call",
		.data.ptr = buff,
		.datalen = len,
		.samples = len / sizeof(int16_t),
	};
	ast_queue_frame(vomp_state->owner, &f);
}

// remote party has started ringing
void remote_ringing(struct vomp_channel *vomp_state){
	if (!vomp_state->owner) return;
	ast_log(LOG_WARNING, "remote_ringing\n");
	ast_indicate(vomp_state->owner, AST_CONTROL_RINGING);
	ast_queue_control(vomp_state->owner, AST_CONTROL_RINGING);
}



static void *vomp_monitor(void *ignored){
	struct ao2_iterator i;
	struct vomp_channel *vomp_state;
	for(;;){
		pthread_testcancel();
		// TODO read monitor socket and process events
		
		// for now, just simulate call life cycle with audio echo
		i = ao2_iterator_init(channels, 0);
		while ((vomp_state = ao2_iterator_next(&i))) {
			ao2_lock(vomp_state);
			
			int now = gettime_ms();
			int age = now - vomp_state->channel_start;
			
			if (vomp_state->owner){
				
				switch(vomp_state->local_state){
					case 0:
						if (age > 500){
							ast_log(LOG_WARNING, "Simulating remote ring on channel %s\n", vomp_state->owner->name);
							
							vomp_state->local_state=1;
							remote_ringing(vomp_state);
						}
						break;
					case 1:
						if (age > 3000){
							ast_log(LOG_WARNING, "Simulating remote pickup on channel %s\n", vomp_state->owner->name);
							
							vomp_state->local_state=2;
							remote_pickup(vomp_state);
						}
						break;
					case 2:
						if (age > 10000){
							ast_log(LOG_WARNING, "Simulating remote hangup on channel %s\n", vomp_state->owner->name);
							
							vomp_state->local_state=3;
							remote_hangup(vomp_state);
						}
						break;
				}
				
				
			}
			ao2_unlock(vomp_state);
		}
		ao2_iterator_destroy(&i);
		pthread_testcancel();
		sleep(1);
	}
	return NULL;
}




// functions for handling incoming asterisk events

// create a channel for a new outgoing call
static struct ast_channel *vomp_request(const char *type, format_t format, const struct ast_channel *requestor, const char *dest, int *cause)
{
	// Note, dest could indicate some config or something, for now we don't care.
	
	ast_log(LOG_WARNING, "vomp_request %s/%s\n", type, dest);
	struct vomp_channel *vomp_state=new_vomp_channel();
	return new_channel(vomp_state, NULL, NULL);
}

static int vomp_hangup(struct ast_channel *ast)
{
	ast_log(LOG_WARNING, "vomp_hangup %s\n", ast->name);
	
	// TODO local_state --> CallEnded
	struct vomp_channel *vomp_state = (struct vomp_channel *)ast->tech_pvt;
	
	ao2_ref(vomp_state,-1);
	ao2_unlink(channels, vomp_state);
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
	
	
	// return -1 and asterisk will generate audible tones.
	
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
	// place a call to dest, expect format "sid:[sid]/[did]"
	ast_log(LOG_WARNING, "vomp_call %s %s\n", ast->name, dest);
	
	// TODO send out call request
	
	
	return 0;
}

static int vomp_answer(struct ast_channel *ast)
{
	ast_log(LOG_WARNING, "vomp_answer %s\n", ast->name);
	
	struct vomp_channel *vomp_state = ast->tech_pvt;
	
	vomp_state->call_start = gettime_ms();
	ast_setstate(ast, AST_STATE_UP);
	// TODO local_state --> InCall
	
	return 0;
}

static struct ast_frame *vomp_read(struct ast_channel *ast)
{
	// this method should only be called if we told asterisk to monitor a file for us.
	
	ast_log(LOG_WARNING, "vomp_read %s - this shouldn't happen\n", ast->name);
	
	return &ast_null_frame;
}

static int vomp_write(struct ast_channel *ast, struct ast_frame *frame)
{
	ast_log(LOG_WARNING, "vomp_write %s\n", ast->name);
	
	if (frame->frametype == AST_FRAME_VOICE){
		struct vomp_channel *vomp_state = ast->tech_pvt;
		// echo test;
		remote_audio(vomp_state, frame->data.ptr, frame->samples*sizeof(int16_t));
		
		// TODO send audio
		// frame->data.ptr, frame->samples		
	}
	
	return 0;
}

// module load / unload

static int vomp_hash(const void *obj, const int flags){
	const struct vomp_channel *vomp_state = obj;
	// TODO return sid?
	return vomp_state->chan_id;
}

static int vomp_compare(void *obj, void *arg, int flags){
	struct vomp_channel *obj1 = obj;
	struct vomp_channel *obj2 = arg;
	return obj1->chan_id==obj2->chan_id ? CMP_MATCH | CMP_STOP : 0;
}

int vomp_register_channel(void)
{
    ast_log(LOG_WARNING, "Registering Serval channel driver\n");
	
	if (ast_channel_register(&vomp_tech)) {
		ast_log(LOG_ERROR, "Unable to register channel class %s\n", type);
		return AST_MODULE_LOAD_FAILURE;
	}
	
	channels=ao2_container_alloc(1, vomp_hash, vomp_compare);
	
	if (ast_pthread_create_background(&thread, NULL, vomp_monitor, NULL)) {
	}
	
    ast_log(LOG_WARNING, "Done\n");
	return 0;
}

int vomp_unregister_channel(void){
    ast_log(LOG_WARNING, "Unregistering Serval channel driver\n");
	
	pthread_cancel(thread);
	pthread_kill(thread, SIGURG);
	pthread_join(thread, NULL);
	
	ast_channel_unregister(&vomp_tech);
	ao2_ref(channels, -1);
	
    ast_log(LOG_WARNING, "Done\n");
	return 0;
}