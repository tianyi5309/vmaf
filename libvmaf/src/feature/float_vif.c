/**
 *
 *  Copyright 2016-2020 Netflix, Inc.
 *
 *     Licensed under the BSD+Patent License (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         https://opensource.org/licenses/BSDplusPatent
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */

#include <errno.h>
#include <math.h>
#include <string.h>

#include "feature_collector.h"
#include "feature_extractor.h"
#include "mem.h"

#include "vif.h"
#include "vif_options.h"
#include "picture_copy.h"

typedef struct VifState {
    size_t float_stride;
    float *ref;
    float *dist;
} VifState;

static int init(VmafFeatureExtractor *fex, enum VmafPixelFormat pix_fmt,
                unsigned bpc, unsigned w, unsigned h)
{
    VifState *s = fex->priv;
    s->float_stride = sizeof(float) * w;
    s->ref = aligned_malloc(s->float_stride * h, 32);
    if (!s->ref) goto fail;
    s->dist = aligned_malloc(s->float_stride * h, 32);
    if (!s->dist) goto free_ref;

    return 0;

free_ref:
    free(s->ref);
fail:
    return -ENOMEM;
}

static int extract(VmafFeatureExtractor *fex,
                   VmafPicture *ref_pic, VmafPicture *dist_pic,
                   unsigned index, VmafFeatureCollector *feature_collector)
{
    VifState *s = fex->priv;
    int err = 0;

    picture_copy(s->ref, ref_pic, -128, ref_pic->bpc);
    picture_copy(s->dist, dist_pic, -128, dist_pic->bpc);

    double score, score_num, score_den;
    double scores[8];
    err = compute_vif(s->ref, s->dist, ref_pic->w[0], ref_pic->h[0],
                      s->float_stride, s->float_stride,
                      &score, &score_num, &score_den, scores);
    if (err) return err;

    err = vmaf_feature_collector_append(feature_collector,
                                        "'VMAF_feature_vif_scale0_score'",
                                        scores[0] / scores[1], index);
    if (err) return err;
    err = vmaf_feature_collector_append(feature_collector,
                                        "'VMAF_feature_vif_scale1_score'",
                                        scores[2] / scores[3], index);
    if (err) return err;
    err = vmaf_feature_collector_append(feature_collector,
                                        "'VMAF_feature_vif_scale2_score'",
                                        scores[4] / scores[5], index);
    if (err) return err;
    err = vmaf_feature_collector_append(feature_collector,
                                        "'VMAF_feature_vif_scale3_score'",
                                        scores[6] / scores[7], index);
    if (err) return err;

    return 0;
}

static int close(VmafFeatureExtractor *fex)
{
    VifState *s = fex->priv;
    if (s->ref) aligned_free(s->ref);
    if (s->dist) aligned_free(s->dist);
    return 0;
}

static const char *provided_features[] = {
    "'VMAF_feature_vif_scale0_score'", "'VMAF_feature_vif_scale1_score'",
    "'VMAF_feature_vif_scale2_score'", "'VMAF_feature_vif_scale3_score'",
    NULL
};

VmafFeatureExtractor vmaf_fex_float_vif = {
    .name = "float_vif",
    .init = init,
    .extract = extract,
    .close = close,
    .priv_size = sizeof(VifState),
    .provided_features = provided_features,
};
