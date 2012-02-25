/***************************************************************************
 *   Based on work by xmms2 team. #xmms2 irc.freenode.org
 *   Based on work by Max Howell <max.howell@methylblue.com>               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <sys/types.h>
//#include <sys/socket.h>
//#include <sys/un.h>
#include <unistd.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <jni.h>
#include <time.h>
#include <android/log.h>
#include <android/bitmap.h>
#include <libvisual/libvisual.h>

#define DEVICE_DEPTH VISUAL_VIDEO_DEPTH_16BIT

#define  LOG_TAG    "FroyVisuals"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGW(...)  __android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

#define MORPH "alphablend"
#define ACTOR "bumpscope"
#define INPUT "dummy"

struct {
    int16_t *pcm_data;
    int size;
    VisAudioSampleRateType rate;
    VisAudioSampleChannelType channels;
    VisAudioSampleFormatType encoding;
} pcm_ref;

/* LIBVISUAL */
struct {
	VisVideo   *video;
    VisPalette  *pal;
	VisBin     *bin;
	const char *actor_name;
    const char *morph_name;
    const char *input_name;
	int         pluginIsGL;
} v;

/* Return current time in milliseconds */
static double now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000. + tv.tv_usec/1000.;
}

/* simple stats management */
typedef struct {
    double  renderTime;
    double  frameTime;
} FrameStats;

#define  MAX_FRAME_STATS  200
#define  MAX_PERIOD_MS    1500

typedef struct {
    double  firstTime;
    double  lastTime;
    double  frameTime;
    int         firstFrame;
    int         numFrames;
    FrameStats  frames[ MAX_FRAME_STATS ];
} Stats;

static void
stats_init( Stats*  s )
{
    s->lastTime = now_ms();
    s->firstTime = 0.;
    s->firstFrame = 0;
    s->numFrames  = 0;
}

static void
stats_startFrame( Stats*  s )
{
    s->frameTime = now_ms();
}

static void
stats_endFrame( Stats*  s )
{
    double now = now_ms();
    double renderTime = now - s->frameTime;
    double frameTime  = now - s->lastTime;
    int nn;

    if (now - s->firstTime >= MAX_PERIOD_MS) {
        if (s->numFrames > 0) {
            double minRender, maxRender, avgRender;
            double minFrame, maxFrame, avgFrame;
            int count;

            nn = s->firstFrame;
            minRender = maxRender = avgRender = s->frames[nn].renderTime;
            minFrame  = maxFrame  = avgFrame  = s->frames[nn].frameTime;
            for (count = s->numFrames; count > 0; count-- ) {
                nn += 1;
                if (nn >= MAX_FRAME_STATS)
                    nn -= MAX_FRAME_STATS;
                double render = s->frames[nn].renderTime;
                if (render < minRender) minRender = render;
                if (render > maxRender) maxRender = render;
                double frame = s->frames[nn].frameTime;
                if (frame < minFrame) minFrame = frame;
                if (frame > maxFrame) maxFrame = frame;
                avgRender += render;
                avgFrame  += frame;
            }
            avgRender /= s->numFrames;
            avgFrame  /= s->numFrames;

            visual_log(VISUAL_LOG_INFO, "frame/s (avg,min,max) = (%.1f,%.1f,%.1f) "
                 "render time ms (avg,min,max) = (%.1f,%.1f,%.1f)\n",
                 1000./avgFrame, 1000./maxFrame, 1000./minFrame,
                 avgRender, minRender, maxRender);
        }
        s->numFrames  = 0;
        s->firstFrame = 0;
        s->firstTime  = now;
    }

    nn = s->firstFrame + s->numFrames;
    if (nn >= MAX_FRAME_STATS)
        nn -= MAX_FRAME_STATS;

    s->frames[nn].renderTime = renderTime;
    s->frames[nn].frameTime  = frameTime;

    if (s->numFrames < MAX_FRAME_STATS) {
        s->numFrames += 1;
    } else {
        s->firstFrame += 1;
        if (s->firstFrame >= MAX_FRAME_STATS)
            s->firstFrame -= MAX_FRAME_STATS;
    }

    s->lastTime = now;
}


