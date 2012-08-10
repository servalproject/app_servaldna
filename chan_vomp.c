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
#include "monitor-client.h"
#include "constants.h"

static struct ast_channel  *vomp_request(const char *type, format_t format, const struct ast_channel *requestor, void *dest, int *cause);
static int vomp_call(struct ast_channel *ast, char *dest, int timeout);
static int vomp_hangup(struct ast_channel *ast);
static int vomp_answer(struct ast_channel *ast);
static struct ast_frame *vomp_read(struct ast_channel *ast);
static int vomp_write(struct ast_channel *ast, struct ast_frame *frame);
static int vomp_indicate(struct ast_channel *ast, int ind, const void *data, size_t datalen);
static int vomp_fixup(struct ast_channel *oldchan, struct ast_channel *newchan);
static struct vomp_channel *get_channel(char *token);

static void send_hangup(int session_id);
static void send_ringing(struct vomp_channel *vomp_state);
static void send_pickup(struct vomp_channel *vomp_state);
static void send_call(const char *sid, const char *caller_id, const char *remote_ext);
static void send_audio(struct vomp_channel *vomp_state, unsigned char *buffer, int len, int codec);

static int remote_dialing(char *cmd, int argc, char **argv, unsigned char *data, int dataLen, void *context);
static int remote_call(char *cmd, int argc, char **argv, unsigned char *data, int dataLen, void *context);
static int remote_pickup(char *cmd, int argc, char **argv, unsigned char *data, int dataLen, void *context);
static int remote_hangup(char *cmd, int argc, char **argv, unsigned char *data, int dataLen, void *context);
static int remote_audio(char *cmd, int argc, char **argv, unsigned char *data, int dataLen, void *context);
static int remote_ringing(char *cmd, int argc, char **argv, unsigned char *data, int dataLen, void *context);
static int remote_noop(char *cmd, int argc, char **argv, unsigned char *data, int dataLen, void *context);

static const char desc[] = "Serval Vomp Channel Driver";
static const char type[] = "VOMP";
static const char tdesc[] = "Serval Vomp Channel Driver";

AST_MUTEX_DEFINE_STATIC(vomplock); 
int monitor_client_fd=-1;

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
	int session_id; // call session id as returned by servald, used as the key for the channels collection
	int chan_id; // unique number for generating a name for the channel
	int channel_start; // time when we started the channel
	int call_start; // time when we hit in-call
	int initiated; // did asterisk start dialing?
	struct ast_channel *owner;
};

struct vomp_channel *dialed_call;

struct monitor_command_handler monitor_handlers[]={
	{.command="CALLFROM",      .handler=remote_call},
	{.command="RINGING",       .handler=remote_ringing},
	{.command="ANSWERED",      .handler=remote_pickup},
	{.command="CALLTO",        .handler=remote_dialing},
	{.command="HANGUP",        .handler=remote_hangup},
	{.command="AUDIOPACKET",   .handler=remote_audio},
	{.command="KEEPALIVE",     .handler=remote_noop},
	{.command="CALLSTATUS",    .handler=remote_noop},
	{.command="MONITORSTATUS", .handler=remote_noop},
	{.command="MONITOR",       .handler=remote_noop},
};

int chan_id=0;
// id for the monitor thread
pthread_t thread;

static struct ao2_container *channels;

static long long gettime_ms(void)
{
	struct timeval nowtv;
	gettimeofday(&nowtv, NULL);
	return nowtv.tv_sec * 1000LL + nowtv.tv_usec / 1000;
}

static void vomp_channel_destructor(void *obj){
	// noop...
}

static struct vomp_channel *new_vomp_channel(void){
	struct vomp_channel *vomp_state;
	vomp_state = ao2_alloc(sizeof(struct vomp_channel), vomp_channel_destructor);
	
	// allocate a unique number for this channel
	vomp_state->chan_id = ast_atomic_fetchadd_int(&chan_id, +1);
	vomp_state->channel_start = gettime_ms();
	
	return vomp_state;
}

