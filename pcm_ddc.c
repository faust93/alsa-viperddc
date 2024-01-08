/*
 * by faust93 at <monumentum@gmail.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <stdio.h>
#include <alsa/asoundlib.h>
#include <alsa/pcm.h>
#include <alsa/pcm_external.h>
#include <alsa/control.h>
#include <linux/soundcard.h>

#include "vdc.h"

//#define DEBUG

typedef struct snd_pcm_ddc {
    snd_pcm_extplug_t ext;
    char ddc_enable;
    char ddc_file[128];
    DirectForm2 **df441, **df48, **sosPointer;
    int sosCount, usedSOSCount;
    int samplerate;
    int channels;
} snd_pcm_ddc_t;


static snd_pcm_sframes_t ddc_transfer(snd_pcm_extplug_t *ext,
    const snd_pcm_channel_area_t *dst_areas,
    snd_pcm_uframes_t dst_offset,
    const snd_pcm_channel_area_t *src_areas,
    snd_pcm_uframes_t src_offset,
    snd_pcm_uframes_t size)
{
    snd_pcm_ddc_t *ddc = (snd_pcm_ddc_t *)ext;
    float *src, *dst;

    /* Calculate buffer locations */
    src = (float*)(src_areas->addr + (src_areas->first + src_areas->step * src_offset)/8);
    dst = (float*)(dst_areas->addr + (dst_areas->first + dst_areas->step * dst_offset)/8);

    if (ddc->ddc_enable) {
        for(long int i = 0; i < size; i++)
        {
            for (int j = 0; j < ddc->usedSOSCount; j++) {
                SOS_DF2_Float_StereoProcess(ddc->sosPointer[j], src[i << 1], src[(i << 1) + 1],
                                            &src[i << 1], &src[(i << 1) + 1]);
            }
        }
    }
    memcpy(dst, src, snd_pcm_frames_to_bytes(ext->pcm, size));
    return size;
}

char *memory_read_ascii(char *path) {
    int c;
    long size;
    FILE *file;
    int i = 0;
    file = fopen(path, "r");
    if (file) {
        fseek(file, 0, SEEK_END);
        size = ftell(file);
        fseek(file, 0, SEEK_SET);
        char *buffer = (char *) malloc(size * sizeof(char));
        while ((c = getc(file)) != EOF) {
            buffer[i] = (char) c;
            i++;
        }
        fclose(file);
        return buffer;
    }
    return NULL;
}

static int ddc_close(snd_pcm_extplug_t *ext) {
    snd_pcm_ddc_t *ddc = ext->private_data;
    if (ddc->sosCount) {
        for (int i = 0; i < ddc->sosCount; i++) {
            free(ddc->df441[i]);
            free(ddc->df48[i]);
        }
        free(ddc->df441);
        ddc->df441 = 0;
        free(ddc->df48);
        ddc->df48 = 0;
        ddc->sosCount = 0;
        ddc->sosPointer = 0;
    }
    free(ddc);
    return 0;
}