static void my_info_handler (const char *msg, const char *funcname, void *privdata)
{
    LOGI("libvisual INFO: %s: %s\n", __lv_progname, msg);
}
        
static void my_warning_handler (const char *msg, const char *funcname, void *privdata)
{
    if (funcname)
        LOGW("libvisual WARNING: %s: %s(): %s\n",
        __lv_progname, funcname, msg);
    else
    LOGW("libvisual WARNING: %s: %s\n", __lv_progname, msg);
}

static void my_critical_handler (const char *msg, const char *funcname, void *privdata)
{
    if (funcname)
        LOGW("libvisual CRITICAL: %s: %s(): %s\n",
        __lv_progname, funcname, msg);
    else
    LOGW("libvisual CRITICAL: %s: %s\n", __lv_progname, msg);
}

static void my_error_handler (const char *msg, const char *funcname, void *privdata)
{
    if (funcname)
        LOGW("libvisual ERROR: %s: %s(): %s\n",
        __lv_progname, funcname, msg);
    else
    LOGW("libvisual ERROR: %s: %s\n", __lv_progname, msg);
}

VisVideo *new_video(int w, int h, VisVideoDepth depth, void *pixels)
{
    VisVideo *video = visual_video_new();
    visual_video_set_depth(video, depth);
    visual_video_set_dimension(video, w, h);
    visual_video_set_pitch(video, visual_video_bpp_from_depth(depth) * w);
    visual_video_set_buffer(video, pixels);
    return video;
}

static void v_cycleActor (int prev)
{
    v.actor_name = (prev ? visual_actor_get_prev_by_name (v.actor_name)
                     : visual_actor_get_next_by_name (v.actor_name));
    if (!v.actor_name) {
        v.actor_name = (prev ? visual_actor_get_prev_by_name (0)
                         : visual_actor_get_next_by_name (0));
    }

    v.morph_name = visual_morph_get_next_by_name(v.morph_name);
    if(!v.morph_name) {
        v.morph_name = visual_morph_get_next_by_name(0);
    }
}

v_upload_callback (VisInput* input, VisAudio *audio, void* unused)
{
    visual_log_return_if_fail(input != NULL);
    visual_log_return_if_fail(audio != NULL);
    visual_log_return_if_fail(pcm_ref.pcm_data != NULL);

    VisBuffer buf;

    visual_buffer_init( &buf, pcm_ref.pcm_data, pcm_ref.size, 0 );
    visual_audio_samplepool_input( audio->samplepool, &buf, pcm_ref.rate, pcm_ref.encoding, pcm_ref.channels);

    return 0;
}


// ---------- INPUT ----------

// Get the VisInput at the requested index.
VisInput *get_input(int index)
{
    VisInput *input = NULL;
    VisList *list = visual_input_get_list();
    int count = visual_list_count(list);
    if(count <= index)
    {
        input = visual_list_get(list, index);
    }
    return input;
}

// Get the count of available input plugins.
JNIEXPORT jint JNICALL Java_com_starlon_froyvisuals_FroyVisuals_inputCount(JNIEnv *env, jobject obj)
{
    return visual_list_count(visual_input_get_list());
}

// Get the input's plugin name.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_inputGetName(JNIEnv *env, jobject obj, jint index)
{
    VisInput *input = get_input(index);
    char *plugname = NULL;
    if(input)
    {
        plugname = input->plugin->info->plugname;
    }
    return plugname;
}

// Get the input's plugin longname.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_inputGetLongName(JNIEnv *env, jobject obj, jint index)
{
    VisInput *input = get_input(index);
    char *plugname = NULL;
    if(input)
    {
        plugname = input->plugin->info->name;
    }
    return plugname;
}

// Get the input's plugin author.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_inputGetAuthor(JNIEnv *env, jobject obj, jint index)
{
    VisInput *input = get_input(index);
    char *author = NULL;
    if(input)
    {
        author = input->plugin->info->author;
    }
    return author;
}

// Get the input's plugin version.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_inputGetVersion(JNIEnv *env, jobject obj, jint index)
{
    VisInput *input = get_input(index);
    char *version = NULL;
    if(input)
    {
        version = input->plugin->info->version;
    }
    return version;
}

