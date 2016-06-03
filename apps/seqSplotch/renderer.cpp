
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
#include <blur.vert.h>

#ifdef SEQSPLOTCH_USE_OSPRAY
#  include "osprayRenderer.h"
#endif

namespace seqSplotch
{

Renderer::Renderer( seq::Application& app )
    : seq::Renderer( app )
    , _particleShader( 0 )
    , _blurShader( 0 )
    , _fbo( nullptr )
    , _fboBlur1( nullptr )
    , _fboBlur2( nullptr )
    , _posSSBO( 0 )
    , _colorSSBO( 0 )
    , _indices( 0 )
    , _rectVBO( 0 )
    , _modelFrameIndex( std::numeric_limits< size_t >::max( ))
    , _numParticles( 0 )
{}

bool Renderer::init( co::Object* initData )
{
    if( !seq::Renderer::init( initData ))
        return false;

    if( !_loadShaders( ))
        return false;

    if( !_createBuffers( ))
        return false;

    return true;
}

bool Renderer::exit()
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
    om.deleteBuffer( &_rectVBO );

    return seq::Renderer::exit();
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
    if( !seq::linkProgram( om.glewGetContext(), _blurShader, blur_vert,
                           blur_frag ))
    {
        om.deleteProgram( &_blurShader );
        return false;
    }

    EQ_GL_CALL( glUseProgram( _blurShader ));
    const GLint loc = glGetUniformLocation( _blurShader, "tex0" );
    EQ_GL_CALL( glUniform1i( loc, 0 ));
    EQ_GL_CALL( glUseProgram( 0 ));

    return true;
}

bool Renderer::_createBuffers()
{
    seq::ObjectManager& om = getObjectManager();

    _fbo = om.newEqFrameBufferObject( &_fbo );
    if( _fbo->init( 1, 1, GL_RGBA, 24, 0 ) != eq::ERROR_NONE )
    {
        om.deleteEqFrameBufferObject( &_fbo );
        return false;
    }

    _fboBlur1 = om.newEqFrameBufferObject( &_fboBlur1 );
    if( _fboBlur1->init( 1, 1, GL_RGBA, 24, 0 ) != eq::ERROR_NONE )
    {
        om.deleteEqFrameBufferObject( &_fboBlur1 );
        return false;
    }

    _fboBlur2 = om.newEqFrameBufferObject( &_fboBlur2 );
    if( _fboBlur2->init( 1, 1, GL_RGBA, 24, 0 ) != eq::ERROR_NONE )
    {
        om.deleteEqFrameBufferObject( &_fboBlur2 );
        return false;
    }

    _posSSBO = om.newBuffer( &_posSSBO );
    _colorSSBO = om.newBuffer( &_colorSSBO );
    _indices = om.newBuffer( &_indices );
    _rectVBO = om.newBuffer( &_rectVBO );

    return true;
}

void Renderer::_updateGPUBuffers( Model& model )
{
    const auto& allParticles = model.getParticles();

    std::vector< particle_sim >  filteredParticles;
    filteredParticles.reserve( allParticles.size( ));

    // Generate colour in same way splotch does (Add brightness here):
    for( size_t i = 0; i < allParticles.size(); ++i )
    {
        const auto& particle = allParticles[i];
        if( particle.r <= std::numeric_limits< float >::epsilon( ))
            continue;

        filteredParticles.push_back( particle );
        auto& newParticle = *filteredParticles.rbegin();
        if (!model.getColourIsVec()[particle.type])
            newParticle.e = model.getColorMaps()[particle.type].getVal_const(particle.e.r) * particle.I;
        else
            newParticle.e *= particle.I;
    }

    _numParticles = filteredParticles.size();

    EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, _posSSBO ));
    EQ_GL_CALL( glBufferData( GL_SHADER_STORAGE_BUFFER,
                              sizeof(seq::Vector4f) * _numParticles, 0, GL_STATIC_DRAW ));
    EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 ));

    EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, _colorSSBO ));
    EQ_GL_CALL( glBufferData( GL_SHADER_STORAGE_BUFFER,
                              sizeof(seq::Vector4f) * _numParticles, 0, GL_STATIC_DRAW ));
    EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 ));

    std::vector< uint32_t > indices( _numParticles * 6 );
    for (size_t i = 0, j = 0; i < _numParticles; ++i)
    {
        size_t index = i << 2;
        indices[j++] = index;
        indices[j++] = index + 1;
        indices[j++] = index + 2;
        indices[j++] = index;
        indices[j++] = index + 2;
        indices[j++] = index + 3;
    }

    EQ_GL_CALL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, _indices ));
    EQ_GL_CALL( glBufferData( GL_ELEMENT_ARRAY_BUFFER,
                              sizeof(uint32_t) * indices.size(), indices.data(), GL_STATIC_DRAW ));
    EQ_GL_CALL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 ));

    EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, _posSSBO ));
    seq::Vector4f* pos = reinterpret_cast< seq::Vector4f* >( glMapBuffer( GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY ));

    EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, _colorSSBO ));
    seq::Vector4f* color = reinterpret_cast< seq::Vector4f* >( glMapBuffer( GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY ));

    for( size_t i = 0; i < _numParticles; ++i )
    {
        pos[i] = seq::Vector4f( filteredParticles[i].x, filteredParticles[i].y, filteredParticles[i].z, 1.0f );
        color[i] = seq::Vector4f( filteredParticles[i].e.r, filteredParticles[i].e.g, filteredParticles[i].e.b, 1.0f );
    }

    EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, _posSSBO ));
    EQ_GL_CALL( glUnmapBuffer( GL_SHADER_STORAGE_BUFFER ));

    EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, _colorSSBO ));
    EQ_GL_CALL( glUnmapBuffer( GL_SHADER_STORAGE_BUFFER ));

    EQ_GL_CALL( glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 ));


    // for blur
    EQ_GL_CALL( glBindBuffer( GL_ARRAY_BUFFER, _rectVBO ));
    EQ_GL_CALL( glBufferData( GL_ARRAY_BUFFER, sizeof(float) * 8, 0,
                              GL_STATIC_DRAW ));
    EQ_GL_CALL( glBindBuffer( GL_ARRAY_BUFFER, 0 ));
}