static int ddc_init(snd_pcm_extplug_t *ext)
{
    snd_pcm_ddc_t *ddc = (snd_pcm_ddc_t *)ext;

    ddc->ddc_enable = 1;
    ddc->samplerate = ext->rate;
#ifdef DEBUG
    printf("samplerate: %d\n", ddc->samplerate);
    printf("ddc file: %s\n", ddc->ddc_file);
#endif
    char *ddcString = memory_read_ascii(ddc->ddc_file);
    if (!ddcString || ddcString == NULL) {
        SNDERR("Unable to open DDC file");
        return -EINVAL;
    }

    int d = 0;
    for (int i = 0; i < strlen(ddcString); ++i) {
        d |= ddcString[i];
    }
    if (d == 0) {
        SNDERR("DDC contents contain no data");
        if (ddcString != NULL) {
            free(ddcString);
        }
        return -EINVAL;
    }

    int begin = strcspn(ddcString, "S");
    if (strcspn(ddcString, "R") != begin + 1) { //check for 'SR' in the string
        SNDERR("Invalid DDC string");
        if (ddcString != NULL) {
            free(ddcString);
        }
        return -EINVAL;
    }

    ddc->sosCount = DDCParser(ddcString, &ddc->df441, &ddc->df48);
#ifdef DEBUG
    printf("SOS count %d\n", ddc->sosCount);
#endif
    if(ddc->sosCount < 1){
        SNDERR("SOS count is zero");
    }

    if (ddc->samplerate == 44100 && ddc->df441) {
        ddc->sosPointer = ddc->df441;
        ddc->usedSOSCount = ddc->sosCount;
    } else if (ddc->samplerate == 48000 && ddc->df48) {
        ddc->sosPointer = ddc->df48;
        ddc->usedSOSCount = ddc->sosCount;
    } else {
        SNDERR("Invalid sampling rate, only 44.1 & 48.0 are supported");
    }

    if (ddcString != NULL) {
        free(ddcString);
    }
#ifdef DEBUG
    printf("VDC num of SOS: %d\n", ddc->sosCount);
    printf("VDC df48[0].b0: %1.14f\n", (float) ddc->df48[0]->b0);
#endif
    return 0;
}

static snd_pcm_extplug_callback_t ddc_callback = {
    .transfer = ddc_transfer,
    .init = ddc_init,
    .close = ddc_close,
};

SND_PCM_PLUGIN_DEFINE_FUNC(ddc)
{
    snd_config_iterator_t i, next;
    snd_pcm_ddc_t *ddc;
    snd_config_t *sconf = NULL;
    const char *ddc_file = "V4ARISE.vdc";
    long channels = 2;
    int err;

    snd_config_for_each(i, next, conf) {
        snd_config_t *n = snd_config_iterator_entry(i);
        const char *id;
        if (snd_config_get_id(n, &id) < 0)
            continue;
        if (strcmp(id, "comment") == 0 || strcmp(id, "type") == 0 || strcmp(id, "hint") == 0)
            continue;
        if (strcmp(id, "slave") == 0) {
            sconf = n;
            continue;
        }
        if (strcmp(id, "ddc_file") == 0) {
            snd_config_get_string(n, &ddc_file);
            continue;
        }
        if (strcmp(id, "channels") == 0) {
            snd_config_get_integer(n, &channels);
            if (channels != 2) {
                SNDERR("Only stereo streams supported");
                return -EINVAL;
            }
            continue;
        }
        SNDERR("Unknown field %s", id);
        return -EINVAL;
    }

    if (! sconf) {
        SNDERR("No slave configuration for ddc pcm");
        return -EINVAL;
    }

    ddc = calloc(1, sizeof(*ddc));
    if (ddc == NULL)
        return -ENOMEM;

    ddc->ext.version = SND_PCM_EXTPLUG_VERSION;
    ddc->ext.name = "viperddc";
    ddc->ext.callback = &ddc_callback;
    ddc->ext.private_data = ddc;
    ddc->channels = channels;
    strncpy(ddc->ddc_file, ddc_file, 128);
    if(ddc->ddc_file == NULL) {
        free(ddc);
        return -1;
    }

    err = snd_pcm_extplug_create(&ddc->ext, name, root, sconf, stream, mode);
    if (err < 0) {
        free(ddc);
        return err;
    }

    snd_pcm_extplug_set_param_minmax(&ddc->ext, SND_PCM_EXTPLUG_HW_CHANNELS, channels, channels);
    snd_pcm_extplug_set_slave_param(&ddc->ext, SND_PCM_EXTPLUG_HW_CHANNELS, channels);
    snd_pcm_extplug_set_param(&ddc->ext, SND_PCM_EXTPLUG_HW_FORMAT, SND_PCM_FORMAT_FLOAT);
    snd_pcm_extplug_set_slave_param(&ddc->ext, SND_PCM_EXTPLUG_HW_FORMAT, SND_PCM_FORMAT_FLOAT);

    *pcmp = ddc->ext.pcm;

    return 0;

}

SND_PCM_PLUGIN_SYMBOL(ddc);
