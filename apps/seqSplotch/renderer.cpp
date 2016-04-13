
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

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef CUDA
#  include <splotch/cuda/cuda_splotch.h>
#else
#  include <splotch/splotch_host.h>
#endif

#include <PP_FBO.frag.h>
#include <PP_FBO.geom.h>
#include <PP_FBO.vert.h>

#include <FBO_Passthrough.frag.h>
#include <FBO_Passthrough.vert.h>

#include <FBO_ToneMap.frag.h>
#include <FBO_ToneMap.vert.h>

#include <particles.frag.h>
#include <particles.vert.h>
#include <blur.frag.h>
#include <blur_pass.vert.h>

//// light parameters
//static GLfloat lightPosition[] = {0.0f, 0.0f, 1.0f, 0.0f};
//static GLfloat lightAmbient[]  = {0.1f, 0.1f, 0.1f, 1.0f};
//static GLfloat lightDiffuse[]  = {0.8f, 0.8f, 0.8f, 1.0f};
//static GLfloat lightSpecular[] = {0.8f, 0.8f, 0.8f, 1.0f};

//// material properties
//static GLfloat materialAmbient[]  = {0.2f, 0.2f, 0.2f, 1.0f};
//static GLfloat materialDiffuse[]  = {0.8f, 0.8f, 0.8f, 1.0f};
//static GLfloat materialSpecular[] = {0.5f, 0.5f, 0.5f, 1.0f};
//static GLint  materialShininess   = 64;