void Renderer::_drawBlurPass( eq::util::FrameBufferObject* targetFBO,
                              eq::util::Texture* inputTexture,
                              const seq::Vector2f& sampleOffset )
{
    targetFBO->bind();
    inputTexture->bind();

    EQ_GL_CALL( glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ));

    const GLint loc = glGetUniformLocation( _blurShader, "sampleOffset" );
    EQ_GL_CALL( glUniform2fv( loc, 1, &sampleOffset.array[0] ));

    EQ_GL_CALL( glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 ));

    EQ_GL_CALL( glBindTexture( inputTexture->getTarget(), 0 ));
    targetFBO->unbind();
}

void Renderer::_blit( eq::util::FrameBufferObject* fbo )
{
    const eq::PixelViewport& pvp = getPixelViewport();
    fbo->bind( GL_READ_FRAMEBUFFER_EXT );
    bindDrawFrameBuffer();
    EQ_GL_CALL( glBlitFramebuffer( pvp.x, pvp.y, pvp.w, pvp.h,
                                   pvp.x, pvp.y, pvp.w, pvp.h,
                                   GL_COLOR_BUFFER_BIT, GL_NEAREST ));
    fbo->unbind();
}

void Renderer::_updateModel( Model& model )
{
    const ViewData* viewData = static_cast< const ViewData* >( getViewData( ));

    const bool modelDirty = _modelFrameIndex != model.getFrameIndex();
    switch( viewData->getRenderer( ))
    {
    case RENDERER_GPU:
        if( modelDirty )
            _updateGPUBuffers( model );
        break;
    case RENDERER_SPLOTCH_OLD:
    case RENDERER_SPLOTCH_NEW:
    {
        const eq::PixelViewport& pvp = getPixelViewport();
        paramfile& params = model.getParams();
        params.setParam( "xres", pvp.w );
        params.setParam( "yres", pvp.h );
        params.setParam( "fov", int(viewData->getFOV()[0] ));
        break;
    }
#ifdef SEQSPLOTCH_USE_OSPRAY
    case RENDERER_OSPRAY:
        if( modelDirty || !_osprayRenderer )
            _updateOspray( model );
        break;
#endif
    case RENDERER_LAST:
    default:
        std::cerr << "Unknown renderer " << int(viewData->getRenderer( )) << std::endl;
        break;
    }

    _modelFrameIndex = model.getFrameIndex();
}