// Get the input's plugin about string.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_inputGetAbout(JNIEnv *env, jobject obj, jint index)
{
    VisInput *input = get_input(index);
    char *about = NULL;
    if(input)
    {
        about = input->plugin->info->about;
    }
    return about;
}

// Get the input's plugin help string.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_inputGetHelp(JNIEnv *env, jobject obj, jint index)
{
    VisInput *input = get_input(index);
    char *help = NULL;
    if(input)
    {
        help = input->plugin->info->help;
    }
    return help;
}

// Get the input's plugin license string.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_inputGetLicense(JNIEnv *env, jobject obj, jint index)
{
    VisInput *input = get_input(index);
    char *license = NULL;
    if(input)
    {
        license = input->plugin->info->license;
    }
    return license;
}



// ------ MORPH ------

// Get the VisMorph at the requested index.
VisMorph *get_morph(int index)
{
    VisMorph *morph = NULL;
    VisList *list = visual_morph_get_list();
    int count = visual_list_count(list);
    if(count <= index)
    {
        morph = visual_list_get(list, index);
    }
    return morph;
}


// Get the count of available morph plugins.
JNIEXPORT jint JNICALL Java_com_starlon_froyvisuals_FroyVisuals_morphCount(JNIEnv *env, jobject obj)
{
    return visual_list_count(visual_morph_get_list());
}

// Get the morph plugin's name string.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_morphGetName(JNIEnv *env, jobject obj, jint index)
{
    VisMorph *morph = get_morph(index);
    char *plugname = NULL;
    if(morph)
    {
        plugname = morph->plugin->info->plugname;
    }
    return plugname;
}

// Get the morph plugin's long name string.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_morphGetLongName(JNIEnv *env, jobject obj, jint index)
{
    VisMorph *morph = get_morph(index);
    char *name = NULL;
    if(morph)
    {
        name = morph->plugin->info->name;
    }
    return name;
}

// Get the morph plugin's author string.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_morphGetAuthor(JNIEnv *env, jobject obj, jint index)
{
    VisMorph *morph = get_morph(index);
    char *author = NULL;
    if(morph)
    {
        author = morph->plugin->info->author;
    }
    return author;
}

// Get the morph plugin's version string.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_morphGetVersion(JNIEnv *env, jobject obj, jint index)
{
    VisMorph *morph = get_morph(index);
    char *version = NULL;
    if(morph)
    {
        version = morph->plugin->info->version;
    }
    return version;
}

// Get the morph plugin's about string.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_morphGetAbout(JNIEnv *env, jobject obj, jint index)
{
    VisMorph *morph = get_morph(index);
    char *about = NULL;
    if(morph)
    {
        about = morph->plugin->info->about;
    }
    return about;
}

// Get the morph plugin's help string.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_morphGetHelp(JNIEnv *env, jobject obj, jint index)
{
    VisMorph *morph = get_morph(index);
    char *help = NULL;
    if(morph)
    {
        help = morph->plugin->info->help;
    }
    return help;
}

// Get the morph plugin's license string.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_morphGetLicense(JNIEnv *env, jobject obj, jint index)
{
    VisMorph *morph = get_morph(index);
    char *license = NULL;
    if(morph)
    {
        license = morph->plugin->info->license;
    }
    return license;
}



// ------ ACTORS ------

// Get the VisActor at the requested index.
VisActor *get_actor(int index)
{
    VisActor *actor = NULL;
    VisList *list = visual_actor_get_list();
    int count = visual_list_count(list);
    if(count <= index)
    {
        actor = visual_list_get(list, index);
    }
    return actor;
}

// Get the count of available actor plugins.
JNIEXPORT jint JNICALL Java_com_starlon_froyvisuals_FroyVisuals_actorCount(JNIEnv *env, jobject obj)
{
    return visual_list_count(visual_actor_get_list());
}

// Get the actor's plugin name.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_actorGetName(JNIEnv *env, jobject obj, jint index)
{
    VisActor *actor = get_actor(index);
    char *plugname = NULL;
    if(actor)
    {
        plugname = actor->plugin->info->plugname;
    }
    return plugname;
}

// Get the actor's long name.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_actorGetLongName(JNIEnv *env, jobject obj, jint index)
{
    VisActor *actor = get_actor(index);
    char *name = NULL;
    if(actor)
    {
        name = actor->plugin->info->name;
    }
    return name;
}

