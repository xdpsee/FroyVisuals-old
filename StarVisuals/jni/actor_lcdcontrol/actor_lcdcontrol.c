/* Libvisual-plugins - Standard plugins for libvisual
 * 
 * Copyright (C) 2004, 2005, 2006 Dennis Smit <ds@nerds-incorporated.org>
 *
 * Authors: Dennis Smit <ds@nerds-incorporated.org>
 *
 * $Id: actor_starscope.c,v 1.21 2006/01/27 20:19:17 synap Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <gettext.h>
#include <math.h>

#include <libvisual/libvisual.h>

#define PCM_SIZE	2048

typedef struct {
    VisPalette pal;
	VisBuffer	pcm;
    VisTimer timer;
} ScopePrivate;

int starscope_init (VisPluginData *plugin);
int starscope_cleanup (VisPluginData *plugin);
int starscope_requisition (VisPluginData *plugin, int *width, int *height);
int starscope_dimension (VisPluginData *plugin, VisVideo *video, int width, int height);
int starscope_events (VisPluginData *plugin, VisEventQueue *events);
VisPalette *starscope_palette (VisPluginData *plugin);
int starscope_render (VisPluginData *plugin, VisVideo *video, VisAudio *audio);

VISUAL_PLUGIN_API_VERSION_VALIDATOR

const VisPluginInfo *get_plugin_info (int *count)
{
	static VisActorPlugin actor[] = {{
		.requisition = starscope_requisition,
		.palette = starscope_palette,
		.render = starscope_render,
		.vidoptions.depth = VISUAL_VIDEO_DEPTH_8BIT
	}};

	static VisPluginInfo info[] = {{
		.type = VISUAL_PLUGIN_TYPE_ACTOR,

		.plugname = "starscope",
		.name = "StarScope",
		.author = "Dennis Smit <ds@nerds-incorporated.org> with additions by Scott Sibley <sisibley@gmail.com>",
		.version = "0.1",
		.about = "Libvisual star scope plugin",
		.help = "This is a simple scope with a starfield behind it.",

		.init = starscope_init,
		.cleanup = starscope_cleanup,
		.events = starscope_events,

		.plugin = VISUAL_OBJECT (&actor[0])
	}};

	*count = sizeof (info) / sizeof (*info);

	return info;
}

int starscope_init (VisPluginData *plugin)
{
	ScopePrivate *priv;

#if ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
#endif

	priv = visual_mem_new0 (ScopePrivate, 1);
	visual_object_set_private (VISUAL_OBJECT (plugin), priv);

	visual_palette_allocate_colors (&priv->pal, 256);

	visual_buffer_init_allocate (&priv->pcm, sizeof (float) * PCM_SIZE, visual_buffer_destroyer_free);

    visual_timer_init(&priv->timer);

	return 0;
}

int starscope_cleanup (VisPluginData *plugin)
{
	ScopePrivate *priv = visual_object_get_private (VISUAL_OBJECT (plugin));

	visual_palette_free_colors (&priv->pal);

	visual_object_unref (VISUAL_OBJECT (&priv->pcm));

	visual_mem_free (priv);

	return 0;
}

int starscope_requisition (VisPluginData *plugin, int *width, int *height)
{
	int reqw, reqh;

	reqw = *width;
	reqh = *height;

	while (reqw % 2 || (reqw / 2) % 2)
		reqw--;

	while (reqh % 2 || (reqh / 2) % 2)
		reqh--;

	if (reqw < 32)
		reqw = 32;

	if (reqh < 32)
		reqh = 32;

	*width = reqw;
	*height = reqh;

	return 0;
}

int starscope_dimension (VisPluginData *plugin, VisVideo *video, int width, int height)
{
	visual_video_set_dimension (video, width, height);

	return 0;
}

int starscope_events (VisPluginData *plugin, VisEventQueue *events)
{
	VisEvent ev;

	while (visual_event_queue_poll (events, &ev)) {
		switch (ev.type) {
			case VISUAL_EVENT_RESIZE:
				starscope_dimension (plugin, ev.event.resize.video,
						ev.event.resize.width, ev.event.resize.height);
				break;
			default: /* to avoid warnings */
				break;
		}
	}

	return 0;
}

VisPalette *starscope_palette (VisPluginData *plugin)
{
	ScopePrivate *priv = visual_object_get_private (VISUAL_OBJECT (plugin));
	int i;

	for (i = 0; i < 256; i++) {
		priv->pal.colors[i].r = i;
		priv->pal.colors[i].g = i;
		priv->pal.colors[i].b = i;
	}

	return &priv->pal;
}

int starscope_render (VisPluginData *plugin, VisVideo *video, VisAudio *audio)
{
	ScopePrivate *priv = visual_object_get_private (VISUAL_OBJECT (plugin));

	visual_audio_get_sample_mixed (audio, &priv->pcm, TRUE, 2,
			VISUAL_AUDIO_CHANNEL_LEFT,
			VISUAL_AUDIO_CHANNEL_RIGHT,
			1.0,
			1.0);

	int16_t *pcmbuf = visual_buffer_get_data (&priv->pcm);

	uint8_t *buf = (uint8_t *) visual_video_get_pixels (video);


	return 0;
}