namespace seqSplotch
{

Renderer::Renderer( seq::Application& app )
    : seq::Renderer( app )
    , _posSSBO( 0 )
    , _fboPassThrough( nullptr )
    /*, _state( 0 )*/
{}

bool Renderer::init( co::Object* initData )
{
    //_state = new State( glewGetContext( ));
    return seq::Renderer::init( initData );
}

bool Renderer::initContext( co::Object* initData )
{
    if( !seq::Renderer::initContext( initData ))
        return false;

    if( !_loadShaders( ))
        return false;

//    if( !_genTexture( ))
//        return false;

//    glEnable(GL_PROGRAM_POINT_SIZE_EXT);



    return true;
}

bool Renderer::exitContext()
{
    seq::ObjectManager& om = getObjectManager();
    om.deleteEqTexture( &_texture );
    om.deleteProgram( &_program );
    om.deleteProgram( &_passThroughShader );
    om.deleteProgram( &_tonemapShader );
    om.deleteBuffer( &_vertexBuffer );
    om.deleteEqFrameBufferObject( &_fboPassThrough );
    om.deleteEqFrameBufferObject( &_fboTonemap );

    return seq::Renderer::exitContext();
}

bool Renderer::_loadShaders()
{
#if 1
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
#else
    seq::ObjectManager& om = getObjectManager();
    _program = om.newProgram( &_program );
    if( !seq::linkProgram( om.glewGetContext(), _program, PP_FBO_vert,
                           PP_FBO_frag, PP_FBO_geom ))
    {
        om.deleteProgram( &_program );
        return false;
    }

    EQ_GL_CALL( glUseProgram( _program ));
    _matrixUniform = glGetUniformLocation( _program, "MVP" );

    GLuint texSrc = glGetUniformLocation( _program, "Tex0" );
    EQ_GL_CALL( glUniform1i( texSrc, 0 ));

    _passThroughShader = om.newProgram( &_passThroughShader );
    if( !seq::linkProgram( om.glewGetContext(), _passThroughShader, FBO_Passthrough_vert,
                           FBO_Passthrough_frag ))
    {
        om.deleteProgram( &_passThroughShader );
        return false;
    }

    const seq::Matrix4f mvp = seq::Matrix4f::IDENTITY;
    EQ_GL_CALL( glUseProgram( _passThroughShader ));

    GLuint mvpUnif = glGetUniformLocation( _passThroughShader, "MVP" );
    EQ_GL_CALL( glUniformMatrix4fv( mvpUnif, 1, GL_FALSE, &mvp[0] ));

    texSrc = glGetUniformLocation( _passThroughShader, "Tex0" );
    EQ_GL_CALL( glUniform1i( texSrc, 0 ));

    _tonemapShader = om.newProgram( &_tonemapShader );
    if( !seq::linkProgram( om.glewGetContext(), _tonemapShader, FBO_ToneMap_vert,
                           FBO_ToneMap_frag ))
    {
        om.deleteProgram( &_tonemapShader );
        return false;
    }

    EQ_GL_CALL( glUseProgram( _tonemapShader ));
    mvpUnif = glGetUniformLocation( _tonemapShader, "MVP" );
    EQ_GL_CALL( glUniformMatrix4fv( mvpUnif, 1, GL_FALSE, &mvp[0] ));

    texSrc = glGetUniformLocation( _tonemapShader, "Tex0" );
    EQ_GL_CALL( glUniform1i( texSrc, 0 ));

    EQ_GL_CALL( glUseProgram( 0 ));
    return true;
#endif
}

bool Renderer::_genFBO( Model& /*model*/ )
{
//    int xres = model.params.find<int>("xres",800),
//        yres = model.params.find<int>("yres",xres);
    int xres = getPixelViewport().w;
    int yres = getPixelViewport().h;

    seq::ObjectManager& om = getObjectManager();

    _fboPassThrough = om.newEqFrameBufferObject( &_fboPassThrough );
    if( _fboPassThrough->init( xres, yres, GL_RGBA32F, 24, 0 ) != eq::ERROR_NONE )
    {
        om.deleteEqFrameBufferObject( &_fboPassThrough );
        return false;
    }

    _fboTonemap = om.newEqFrameBufferObject( &_fboTonemap );
    if( _fboTonemap->init( xres, yres, GL_RGBA32F, 24, 0 ) != eq::ERROR_NONE )
    {
        om.deleteEqFrameBufferObject( &_fboTonemap );
        return false;
    }

    return true;
}

bool Renderer::_genVBO()
{
    seq::ObjectManager& om = getObjectManager();
    Application& application = static_cast< Application& >( getApplication( ));
    Model& model = application.getModel();

    const size_t bufferSize = model.getParticles().size() *sizeof(particle_sim);

    _vertexBuffer = om.newBuffer( &_vertexBuffer );
    EQ_GL_CALL( glBindBuffer( GL_ARRAY_BUFFER, _vertexBuffer ));
    EQ_GL_CALL( glBufferData( GL_ARRAY_BUFFER,
                              bufferSize, 0, GL_STATIC_DRAW ));

    //enum,offset,size,start
    EQ_GL_CALL( glBufferSubData( GL_ARRAY_BUFFER, 0, bufferSize,
                                 &model.getParticles()[0] ));
    EQ_GL_CALL( glBindBuffer( GL_ARRAY_BUFFER, 0 ));

    return true;
}

bool Renderer::_genTexture()
{
    int width, height, comp;
    const std::string particleTexture( lunchbox::getRootPath() +
                                      "/share/seqSplotch/data/particle.tga" );
    void* pixels = stbi_load( particleTexture.c_str(), &width, &height, &comp,
                              0 );
    if( !pixels )
        return false;
    seq::ObjectManager& om = getObjectManager();
    _texture = om.newEqTexture( &_texture, GL_TEXTURE_2D );
    _texture->init( GL_RGBA, width, height );
    _texture->setExternalFormat( GL_RGBA, GL_UNSIGNED_BYTE );
    _texture->upload( width, height, pixels );
    EQ_GL_CALL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT ));
    EQ_GL_CALL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT ));
    EQ_GL_CALL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ));
    EQ_GL_CALL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR ));
    stbi_image_free( pixels );
    EQ_GL_CALL( glBindTexture( GL_TEXTURE_2D, 0 ));
    return true;
}

bool Renderer::exit()
{
//    _state->deleteAll();
//    delete _state;
//    _state = 0;
    return seq::Renderer::exit();
}

