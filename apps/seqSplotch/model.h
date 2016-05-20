
/* Copyright (c) 2011-2014, Stefan Eilemann <eile@eyescale.ch>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of Eyescale Software GmbH nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SEQ_SPLOTCH_MODEL_H
#define SEQ_SPLOTCH_MODEL_H

#include <seq/sequel.h>

#include <splotch/scenemaker.h>
#include <splotch/splotch_host.h>

namespace seqSplotch
{

class Model
{
public:
    explicit Model( const servus::URI& uri );

    void loadNextFrame();

    const std::vector< particle_sim >& getParticles() const;

    seq::Matrix4f getModelMatrix() const;
    const seq::Vector4f& getBoundingSphere() const;

    paramfile& getParams();
    std::vector< COLOURMAP >& getColorMaps();
    float getBrightness() const;
    const std::vector<bool>& getColourIsVec() const;

    size_t getFrameIndex() const;

private:
    void _computeBoundingSphere();

    paramfile _params;
    sceneMaker _sceneMaker;
    std::vector< particle_sim > _particles;
    std::vector< particle_sim > _points;
    std::vector< COLOURMAP > _colorMaps;
    vec3 _cameraPosition;
    vec3 _lookAt;
    vec3 _up;
    bool _boost;
    float _brightness;
    seq::Vector4f _boundingSphere;
    std::vector<bool> _colourIsVec;
    size_t _currentFrame;
};

}

#endif