void Renderer::_cpuRender( Model& model )
{
    const eq::PixelViewport& pvp = getPixelViewport();
    const int width = pvp.w;
    const int height = pvp.h;

    arr2< COLOUR > pic( width, height );
    seq::Vector3f origin, lookAt, up;
    getModelMatrix().getLookAt( origin, lookAt, up );
    auto particles = model.getParticles();
    paramfile& params = model.getParams();

    seq::Vector3f eye = origin;
    if( getRenderContext().eye != eq::EYE_CYCLOP )
    {
        const seq::Vector3f view = lookAt - origin;
        const seq::Vector3f right = vmml::cross( view, up );

        const ViewData* viewData = static_cast< const ViewData* >( getViewData( ));
        const float dist2 = viewData->getEyeSeparation() * view.length() * .5f;

        switch( getRenderContext().eye )
        {
        case eq::EYE_LEFT:
            eye = origin - right / right.length()  * dist2;
            break;
        case eq::EYE_RIGHT:
            eye = origin + right / right.length()  * dist2;
            break;
        default:;
        }
    }

#ifdef CUDA
    cuda_rendering( 0, 1, pic, particles,
                    vec3( eye.x(), eye.y(), eye.z()),
                    vec3( origin.x(), origin.y(), origin.z()),
                    vec3( lookAt.x(), lookAt.y(), lookAt.z()),
                    vec3( up.x(), up.y(), up.z()),
                    model.amap, model.b_brightness, params );
#else
    host_rendering( params, particles, pic,
                    vec3( eye.x(), eye.y(), eye.z()),
                    vec3( origin.x(), origin.y(), origin.z()),
                    vec3( lookAt.x(), lookAt.y(), lookAt.z()),
                    vec3( up.x(), up.y(), up.z()),
                    model.getColorMaps(), model.getBrightness(), particles.size( ));
#endif

    const bool a_eq_e = params.find<bool>("a_eq_e",true);
    if( a_eq_e )
    {
        exptable<float32> xexp(-20.0);
#pragma omp parallel for
        for( int ix = 0; ix < width; ++ix )
        {
            for( int iy = 0; iy < height; ++iy )
            {
                pic[ix][iy].r = -xexp.expm1(pic[ix][iy].r);
                pic[ix][iy].g = -xexp.expm1(pic[ix][iy].g);
                pic[ix][iy].b = -xexp.expm1(pic[ix][iy].b);
            }
        }
    }

    const double gamma = params.find< double >("pic_gamma", 1.0 );
    const double brightness = params.find< double >("pic_brighness", 0.0 );
    const double contrast = params.find< double >("pic_contrast", 1.0 );

    if( gamma != 1.0 || brightness != 0.0 || contrast != 1.0 )
    {
#pragma omp parallel for
        for( tsize i = 0; i < pic.size1(); ++i )
        {
            for( tsize j = 0; j < pic.size2(); ++j )
            {
                pic[i][j].r = contrast * pow((double)pic[i][j].r,gamma) + brightness;
                pic[i][j].g = contrast * pow((double)pic[i][j].g,gamma) + brightness;
                pic[i][j].b = contrast * pow((double)pic[i][j].b,gamma) + brightness;
            }
       }
    }

    _pixels.grow( width * height * 3 );
    int k = 0;
    for( int j = 0; j < height; ++j )
    {
        for( int i = 0; i < width; ++i, k+=3 )
            memcpy( &_pixels[k], &pic[i][j], 3 * sizeof( float ));
    }

    glWindowPos2i( pvp.x, pvp.y );
    glDrawPixels( width, height, GL_RGB, GL_FLOAT, _pixels.getData( ));
}

void Renderer::_osprayRender( Model& model LB_UNUSED )
{
#ifdef SEQSPLOTCH_USE_OSPRAY
    const eq::PixelViewport& pvp = getPixelViewport();
    const ViewData* viewData = static_cast< const ViewData* >( getViewData( ));

    glWindowPos2i( pvp.x, pvp.y );
    _osprayRenderer->render( model, seq::Vector2i( pvp.w, pvp.h ), getModelMatrix(), viewData->getFOV()[1]);
#endif
}

void Renderer::_updateOspray( Model& model LB_UNUSED )
{
#ifdef SEQSPLOTCH_USE_OSPRAY
    if( !_osprayRenderer )
        _osprayRenderer.reset( new OSPRayRenderer );
    _osprayRenderer->update( model );
#endif
}