static void set_session_id(struct vomp_channel *vomp_state, int session_id){
	ast_log(LOG_WARNING, "Adding session %06x\n",session_id);
	vomp_state->session_id = session_id;
	ao2_link(channels, vomp_state);
}

static struct ast_channel *new_channel(struct vomp_channel *vomp_state, int state, char *context, char *ext){
	struct ast_channel *ast;

	ao2_lock(vomp_state);
	
	if (vomp_state->owner)
		return vomp_state->owner;
	
	ast = ast_channel_alloc(1, state, NULL, NULL, NULL, ext, context, NULL, 0, "VoMP/%08x", vomp_state->chan_id);
	
	ast->nativeformats = AST_FORMAT_SLINEAR;
	ast->readformat = AST_FORMAT_SLINEAR;
	ast->writeformat = AST_FORMAT_SLINEAR;
	ast->tech=&vomp_tech;
	ao2_ref(vomp_state, 1);
	ast->tech_pvt=vomp_state;
	vomp_state->owner = ast;
	
	ao2_unlock(vomp_state);
	return ast;
}


// functions for handling incoming vomp events

// find the channel struct from the servald token
// note that ao2_find adds a reference to the returned object that must be released
struct vomp_channel *get_channel(char *token){
	struct vomp_channel search={
		.session_id=strtol(token, NULL, 16),
	};
	struct vomp_channel *ret = ao2_find(channels, &search, OBJ_POINTER);
	if (ret==NULL)
		ast_log(LOG_WARNING, "Failed to find call structure for session %s (%06x)\n",token,search.session_id);
	return ret;
}

// Send outgoing monitor messages

// TODO fix servald, commands are currently case sensitive
void send_hangup(int session_id){
	monitor_client_writeline(monitor_client_fd, "hangup %06x\n",session_id);
}
void send_ringing(struct vomp_channel *vomp_state){
	monitor_client_writeline(monitor_client_fd, "ringing %06x\n",vomp_state->session_id);
}
void send_pickup(struct vomp_channel *vomp_state){
	monitor_client_writeline(monitor_client_fd, "pickup %06x\n",vomp_state->session_id);
}
void send_call(const char *sid, const char *caller_id, const char *remote_ext){
	monitor_client_writeline(monitor_client_fd, "call %s %s %s\n", sid, caller_id, remote_ext);
}
void send_audio(struct vomp_channel *vomp_state, unsigned char *buffer, int len, int codec){
	monitor_client_writeline_and_data(monitor_client_fd, buffer, len, "AUDIO %06x %d\n", vomp_state->session_id, codec);
}

// CALLTO [token] [localsid] [localdid] [remotesid] [remotedid]
// sent so that we can link an outgoing call to a servald session id
int remote_dialing(char *cmd, int argc, char **argv, unsigned char *data, int dataLen, void *context){
	ast_log(LOG_WARNING, "remote_dialing\n");
	if (!dialed_call)
		return 0;
	// add the vomp state to our collection so we can find it later
	set_session_id(dialed_call, strtol(argv[0], NULL, 16));
	dialed_call=NULL;
	return 1;
}

// CALLFROM [token] [localsid] [localdid] [remotesid] [remotedid]
int remote_call(char *cmd, int argc, char **argv, unsigned char *data, int dataLen, void *context){
	// TODO fix servald and other VOMP clients to pass extension correctly
	// TODO add callerid...
	char *ext = "100";//argv[2];
	char *ctx="servald-in";
	ast_log(LOG_WARNING, "remote_call\n");
	int session_id=strtol(argv[0], NULL, 16);
	
	if (ast_exists_extension(NULL, ctx, ext, 1, NULL)) {
		struct vomp_channel *vomp_state=new_vomp_channel();
		set_session_id(vomp_state, session_id);
		vomp_state->initiated=0;
		
		struct ast_channel *ast = new_channel(vomp_state, AST_STATE_RINGING, ctx, ext);
		ast_log(LOG_WARNING, "Handing call %s@%s over to pbx_start\n", ext, ctx);
		if (ast_pbx_start(ast)) {
			ast->hangupcause = AST_CAUSE_SWITCH_CONGESTION;
			ast_log(LOG_WARNING, "pbx_start failed, hanging up\n");
			ast_hangup(ast);
			return 0;
		}
		return 1;
	}
	send_hangup(session_id);
	return 0;
}

