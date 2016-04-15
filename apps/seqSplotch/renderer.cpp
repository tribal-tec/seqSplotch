
/* Copyright (c) 2011-2015, Stefan Eilemann <eile@eyescale.ch>
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

#include "renderer.h"

#include "model.h"
#include "viewData.h"

#ifdef CUDA
#  include <splotch/cuda/cuda_splotch.h>
#else
#  include <splotch/splotch_host.h>
#endif

#include <particles.frag.h>
#include <particles.vert.h>
#include <blur.frag.h>
#include <blur_pass.vert.h>

//#define BLUR

namespace seqSplotch
{

Renderer::Renderer( seq::Application& app )
    : seq::Renderer( app )
    , _posSSBO( 0 )
{}

bool Renderer::init( co::Object* initData )
{
    return seq::Renderer::init( initData );
}

bool Renderer::initContext( co::Object* initData )
{
    if( !seq::Renderer::initContext( initData ))
        return false;

    if( !_loadShaders( ))
        return false;

    glEnable(GL_PROGRAM_POINT_SIZE_EXT);

    return true;
}

bool Renderer::exitContext()
{
    seq::ObjectManager& om = getObjectManager();
    om.deleteEqFrameBufferObject( &_fbo );
    om.deleteEqFrameBufferObject( &_fboBlur1 );
    om.deleteEqFrameBufferObject( &_fboBlur2 );
    om.deleteProgram( &_particleShader );
    om.deleteProgram( &_blurShader );
    om.deleteBuffer( &_posSSBO );
    om.deleteBuffer( &_colorSSBO );
    om.deleteBuffer( &_indices );

    return seq::Renderer::exitContext();
}

bool Renderer::_loadShaders()
{
    seq::ObjectManager& om = getObjectManager();
    _particleShader = om.newProgram( &_particleShader );
    if( !seq::linkProgram( om.glewGetContext(), _particleShader, particles_vert,
                           particles_frag ))
    {
        om.deleteProgram( &_particleShader );
        return false;
    }

    _blurShader = om.newProgram( &_blurShader );
    if( !seq::linkProgram( om.glewGetContext(), _blurShader, blur_pass_vert,
                           blur_frag ))
    {
        om.deleteProgram( &_blurShader );
        return false;
    }

    return true;
}

bool Renderer::exit()
{
    return seq::Renderer::exit();
}

void Renderer::cpuRender( Model& model )
{
    seq::Vector3f eye;
    seq::Vector3f lookAt;
    seq::Vector3f up;
    getModelMatrix().getLookAt( eye, lookAt, up );

    const int width = getPixelViewport().w;
    const int height = getPixelViewport().h;

    paramfile& params = model.getParams();
    params.setParam( "xres", width );
    params.setParam( "yres", height );

    arr2<COLOUR> pic( width, height );

    auto particles = model.getParticles();
#ifdef CUDA
    cuda_rendering( 0, 1, pic, particles,
                    vec3( eye.x(), eye.y(), eye.z()),
                    vec3( eye.x(), eye.y(), eye.z()),
                    vec3( lookAt.x(), lookAt.y(), lookAt.z()),
                    vec3( up.x(), up.y(), up.z()),
                    model.amap, model.b_brightness, params );
#else
    host_rendering( params, particles, pic,
                    vec3( eye.x(), eye.y(), eye.z()),
                    vec3( eye.x(), eye.y(), eye.z()),
                    vec3( lookAt.x(), lookAt.y(), lookAt.z()),
                    vec3( up.x(), up.y(), up.z()),
                    model.getColorMaps(), model.getBrightness(), particles.size( ));
#endif

    bool a_eq_e = params.find<bool>("a_eq_e",true);
    if( a_eq_e )
    {
        exptable<float32> xexp(-20.0);
#pragma omp parallel for
      for (int ix=0;ix<width;ix++)
        for (int iy=0;iy<height;iy++)
          {
          pic[ix][iy].r=-xexp.expm1(pic[ix][iy].r);
          pic[ix][iy].g=-xexp.expm1(pic[ix][iy].g);
          pic[ix][iy].b=-xexp.expm1(pic[ix][iy].b);
          }
    }

    const double gamma = params.find<double>("pic_gamma",1.0);
    const double helligkeit = params.find<double>("pic_brighness",0.0);
    const double kontrast = params.find<double>("pic_contrast",1.0);

    if( gamma != 1.0 || helligkeit != 0.0 || kontrast != 1.0 )
    {
#pragma omp parallel for
        for( tsize i = 0; i < pic.size1(); ++i )
        {
            for( tsize j = 0; j < pic.size2(); ++j )
            {
                pic[i][j].r = kontrast * pow((double)pic[i][j].r,gamma) + helligkeit;
                pic[i][j].g = kontrast * pow((double)pic[i][j].g,gamma) + helligkeit;
                pic[i][j].b = kontrast * pow((double)pic[i][j].b,gamma) + helligkeit;
            }
       }
    }

    _pixels.grow( width*height*3 );
    int k = 0;
    for( int j = 0; j < height; ++j )
    {
        for( int i = 0; i < width; ++i, k+=3 )
            memcpy( &_pixels[k], &pic[i][j], 3 * sizeof( float ));
    }

    glWindowPos2i( 0, 0 );
    glDrawPixels( width, height, GL_RGB, GL_FLOAT, _pixels.getData( ));
}

void Renderer::gpuRender( Model& model )
{
    auto particles = model.getParticles();
    const eq::PixelViewport& pvp = getPixelViewport();

    if( !_posSSBO )
    {
        int xres = pvp.w;
        int yres = pvp.h;

        seq::ObjectManager& om = getObjectManager();

        _fbo = om.newEqFrameBufferObject( &_fbo );
        if( _fbo->init( xres, yres, GL_RGBA, 24, 0 ) != eq::ERROR_NONE )
        {
            om.deleteEqFrameBufferObject( &_fbo );
            return;
        }

        _fboBlur1 = om.newEqFrameBufferObject( &_fboBlur1 );
        if( _fboBlur1->init( xres, yres, GL_RGBA, 24, 0 ) != eq::ERROR_NONE )
        {
            om.deleteEqFrameBufferObject( &_fboBlur1 );
            return;
        }

        _fboBlur2 = om.newEqFrameBufferObject( &_fboBlur2 );
        if( _fboBlur2->init( xres, yres, GL_RGBA, 24, 0 ) != eq::ERROR_NONE )
        {
            om.deleteEqFrameBufferObject( &_fboBlur2 );
            return;
        }

        for (size_t i = 0; i < particles.size(); i++)
        {
            // Generate colour in same way splotch does (Add brightness here):
            if (!model.getColourIsVec()[particles[i].type])
                particles[i].e = model.getColorMaps()[particles[i].type].getVal_const(particles[i].e.r) * particles[i].I;
            else
                particles[i].e *= particles[i].I;
        }

        _posSSBO = om.newBuffer( &_posSSBO );
        EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, _posSSBO ));
        EQ_GL_CALL( glBufferData( GL_SHADER_STORAGE_BUFFER,
                                  sizeof(seq::Vector4f) * particles.size(), 0, GL_STATIC_DRAW ));
        EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 ));

        _colorSSBO = om.newBuffer( &_colorSSBO );
        EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, _colorSSBO ));
        EQ_GL_CALL( glBufferData( GL_SHADER_STORAGE_BUFFER,
                                  sizeof(seq::Vector4f) * particles.size(), 0, GL_STATIC_DRAW ));
        EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 ));

        std::vector<uint32_t> indices(particles.size() * 6);
        for (size_t i = 0, j = 0; i < particles.size(); ++i)
        {
            size_t index = i << 2;
            indices[j++] = index;
            indices[j++] = index + 1;
            indices[j++] = index + 2;
            indices[j++] = index;
            indices[j++] = index + 2;
            indices[j++] = index + 3;
        }

        _indices = om.newBuffer( &_indices );
        EQ_GL_CALL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, _indices ));
        EQ_GL_CALL( glBufferData( GL_ELEMENT_ARRAY_BUFFER,
                                  sizeof(uint32_t) * indices.size(), indices.data(), GL_STATIC_DRAW ));
        EQ_GL_CALL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 ));

        EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, _posSSBO ));
        seq::Vector4f* pos = reinterpret_cast< seq::Vector4f* >( glMapBuffer( GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY ));

        EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, _colorSSBO ));
        seq::Vector4f* color = reinterpret_cast< seq::Vector4f* >( glMapBuffer( GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY ));

        for( size_t i = 0; i < particles.size(); ++i )
        {
            pos[i] = seq::Vector4f( particles[i].x, particles[i].y, particles[i].z, 1.0f );
            color[i] = seq::Vector4f( particles[i].e.r, particles[i].e.g, particles[i].e.b, 1.0f );
        }

        EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, _posSSBO ));
        EQ_GL_CALL( glUnmapBuffer( GL_SHADER_STORAGE_BUFFER ));

        EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, _colorSSBO ));
        EQ_GL_CALL( glUnmapBuffer( GL_SHADER_STORAGE_BUFFER ));

        EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 ));
    }

    float brightnessMod = 1.0;
    float saturation = 1.0;
    float contrast   = 1.0;
    float particleSize = 0.6;
    float nParticleSize = particleSize * 1;

#ifdef BLUR
    float blurStrength = 0.13f;
    float blurColorModifier = 1.2f;

    bool blurOn = true;
#endif

    EQ_GL_CALL( glUseProgram( _particleShader ));
    GLint loc = glGetUniformLocation( _particleShader, "brightnessMod" );
    EQ_GL_CALL( glUniform1f( loc, brightnessMod ));

    loc = glGetUniformLocation( _particleShader, "saturation" );
    EQ_GL_CALL( glUniform1f( loc, saturation ));

    loc = glGetUniformLocation( _particleShader, "contrast" );
    EQ_GL_CALL( glUniform1f( loc, contrast ));

    loc = glGetUniformLocation( _particleShader, "iparticleSize" );
    EQ_GL_CALL( glUniform1f( loc, nParticleSize ));

    {
        EQ_GL_CALL( glViewport( pvp.x, pvp.y, pvp.w, pvp.h ));

#ifdef BLUR
        _fbo->bind();
#endif

        EQ_GL_CALL( glClearColor( 0.9, 0.2, 0.2, 1.0 ));
        EQ_GL_CALL( glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ));

        const seq::Matrix4f p = getFrustum().computePerspectiveMatrix();
        const seq::Matrix4f mv = /*getFrustum().computePerspectiveMatrix() **/
                                  getViewMatrix() * getModelMatrix();

        loc = glGetUniformLocation( _particleShader, "ciProjectionMatrix" );
        EQ_GL_CALL( glUniformMatrix4fv( loc, 1, GL_FALSE, p.data( )));

        loc = glGetUniformLocation( _particleShader, "ciModelView" );
        EQ_GL_CALL( glUniformMatrix4fv( loc, 1, GL_FALSE, mv.data( )));

        EQ_GL_CALL( glEnable( GL_BLEND ));
        EQ_GL_CALL( glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ));
        EQ_GL_CALL( glHint( GL_LINE_SMOOTH_HINT, GL_FASTEST ));

        {
            EQ_GL_CALL( glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, _posSSBO ));
            EQ_GL_CALL( glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, _colorSSBO ));
            EQ_GL_CALL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, _indices ));
            EQ_GL_CALL( glDrawElements( GL_TRIANGLES, particles.size() * 6, GL_UNSIGNED_INT, 0 ));
            EQ_GL_CALL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 ));
        }

        EQ_GL_CALL( glDisable( GL_BLEND ));
        EQ_GL_CALL( glUseProgram( 0 ));