void Renderer::_gpuRender( const bool blurOn )
{
    const eq::PixelViewport& pvp = getPixelViewport();
    _fbo->resize( pvp.w, pvp.h );
    _fboBlur1->resize( pvp.w, pvp.h );
    _fboBlur2->resize( pvp.w, pvp.h );

    float brightnessMod = 8.0;
    float saturation = 1.0;
    float contrast   = 1.0;
    float particleSize = 0.6;
    float nParticleSize = particleSize * 10;
    float blurStrength = 0.1f;
    float blurColorModifier = 1.2f;

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
        _fbo->bind();

        EQ_GL_CALL( glViewport( pvp.x, pvp.y, pvp.w, pvp.h ));
        EQ_GL_CALL( glScissor( pvp.x, pvp.y, pvp.w, pvp.h ));
        EQ_GL_CALL( glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ));

        const seq::Matrix4f projectionMatrix = getFrustum().computePerspectiveMatrix();
        seq::Matrix4f modelViewMatrix = getViewMatrix() * getModelMatrix();
        modelViewMatrix( 3, 2 ) = -modelViewMatrix( 3, 2 );

        loc = glGetUniformLocation( _particleShader, "projection" );
        EQ_GL_CALL( glUniformMatrix4fv( loc, 1, GL_FALSE, projectionMatrix.data( )));

        loc = glGetUniformLocation( _particleShader, "modelview" );
        EQ_GL_CALL( glUniformMatrix4fv( loc, 1, GL_FALSE, modelViewMatrix.data( )));

        EQ_GL_CALL( glEnable( GL_BLEND ));
        EQ_GL_CALL( glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ));
        EQ_GL_CALL( glHint( GL_LINE_SMOOTH_HINT, GL_FASTEST ));

        {
            EQ_GL_CALL( glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, _posSSBO ));
            EQ_GL_CALL( glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, _colorSSBO ));
            EQ_GL_CALL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, _indices ));
            EQ_GL_CALL( glDrawElements( GL_TRIANGLES, _numParticles * 6, GL_UNSIGNED_INT, 0 ));
            EQ_GL_CALL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 ));
        }

        EQ_GL_CALL( glDisable( GL_BLEND ));
        EQ_GL_CALL( glUseProgram( 0 ));

        _fbo->unbind();
    }

    if( !blurOn )
    {
        _blit( _fbo );
        return;
    }

    EQ_GL_CALL( glUseProgram( _blurShader ));
    loc = glGetUniformLocation( _blurShader, "projection" );
    const eq::Matrix4f& proj = eq::Frustumf( pvp.x, pvp.getXEnd(),
                                             pvp.y, pvp.getYEnd(),
                                             -1.0f, 1.0f ).computeOrthoMatrix();
    EQ_GL_CALL( glUniformMatrix4fv( loc, 1, GL_FALSE, proj.data( )));

    loc = glGetUniformLocation( _blurShader, "colorModifier" );
    EQ_GL_CALL( glUniform1f( loc, blurColorModifier ));

    EQ_GL_CALL( glBindAttribLocation( _blurShader, 0, "position" ));
    EQ_GL_CALL( glEnableVertexAttribArray( 0 ));

    const GLfloat rect[8] = { float( pvp.getXEnd( )), float( pvp.y ),
                              float( pvp.x ), float( pvp.y ),
                              float( pvp.getXEnd( )), float( pvp.getYEnd( )),
                              float( pvp.x ), float( pvp.getYEnd( )) };

    EQ_GL_CALL( glBindBuffer( GL_ARRAY_BUFFER, _rectVBO ));
    EQ_GL_CALL( glBufferSubData( GL_ARRAY_BUFFER, 0, sizeof(float)*8, rect ));
    EQ_GL_CALL( glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0 ));

    _drawBlurPass( _fboBlur1, _fbo->getColorTextures()[0],
                   seq::Vector2f( blurStrength, 0.0f ));
    _drawBlurPass( _fboBlur2, _fboBlur1->getColorTextures()[0],
                   seq::Vector2f( 0.0f, blurStrength ));

    EQ_GL_CALL( glUseProgram( 0 ));
    EQ_GL_CALL( glBindBuffer( GL_ARRAY_BUFFER, 0 ));

    EQ_GL_CALL( glEnable( GL_BLEND ));
    EQ_GL_CALL( glBlendFunc( GL_SRC_ALPHA, GL_ONE ));
    _blit( _fboBlur2 );
    EQ_GL_CALL( glDisable( GL_BLEND ));
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
    _updateModel( model );

    updateNearFar( model.getBoundingSphere( ));
    applyRenderContext(); // set up OpenGL State

    switch( viewData->getRenderer( ))
    {
    case RENDERER_GPU:
        _gpuRender( viewData->useBlur( ));
        return;
    case RENDERER_SPLOTCH_OLD:
    case RENDERER_SPLOTCH_NEW:
    {
        paramfile& params = model.getParams();
        params.setParam( "new_renderer",
                         viewData->getRenderer() == RENDERER_SPLOTCH_NEW );
        _cpuRender( model );
        return;
    }
#ifdef SEQSPLOTCH_USE_OSPRAY
    case RENDERER_OSPRAY:
        _osprayRender( model );
        return;
#endif
    case RENDERER_LAST:
    default:
        std::cerr << "Unknown renderer " << int(viewData->getRenderer( )) << std::endl;
        return;
    }
}

}