// Get the actor's author.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_actorGetAuthor(JNIEnv *env, jobject obj, jint index)
{
    VisActor *actor = get_actor(index);
    char *author = NULL;
    if(actor)
    {
        author = actor->plugin->info->author;
    }
    return author;
}

// Get the actor's version string.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_actorGetVersion(JNIEnv *env, jobject obj, jint index)
{
    VisActor *actor = get_actor(index);
    char *version = NULL;
    if(actor)
    {
        version = actor->plugin->info->version;
    }
    return version;
}

// Get the actor's about string.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_actorGetAbout(JNIEnv *env, jobject obj, jint index)
{
    VisActor *actor = get_actor(index);
    char *about = NULL;
    if(actor)
    {
        about = actor->plugin->info->about;
    }
    return about;
}

// Get the actor's help string.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_actorGetHelp(JNIEnv *env, jobject obj, jint index)
{
    VisActor *actor = get_actor(index);
    char *help = NULL;
    if(actor)
    {
        help = actor->plugin->info->help;
    }
    return help;
}

// Get the actor's license string.
JNIEXPORT jstring JNICALL Java_com_starlon_froyvisuals_FroyVisuals_actorGetLicense(JNIEnv *env, jobject obj, jint index)
{
    VisActor *actor = get_actor(index);
    char *license = NULL;
    if(actor)
    {
        license = actor->plugin->info->license;
    }
    return license;
}

// For fallback audio source.
JNIEXPORT void JNICALL Java_com_starlon_froyvisuals_FroyVisualsView_uploadAudio(JNIEnv * env, jobject  obj, jshortArray data)
{
    int i;
    jshort *pcm;
    jsize len = (*env)->GetArrayLength(env, data);
    pcm = (*env)->GetShortArrayElements(env, data, NULL);
    for(i = 0; i < len && i < pcm_ref.size; i++)
    {
        pcm_ref.pcm_data[i] = pcm[i];
    }
    (*env)->ReleaseShortArrayElements(env, data, pcm, 0);
}


JNIEXPORT void JNICALL Java_com_starlon_froyvisuals_FroyVisualsView_resizePCM(jint size, jint samplerate, jint channels, jint encoding)
{
    if(pcm_ref.pcm_data)
        visual_mem_free(pcm_ref.pcm_data);
    pcm_ref.pcm_data = visual_mem_malloc(sizeof(int16_t) * size);
    pcm_ref.size = size;
    switch(samplerate)
    {
        case 8000:
            pcm_ref.rate = VISUAL_AUDIO_SAMPLE_RATE_8000;
        break;
        case 11250:
            pcm_ref.rate = VISUAL_AUDIO_SAMPLE_RATE_11250;
        break;
        case 22500:
            pcm_ref.rate = VISUAL_AUDIO_SAMPLE_RATE_22500;
        break;
        case 32000:
            pcm_ref.rate = VISUAL_AUDIO_SAMPLE_RATE_32000;
        break;
        case 44100:
            pcm_ref.rate = VISUAL_AUDIO_SAMPLE_RATE_44100;
        break;
        case 48000:
            pcm_ref.rate = VISUAL_AUDIO_SAMPLE_RATE_48000;
        break;
        case 96000:
            pcm_ref.rate = VISUAL_AUDIO_SAMPLE_RATE_96000;
        break;
    }
    pcm_ref.channels = VISUAL_AUDIO_SAMPLE_CHANNEL_STEREO;
    pcm_ref.encoding = VISUAL_AUDIO_SAMPLE_FORMAT_S16;
}

JNIEXPORT void JNICALL Java_com_starlon_froyvisuals_FroyVisualsView_switchActor(JNIEnv * env, jobject  obj, jboolean prev)
{
    VisMorph *bin_morph = visual_bin_get_morph(v.bin);
    const char *morph = v.morph_name;

    
    if(bin_morph && !visual_morph_is_done(bin_morph))
        return;

    v_cycleActor((int)prev);

    visual_log(VISUAL_LOG_INFO, "Switching actors %s -> %s", morph, v.morph_name);

    if(prev == 0)
        visual_bin_set_morph_by_name (v.bin, (char *)"slide_left");
    else if(prev == 1)
        visual_bin_set_morph_by_name (v.bin, (char *)"slide_right");
    else
        visual_bin_set_morph_by_name (v.bin, (char *)v.morph_name);

    visual_bin_switch_actor_by_name(v.bin, (char *)v.actor_name);
}