int remote_pickup(char *cmd, int argc, char **argv, unsigned char *data, int dataLen, void *context){
	int ret=0;
	ast_log(LOG_WARNING, "remote_pickup\n");
	struct vomp_channel *vomp_state=get_channel(argv[0]);
	if (vomp_state){
		if (vomp_state->owner){
			// stop any audio indications on the channel
			ast_indicate(vomp_state->owner, -1);
			// yay, we're INCALL
			ast_queue_control(vomp_state->owner, AST_CONTROL_ANSWER);
			ret=1;
		}
		ao2_ref(vomp_state,-1);
	}
	return ret;
}

int remote_hangup(char *cmd, int argc, char **argv, unsigned char *data, int dataLen, void *context){
	int ret=0;
	ast_log(LOG_WARNING, "remote_hangup\n");
	struct vomp_channel *vomp_state=get_channel(argv[0]);
	if (vomp_state){
		if (vomp_state->owner){
			// ask asterisk to hangup the channel
			// that way we can let vomp_hangup do all the work to release memory
			ast_queue_hangup(vomp_state->owner);
			ret=1;
		}
		ao2_ref(vomp_state,-1);
	}
	return ret;
}

int remote_audio(char *cmd, int argc, char **argv, unsigned char *data, int dataLen, void *context){
	int ret=0;
	struct vomp_channel *vomp_state=get_channel(argv[0]);
	if (vomp_state){
		if (vomp_state->owner){
	
			// currently assumes 16bit, 8kHz pcm
			// TODO deal with packet loss and reordering?
			struct ast_frame f = {
				.frametype = AST_FRAME_VOICE,
				.subclass.codec = AST_FORMAT_SLINEAR,
				.src = "vomp_call",
				.data.ptr = data,
				.datalen = dataLen,
				.samples = dataLen / sizeof(int16_t),
			};
			ast_queue_frame(vomp_state->owner, &f);
			ret=1;
		}
		ao2_ref(vomp_state,-1);
	}
	return ret;
}

// remote party has started ringing
int remote_ringing(char *cmd, int argc, char **argv, unsigned char *data, int dataLen, void *context){
	int ret=0;
	ast_log(LOG_WARNING, "remote_ringing\n");
	struct vomp_channel *vomp_state=get_channel(argv[0]);
	if (vomp_state){
		if (vomp_state->owner){
			ast_indicate(vomp_state->owner, AST_CONTROL_RINGING);
			ast_queue_control(vomp_state->owner, AST_CONTROL_RINGING);
			ret=1;
		}
		ao2_ref(vomp_state,-1);
	}
	return ret;
}

int remote_noop(char *cmd, int argc, char **argv, unsigned char *data, int dataLen, void *context){
	// NOOP for now, just to eliminate unnecessary log spam
	return 1;
}

// thread function for the monitor client
// reads and processes incoming messages
static void *vomp_monitor(void *ignored){
	struct monitor_state *state;
	
	// TODO, start servald and retry on error
	
	ast_log(LOG_WARNING, "opening monitor connection\n");
	monitor_client_fd = monitor_client_open(&state);
	if (monitor_client_fd<0){
		ast_log(LOG_ERROR, "Failed to open monitor connection, start servald and restart asterisk\n");
		return NULL;
	}
		
	ast_log(LOG_WARNING, "sending monitor vomp command\n");
	monitor_client_writeline(monitor_client_fd, "MONITOR VOMP\n");
	ast_log(LOG_WARNING, "reading monitor events\n");
	for(;;){
		pthread_testcancel();
		if (monitor_client_read(monitor_client_fd, state, monitor_handlers, 
					sizeof(monitor_handlers)/sizeof(struct monitor_command_handler))<0){
			break;
		}
	}
	monitor_client_close(monitor_client_fd, state);
	monitor_client_fd=-1;
	return NULL;
}



// functions for handling incoming asterisk events