void Renderer::cpuRender( Model& model )
{
    seq::Vector3f eye;
    seq::Vector3f lookAt;
    seq::Vector3f up;
    getModelMatrix().getLookAt( eye, lookAt, up );

    const int xres = getPixelViewport().w;
    const int yres = getPixelViewport().h;

    model.params.setParam( "xres", xres );
    model.params.setParam( "yres", yres );

    arr2<COLOUR> pic( xres, yres );

    auto particles = model.getParticles();
#ifdef CUDA
    cuda_rendering( 0, 1, pic, particles,
                    vec3( eye.x(), eye.y(), eye.z()),
                    vec3( eye.x(), eye.y(), eye.z()),
                    vec3( lookAt.x(), lookAt.y(), lookAt.z()),
                    vec3( up.x(), up.y(), up.z()),
                    model.amap, model.b_brightness, model.params );
#else
    host_rendering( model.params, particles, pic,
                    vec3( eye.x(), eye.y(), eye.z()),
                    vec3( eye.x(), eye.y(), eye.z()),
                    vec3( lookAt.x(), lookAt.y(), lookAt.z()),
                    vec3( up.x(), up.y(), up.z()),
                    model.amap, model.b_brightness, particles.size( ));
#endif

    bool a_eq_e = model.params.find<bool>("a_eq_e",true);
    if( a_eq_e )
    {
        exptable<float32> xexp(-20.0);
#pragma omp parallel for
      for (int ix=0;ix<xres;ix++)
        for (int iy=0;iy<yres;iy++)
          {
          pic[ix][iy].r=-xexp.expm1(pic[ix][iy].r);
          pic[ix][iy].g=-xexp.expm1(pic[ix][iy].g);
          pic[ix][iy].b=-xexp.expm1(pic[ix][iy].b);
          }
    }

    double gamma=model.params.find<double>("pic_gamma",1.0);
    double helligkeit=model.params.find<double>("pic_brighness",0.0);
    double kontrast=model.params.find<double>("pic_contrast",1.0);

    if (gamma != 1.0 || helligkeit != 0.0 || kontrast != 1.0)
      {
#pragma omp parallel for
        for (tsize i=0; i<pic.size1(); ++i)
          for (tsize j=0; j<pic.size2(); ++j)
        {
          pic[i][j].r = kontrast * pow((double)pic[i][j].r,gamma) + helligkeit;
              pic[i][j].g = kontrast * pow((double)pic[i][j].g,gamma) + helligkeit;
              pic[i][j].b = kontrast * pow((double)pic[i][j].b,gamma) + helligkeit;
        }
       }

    float* pixels = new float[xres*yres*3];
    int k = 0;
    for( int j = 0; j < yres; ++j )
    {
        for( int i = 0; i < xres; ++i )
        {
            pixels[k++] = pic[i][j].r;
            pixels[k++] = pic[i][j].g;
            pixels[k++] = pic[i][j].b;
        }
    }

    glWindowPos2i( 0, 0 );
    glDrawPixels(xres,yres,GL_RGB,GL_FLOAT,pixels);
    delete [] pixels;
}

void Renderer::gpuRender( Model& model )
{
    const auto particles = model.getParticles();
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
        for (size_t i = 0, j = 0; i < particles.size(); ++i) {
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
                                  indices.size(), 0, GL_STATIC_DRAW ));
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

//    float blurStrength = 0.13f;
//    float blurColorModifier = 1.2f;

//    bool blurOn = true;

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

        //_fbo->bind();

        EQ_GL_CALL( glClearColor( 0.9, 0.2, 0.2, 1.0 ));
        EQ_GL_CALL( glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ));

        const seq::Matrix4f p = getFrustum().computePerspectiveMatrix();
        const seq::Matrix4f mv = /*getFrustum().computePerspectiveMatrix() **/
                                  getViewMatrix() * getModelMatrix();

        loc = glGetUniformLocation( _particleShader, "ciProjectionMatrix" );
        EQ_GL_CALL( glUniformMatrix4fv( loc, 1, GL_FALSE, p.data( )));

        loc = glGetUniformLocation( _particleShader, "ciModelView" );
        EQ_GL_CALL( glUniformMatrix4fv( loc, 1, GL_FALSE, mv.data( )));

        //gl::setMatrices(mCam);

        //gl::context()->setDefaultShaderVars();

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
        //_fbo->unbind();
    }

//    if (blurOn)
//    {

//        {
//            gl::ScopedViewport scpVp(0, 0, mFboBlur1->getWidth(), mFboBlur1->getHeight());
//            gl::ScopedFramebuffer fbScp(mFboBlur1);
//            gl::ScopedGlslProg scopedblur(_blurShader);
//            gl::ScopedTextureBind tex0(mFbo->getColorTexture(), (uint8_t)0);
//            gl::setMatricesWindowPersp(mFboBlur1->getWidth(), mFboBlur1->getHeight());

//            gl::clear(Color::black());