#ifdef BLUR
        _fbo->unbind();
#endif
    }

#ifdef BLUR
    if (blurOn)
    {

        {
            gl::ScopedViewport scpVp(0, 0, mFboBlur1->getWidth(), mFboBlur1->getHeight());
            gl::ScopedFramebuffer fbScp(mFboBlur1);
            gl::ScopedGlslProg scopedblur(_blurShader);
            gl::ScopedTextureBind tex0(mFbo->getColorTexture(), (uint8_t)0);
            gl::setMatricesWindowPersp(mFboBlur1->getWidth(), mFboBlur1->getHeight());

            gl::clear(Color::black());

            _blurShader->uniform("tex0", 0);

            _blurShader->uniform("sampleOffset", ci::vec2(blurStrength / mFboBlur1->getWidth(), 0.0f));
            _blurShader->uniform("colorModifier", blurColorModifier);

            gl::drawSolidRect(mFboBlur1->getBounds());
        }

        {
            gl::ScopedViewport scpVp(0, 0, mFboBlur2->getWidth(), mFboBlur2->getHeight());
            gl::ScopedFramebuffer fbScp(mFboBlur2);
            gl::ScopedGlslProg scopedblur(_blurShader);
            gl::ScopedTextureBind tex0(mFboBlur1->getColorTexture(), (uint8_t)0);
            gl::setMatricesWindowPersp(mFboBlur2->getWidth(), mFboBlur2->getHeight());

            gl::clear(Color::black());

            _blurShader->uniform("tex0", 0);

            _blurShader->uniform("sampleOffset", ci::vec2(0.0f, blurStrength / mFboBlur2->getHeight()));
            _blurShader->uniform("colorModifier", blurColorModifier);

            gl::drawSolidRect(mFboBlur2->getBounds());
        }
    }

    gl::setMatrices(mCam);
    EQ_GL_CALL( glViewport( pvp.x, pvp.y, pvp.w, pvp.h ));
    //gl::setMatricesWindow(getWindowSize());
    //gl::draw(mFbo->getColorTexture(), getWindowBounds());
    _fbo->bind( GL_READ_FRAMEBUFFER_EXT );
    _fbo->getColorTextures()[0]->writeRGB( "/tmp/bla" );
    EQ_GL_CALL( glBindFramebufferEXT( GL_DRAW_FRAMEBUFFER_EXT, 0 ));
    EQ_GL_CALL( glBlitFramebuffer( pvp.x, pvp.y, pvp.w, pvp.h,
                                   pvp.x, pvp.y, pvp.w, pvp.h,
                                   GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                                   GL_STENCIL_BUFFER_BIT, GL_NEAREST ));
    _fbo->unbind();

    if (blurOn)
    {
        gl::enableAdditiveBlending();
        gl::draw(mFboBlur2->getColorTexture(), getWindowBounds());
        gl::disableAlphaBlending();
    }
#endif
}

seq::ViewData* Renderer::createViewData( seq::View& view )
{
    return new ViewData( view );
}

void Renderer::destroyViewData( seq::ViewData* viewData )
{
    delete viewData;
}

void Renderer::draw( co::Object* /*frameDataObj*/ )
{
    const ViewData* viewData = static_cast< const ViewData* >( getViewData( ));

    Application& application = static_cast< Application& >( getApplication( ));
    Model& model = application.getModel();

    applyRenderContext(); // set up OpenGL State

    if( viewData->useCPURendering( ))
        cpuRender( model );
    else
        gpuRender( model );
}

}
