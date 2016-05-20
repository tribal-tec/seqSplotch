
/* Copyright (c) 2011-2012, Stefan Eilemann <eile@eyescale.ch>
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

#ifndef SEQ_SPLOTCH_RENDERER_H
#define SEQ_SPLOTCH_RENDERER_H

#include "application.h"
#include <seq/sequel.h>
#include <eq/gl.h>

namespace seqSplotch
{

class Renderer : public seq::Renderer
{
public:
    Renderer( seq::Application& app );
    virtual ~Renderer() {}

protected:
    bool init( co::Object* initData ) final;
    bool exit() final;
    void draw( co::Object* frameData ) final;

    seq::ViewData* createViewData( seq::View& view ) final;
    void destroyViewData( seq::ViewData* viewData ) final;

private:
    void _cpuRender( Model& model );
    void _gpuRender( bool blurOn );
    void _updateModel( Model& model );

    bool _loadShaders();
    bool _createBuffers();
    void _fillBuffers( Model& model );

    lunchbox::Buffer< float > _pixels;

    GLuint _particleShader;
    GLuint _blurShader;
    eq::util::FrameBufferObject* _fbo;
    eq::util::FrameBufferObject* _fboBlur1;
    eq::util::FrameBufferObject* _fboBlur2;
    GLuint _posSSBO;
    GLuint _colorSSBO;
    GLuint _indices;

    size_t _modelFrameIndex;
    size_t _numParticles;
};

}

#endif // SEQ_SPLOTCH_RENDERER_H