//            _blurShader->uniform("tex0", 0);

//            _blurShader->uniform("sampleOffset", ci::vec2(blurStrength / mFboBlur1->getWidth(), 0.0f));
//            _blurShader->uniform("colorModifier", blurColorModifier);

//            gl::drawSolidRect(mFboBlur1->getBounds());
//        }

//        {
//            gl::ScopedViewport scpVp(0, 0, mFboBlur2->getWidth(), mFboBlur2->getHeight());
//            gl::ScopedFramebuffer fbScp(mFboBlur2);
//            gl::ScopedGlslProg scopedblur(_blurShader);
//            gl::ScopedTextureBind tex0(mFboBlur1->getColorTexture(), (uint8_t)0);
//            gl::setMatricesWindowPersp(mFboBlur2->getWidth(), mFboBlur2->getHeight());

//            gl::clear(Color::black());

//            _blurShader->uniform("tex0", 0);

//            _blurShader->uniform("sampleOffset", ci::vec2(0.0f, blurStrength / mFboBlur2->getHeight()));
//            _blurShader->uniform("colorModifier", blurColorModifier);

//            gl::drawSolidRect(mFboBlur2->getBounds());
//        }
//    }

    //gl::setMatrices(mCam);
//    EQ_GL_CALL( glViewport( pvp.x, pvp.y, pvp.w, pvp.h ));
//    //gl::setMatricesWindow(getWindowSize());
//    //gl::draw(mFbo->getColorTexture(), getWindowBounds());
//    _fbo->bind( GL_READ_FRAMEBUFFER_EXT );
//    _fbo->getColorTextures()[0]->writeRGB( "/tmp/bla" );
//    EQ_GL_CALL( glBindFramebufferEXT( GL_DRAW_FRAMEBUFFER_EXT, 0 ));
//    EQ_GL_CALL( glBlitFramebuffer( pvp.x, pvp.y, pvp.w, pvp.h,
//                                   pvp.x, pvp.y, pvp.w, pvp.h,
//                                   GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
//                                   GL_STENCIL_BUFFER_BIT, GL_NEAREST ));
//    _fbo->unbind();

//    if (blurOn)
//    {
//        gl::enableAdditiveBlending();
//        gl::draw(mFboBlur2->getColorTexture(), getWindowBounds());
//        gl::disableAlphaBlending();
//    }
}