// create a channel for a new outgoing call
static struct ast_channel *vomp_request(const char *type, format_t format, const struct ast_channel *requestor, void *dest_, int *cause){
	// assume dest = servald subscriber id (sid)
	// TODO parse dest = sid/did
	char sid[64], *dest = dest_;
	int i;
	for (i=0;i<sizeof(sid) && dest[i] && dest[i]!='/';i++){
		sid[i]=dest[i];
	}
	sid[i]=0;
	ast_log(LOG_WARNING, "vomp_request %s/%s\n", type, sid);
	struct vomp_channel *vomp_state=new_vomp_channel();
	
	vomp_state->initiated=1;
	struct ast_channel *ast = new_channel(vomp_state, AST_STATE_DOWN, NULL, NULL);
	
	dialed_call = vomp_state;
	
	send_call(sid,"1","1");
	
	return ast;
}

static int vomp_hangup(struct ast_channel *ast){
	ast_log(LOG_WARNING, "vomp_hangup %s\n", ast->name);
	
	struct vomp_channel *vomp_state = (struct vomp_channel *)ast->tech_pvt;
	
	send_hangup(vomp_state->session_id);
	ao2_unlink(channels, vomp_state);
	ao2_ref(vomp_state,-1);
	ast->tech_pvt = NULL;
	return 0;
}

static int vomp_fixup(struct ast_channel *oldchan, struct ast_channel *newchan){
	struct vomp_channel *vomp_state = newchan->tech_pvt;
	ast_log(LOG_WARNING, "vomp_fixup %s %s\n", oldchan->name, newchan->name);
	vomp_state->owner = newchan;
	return 0;
}

static int vomp_indicate(struct ast_channel *ast, int ind, const void *data, size_t datalen){
	ast_log(LOG_WARNING, "vomp_indicate %d condition on channel %s\n", 
			ind, ast->name);
	// return -1 and asterisk will generate audible tones.
	
	struct vomp_channel *vomp_state = ast->tech_pvt;
	switch(ind){
		case AST_CONTROL_RINGING:
			if (!vomp_state->initiated)
				send_ringing(vomp_state);
			break;
			
		case AST_CONTROL_BUSY:
		case AST_CONTROL_CONGESTION:
			send_hangup(vomp_state->session_id);
			break;
			
		default:
			return -1;
	}
	return 0;
}

static int vomp_call(struct ast_channel *ast, char *dest, int timeout){
	// NOOP, as we have already started the call in vomp_request
	ast_log(LOG_WARNING, "vomp_call %s %s\n", ast->name, dest);
	
	return 0;
}

static int vomp_answer(struct ast_channel *ast){
	ast_log(LOG_WARNING, "vomp_answer %s\n", ast->name);
	
	struct vomp_channel *vomp_state = ast->tech_pvt;
	
	vomp_state->call_start = gettime_ms();
	ast_setstate(ast, AST_STATE_UP);
	send_pickup(vomp_state);
	return 0;
}

static struct ast_frame *vomp_read(struct ast_channel *ast){
	// this method should only be called if we told asterisk to monitor a file for us.
	
	ast_log(LOG_WARNING, "vomp_read %s - this shouldn't happen\n", ast->name);
	
	return &ast_null_frame;
}

static int vomp_write(struct ast_channel *ast, struct ast_frame *frame){
	if (frame->frametype == AST_FRAME_VOICE){
		struct vomp_channel *vomp_state = ast->tech_pvt;
		send_audio(vomp_state, frame->data.ptr, frame->samples*2, VOMP_CODEC_PCM);
	}
	
	return 0;
}

// module load / unload

static int vomp_hash(const void *obj, const int flags){
	const struct vomp_channel *vomp_state = obj;
	return vomp_state->session_id;
}

static int vomp_compare(void *obj, void *arg, int flags){
	struct vomp_channel *obj1 = obj;
	struct vomp_channel *obj2 = arg;
	return obj1->session_id==obj2->session_id ? CMP_MATCH | CMP_STOP : 0;
}

int vomp_register_channel(void){
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


/*
 * Local variables:
 * c-basic-offset: 8
 * End:
 */