JNIEXPORT void JNICALL Java_com_starlon_froyvisuals_FroyVisualsView_mouseMotion(JNIEnv * env, jobject  obj, jfloat x, jfloat y)
{
    visual_log(VISUAL_LOG_INFO, "Mouse motion: x %f, y %f", x, y);
    VisPluginData *plugin = visual_actor_get_plugin(visual_bin_get_actor(v.bin));
    VisEventQueue *eventqueue = visual_plugin_get_eventqueue(plugin);
    visual_event_queue_add_mousemotion(eventqueue, x, y);
}

JNIEXPORT void JNICALL Java_com_starlon_froyvisuals_FroyVisualsView_mouseButton(JNIEnv * env, jobject  obj, jint button, jfloat x, jfloat y)
{
    return;
    visual_log(VISUAL_LOG_INFO, "Mouse button: button %d, x %f, y %f", button, x, y);
    VisPluginData *plugin = visual_actor_get_plugin(visual_bin_get_actor(v.bin));
    VisEventQueue *eventqueue = visual_plugin_get_eventqueue(plugin);
        VisMouseState state = VISUAL_MOUSE_DOWN;
    visual_event_queue_add_mousebutton(eventqueue, button, state, x, y);
}


JNIEXPORT void JNICALL Java_com_starlon_froyvisuals_FroyVisualsView_screenResize(JNIEnv * env, jobject  obj, jint w, jint h)
{
    visual_log(VISUAL_LOG_INFO, "Screen resize w %d h %d", w, h);

    VisPluginData *plugin = visual_actor_get_plugin(visual_bin_get_actor(v.bin));
    VisEventQueue *eventqueue = visual_plugin_get_eventqueue(plugin);
    visual_event_queue_add_resize(eventqueue, v.video, w, h);
}

JNIEXPORT void JNICALL Java_com_starlon_froyvisuals_FroyVisualsView_keyboardEvent(JNIEnv * env, jobject  obj, jint x, jint y)
{
    return;
    VisEventQueue *eventqueue = visual_plugin_get_eventqueue(visual_actor_get_plugin(visual_bin_get_actor(v.bin)));
    VisKey keysym;
    int keymod;
    VisKeyState state;
    visual_event_queue_add_keyboard(eventqueue, keysym, keymod, state);
}

// Is this even needed? What happens when the app is quietly discarded?
// Seems in Android 4.0 you can kill an app by swiping it.
JNIEXPORT void JNICALL Java_com_starlon_froyvisuals_FroyVisualsView_visualsQuit(JNIEnv * env, jobject  obj)
{
    visual_video_free_buffer(v.video);
    visual_object_unref(VISUAL_OBJECT(v.video));
    visual_object_unref(VISUAL_OBJECT(v.bin));
    visual_quit();
}