void Renderer::oldGpuRender( Model& model )
{
    if( !_fboPassThrough )
    {
        if( !_genFBO( model ))
            return;

        if( !_genVBO( ))
            return;

        EQ_GL_CALL( glUseProgram( _program ));

        GLint loc = glGetUniformLocation( _program, "inBrightness" );
        EQ_GL_CALL( glUniform1fv( loc, 10, (float*)&model.brightness[0] ));

        loc = glGetUniformLocation( _program, "inRadialMod" );
        EQ_GL_CALL( glUniform1fv( loc, 1, (float*)&model.radial_mod ));

        loc = glGetUniformLocation( _program, "inSmoothingLength" );
        EQ_GL_CALL( glUniform1fv( loc, 10, (float*)&model.smoothing_length[0] ));

        EQ_GL_CALL( glUseProgram( 0 ));
    }

//    int xres = model.params.find<int>("xres",800),
//        yres = model.params.find<int>("yres",xres);

    EQ_GL_CALL( glClearColor( 0.2, 0.2, 0.2, 1.0 ));
    EQ_GL_CALL( glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ));

    const seq::Matrix4f mvp = getFrustum().computePerspectiveMatrix() *
                              getViewMatrix() * getModelMatrix();

    const eq::PixelViewport& pvp = getPixelViewport();
    //EQ_GL_CALL( glViewport( 0, 0, xres, yres ));
    //EQ_GL_CALL( glScissor( 0, 0, xres, yres ));
    EQ_GL_CALL( glViewport( pvp.x, pvp.y, pvp.w, pvp.h ));
    EQ_GL_CALL( glScissor( pvp.x, pvp.y, pvp.w, pvp.h ));

    glActiveTexture(GL_TEXTURE0);

    // Draw scene into passthrough FBO
    _fboPassThrough->bind();

    EQ_GL_CALL( glClearColor( 0.0, 0.0, 0.0, 1.0 ));
    EQ_GL_CALL( glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ));

    EQ_GL_CALL( glDisable( GL_ALPHA_TEST ));
    EQ_GL_CALL( glDisable( GL_CULL_FACE ));
    EQ_GL_CALL( glDisable( GL_DEPTH_TEST ));
    EQ_GL_CALL( glEnable( GL_BLEND ));
    EQ_GL_CALL( glBlendFunc( GL_SRC_ALPHA, GL_ONE ));

    glActiveTexture( GL_TEXTURE0 );
    glEnable( GL_TEXTURE_2D );
    _texture->bind();

    EQ_GL_CALL( glUseProgram( _program ));
    EQ_GL_CALL( glBindBuffer( GL_ARRAY_BUFFER, _vertexBuffer ));

    EQ_GL_CALL( glUniformMatrix4fv( _matrixUniform, 1, GL_FALSE, mvp.data( )));

    EQ_GL_CALL( glEnableVertexAttribArray( 0 ));
    EQ_GL_CALL( glEnableVertexAttribArray( 1 ));
    EQ_GL_CALL( glEnableVertexAttribArray( 2 ));
    EQ_GL_CALL( glEnableVertexAttribArray( 3 ));
    EQ_GL_CALL( glVertexAttribPointer( 0, 3, GL_FLOAT, GL_TRUE, sizeof(particle_sim), (void*)sizeof(model.getParticles()[0].e)));
    EQ_GL_CALL( glVertexAttribPointer( 1, 3, GL_FLOAT, GL_TRUE, sizeof(particle_sim), (void*)0));
    EQ_GL_CALL( glVertexAttribPointer( 2, 1, GL_FLOAT, GL_TRUE, sizeof(particle_sim), (void*)(sizeof(model.getParticles()[0].x)*6)));
    EQ_GL_CALL( glVertexAttribPointer( 3, 1, GL_UNSIGNED_SHORT, GL_FALSE, sizeof(particle_sim), (void*)(sizeof(model.getParticles()[0].x)*8)));
    EQ_GL_CALL( glDrawArrays( GL_POINTS, 0, model.getParticles().size( )));

    EQ_GL_CALL( glBindBuffer( GL_ARRAY_BUFFER, 0 ));
    EQ_GL_CALL( glUseProgram( 0 ));

    EQ_GL_CALL( glBindTexture( GL_TEXTURE_2D, 0 ));
    _fboPassThrough->unbind();

    const bool showRender = true;

    if( showRender )
    {
        _fboPassThrough->bind( GL_READ_FRAMEBUFFER_EXT );
        _fboTonemap->bind( GL_DRAW_FRAMEBUFFER_EXT );
        EQ_GL_CALL( glBlitFramebuffer( pvp.x, pvp.y, pvp.w, pvp.h,
                                   pvp.x, pvp.y, pvp.w, pvp.h,
                                   GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                                   GL_STENCIL_BUFFER_BIT, GL_NEAREST ));
        _fboPassThrough->unbind();
        _fboTonemap->unbind();
        return;
    }

    //_fboPassThrough->getColorTextures()[0]->writeRGB( "/tmp/bla" );
    _fboPassThrough->getColorTextures()[0]->bind();

    // Draw passthrough FBO into ToneMapping FBO
    _fboTonemap->bind();


    EQ_GL_CALL( glClearColor( 1, 0.0, 0.1, 1.0 ));
    EQ_GL_CALL( glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ));

    EQ_GL_CALL( glDisable( GL_ALPHA_TEST ));
    EQ_GL_CALL( glDisable( GL_CULL_FACE ));
    EQ_GL_CALL( glDisable( GL_DEPTH_TEST ));
    EQ_GL_CALL( glDisable( GL_BLEND ));

//    glActiveTexture( GL_TEXTURE0 );
//    glEnable( GL_TEXTURE_2D );

    // Bind material
    //fboPTMaterial->Bind(ident);
    EQ_GL_CALL( glUseProgram( _passThroughShader ));

//    GLuint mvpUnif = glGetUniformLocation( _passThroughShader, "MVP" );
//    EQ_GL_CALL( glUniformMatrix4fv( mvpUnif, 1, GL_FALSE, &mvp[0] ));

    glBegin(GL_QUADS);

        glTexCoord2f(0,0);  glVertex3f(-1, -1, 0);
        glTexCoord2f(0,1);  glVertex3f(-1, 1, 0);
        glTexCoord2f(1,1);  glVertex3f(1, 1, 0);
        glTexCoord2f(1,0);  glVertex3f(1, -1, 0);

    glEnd();

    //fboPTMaterial->Unbind();
    EQ_GL_CALL( glUseProgram( 0 ));
    EQ_GL_CALL( glBindTexture( GL_TEXTURE_2D, 0 ));

    _fboTonemap->unbind();

    {
        _fboTonemap->bind( GL_READ_FRAMEBUFFER_EXT );
        EQ_GL_CALL( glBlitFramebuffer( pvp.x, pvp.y, pvp.w, pvp.h,
                                   pvp.x, pvp.y, pvp.w, pvp.h,
                                   GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                                   GL_STENCIL_BUFFER_BIT, GL_NEAREST ));
        _fboTonemap->unbind();
    }


//    applyScreenFrustum();

//    EQ_GL_CALL( glViewport( pvp.x, pvp.y, pvp.w, pvp.h ));
//    EQ_GL_CALL( glScissor( pvp.x, pvp.y, pvp.w, pvp.h ));

//    EQ_GL_CALL( glClearColor( 1.0, 0.1, 0.1, 1.0 ));
//    EQ_GL_CALL( glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ));

//    // Bind material
//    //fboTMMaterial->Bind(ident);
//    EQ_GL_CALL( glUseProgram( _tonemapShader ));

////    mvpUnif = glGetUniformLocation( _tonemapShader, "MVP" );
////    EQ_GL_CALL( glUniformMatrix4fv( mvpUnif, 1, GL_FALSE, &mvp[0] ));

//    glBegin(GL_QUADS);

//        glTexCoord2f(0,0);  glVertex3f(-1, -1, 0);
//        glTexCoord2f(0,1);  glVertex3f(-1, 1, 0);
//        glTexCoord2f(1,1);  glVertex3f(1, 1, 0);
//        glTexCoord2f(1,0);  glVertex3f(1, -1, 0);

//    glEnd();

//    //fboTMMaterial->Unbind();
//    EQ_GL_CALL( glUseProgram( 0 ));
}

void Renderer::draw( co::Object* /*frameDataObj*/ )
{
    //const FrameData* frameData = static_cast< FrameData* >( frameDataObj );

    Application& application = static_cast< Application& >( getApplication( ));
    Model& model = application.getModel();

    applyRenderContext(); // set up OpenGL State

    if( model.cpuRender )
        cpuRender( model );
    else
        gpuRender( model );
//    const FrameData* frameData = static_cast< FrameData* >( frameDataObj );
//    Application& application = static_cast< Application& >( getApplication( ));
//    const eq::uint128_t& id = frameData->getModelID();
//    const Model* model = application.getModel( id );
//    if( !model )
//        return;

//    updateNearFar( model->getBoundingSphere( ));
//    applyRenderContext();

//    glLightfv( GL_LIGHT0, GL_POSITION, lightPosition );
//    glLightfv( GL_LIGHT0, GL_AMBIENT,  lightAmbient  );
//    glLightfv( GL_LIGHT0, GL_DIFFUSE,  lightDiffuse  );
//    glLightfv( GL_LIGHT0, GL_SPECULAR, lightSpecular );

//    glMaterialfv( GL_FRONT, GL_AMBIENT,   materialAmbient );
//    glMaterialfv( GL_FRONT, GL_DIFFUSE,   materialDiffuse );
//    glMaterialfv( GL_FRONT, GL_SPECULAR,  materialSpecular );
//    glMateriali(  GL_FRONT, GL_SHININESS, materialShininess );

//    applyModelMatrix();

//    glColor3f( .75f, .75f, .75f );

//    // Compute cull matrix
//    const eq::Matrix4f& modelM = getModelMatrix();
//    const eq::Matrix4f& view = getViewMatrix();
//    const eq::Frustumf& frustum = getFrustum();
//    const eq::Matrix4f projection = frustum.compute_matrix();
//    const eq::Matrix4f pmv = projection * view * modelM;
//    const seq::RenderContext& context = getRenderContext();

//    _state->setProjectionModelViewMatrix( pmv );
//    _state->setRange( triply::Range( &context.range.start ));
//    _state->setColors( model->hasColors( ));

//    model->cullDraw( *_state );
}

}