void app_main(int w, int h)
{
    int depthflag;
    VisVideoDepth depth;

    if(!visual_is_initialized())
    {
        visual_init_path_add("/data/data/com.starlon.froyvisuals/lib");
    	visual_log_set_verboseness (VISUAL_LOG_VERBOSENESS_HIGH);
        visual_log_set_info_handler (my_info_handler, NULL);
        visual_log_set_warning_handler (my_warning_handler, NULL);
        visual_log_set_critical_handler (my_critical_handler, NULL);
        visual_log_set_error_handler (my_error_handler, NULL);
    
    	visual_init (0, NULL);
        memset(&v, 0, sizeof(v));
        memset(&pcm_ref, 0, sizeof(pcm_ref));
    }

    v.morph_name = MORPH;
    v.actor_name = ACTOR;
    v.input_name = INPUT;

	v.bin    = visual_bin_new ();

	if (!visual_actor_valid_by_name (v.actor_name)) {
		visual_log(VISUAL_LOG_CRITICAL, ("Actor plugin not found!"));
        return;
	}

	visual_bin_set_supported_depth (v.bin, VISUAL_VIDEO_DEPTH_ALL);
    visual_bin_set_preferred_depth(v.bin, VISUAL_VIDEO_DEPTH_8BIT);

    VisActor *actor = visual_actor_new((char*)v.actor_name);
    VisInput *input = visual_input_new((char*)v.input_name);

//FIXME For mic input
/*
    VisInput *input = visual_mem_malloc(sizeof( VisInput));
    input->audio = visual_audio_new();
    visual_audio_init(input->audio);

    if (visual_input_set_callback (input, v_upload_callback, NULL) < 0) {
        visual_log (VISUAL_LOG_CRITICAL, "Cannot set input plugin callback");
    }
*/

    depthflag = visual_actor_get_supported_depth(actor);
    depth = visual_video_depth_get_highest(depthflag);

	v.video = visual_video_new();
    visual_video_set_dimension(v.video, w, w);
    visual_video_set_depth(v.video, depth);
    visual_video_set_pitch(v.video, w * visual_video_bpp_from_depth(depth));
    visual_video_allocate_buffer(v.video);
    visual_bin_set_video(v.bin, v.video);

	visual_bin_switch_set_style (v.bin, VISUAL_SWITCH_STYLE_MORPH);
	visual_bin_switch_set_automatic (v.bin, 1);
	visual_bin_switch_set_steps (v.bin, 10);

    visual_bin_connect(v.bin, actor, input);
    if((v.pluginIsGL = (visual_bin_get_depth (v.bin) == VISUAL_VIDEO_DEPTH_GL)))
    {
        visual_video_set_depth(v.video, VISUAL_VIDEO_DEPTH_GL);
        visual_video_free_buffer(v.video);
        visual_video_allocate_buffer(v.video);
    }
	visual_bin_realize (v.bin);
	visual_bin_sync (v.bin, 0);
    visual_bin_depth_changed(v.bin);



	printf ("Libvisual version %s; bpp: %d %s\n", visual_get_version(), v.video->bpp, (v.pluginIsGL ? "(GL)\n" : ""));
}

JNIEXPORT void JNICALL Java_com_starlon_froyvisuals_FroyVisualsView_initApp(JNIEnv * env, jobject  obj, jint w, jint h)
{
    app_main(w, h);
}

JNIEXPORT jboolean JNICALL Java_com_starlon_froyvisuals_FroyVisualsView_render(JNIEnv * env, jobject  obj, jobject bitmap, jint dur)
{
    
    AndroidBitmapInfo  info;
    void*              pixels;
    int                ret;
    static Stats       stats;
    int depthflag;
    VisVideoDepth depth;

    if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
        LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
        return FALSE;
    }

    if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0) {
        LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
        return FALSE;
    }

    stats_startFrame(&stats);

    VisVideo *vid = new_video(info.width, info.height, DEVICE_DEPTH, pixels);

    if(visual_bin_depth_changed(v.bin) || 
        (info.width != v.video->width || 
        info.height != v.video->height) ) 
    {
		v.pluginIsGL = (visual_bin_get_depth (v.bin) == VISUAL_VIDEO_DEPTH_GL);
        depthflag = visual_bin_get_depth(v.bin);
        depth = visual_video_depth_get_highest(depthflag);
        if(v.pluginIsGL)
        {
    		visual_video_set_depth (v.video, VISUAL_VIDEO_DEPTH_GL);
        } else
        {
            visual_video_set_depth(v.video, depth);
        }
        visual_video_set_dimension(v.video, info.width, info.height);
        visual_video_set_pitch(v.video, info.width * visual_video_bpp_from_depth(depth));
        if(visual_video_get_pixels(v.video))
            visual_video_free_buffer(v.video);
        visual_video_allocate_buffer(v.video);
        visual_bin_sync(v.bin, TRUE);
    }

	if (0 && v.pluginIsGL) {
        //FIXME
		//visual_bin_run (v.bin);
	} else {

		visual_bin_run (v.bin);
    }

    //visual_video_blit_overlay(vid, v.video, 0, 0, FALSE);
    visual_video_depth_transform(vid, v.video);

    visual_object_unref(VISUAL_OBJECT(vid));

    AndroidBitmap_unlockPixels(env, bitmap);

    stats_endFrame(&stats);

	return TRUE;
}
