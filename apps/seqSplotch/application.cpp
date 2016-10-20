
/* Copyright (c) 2011-2013, Stefan Eilemann <eile@eyescale.ch>
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

#include "application.h"

#include "model.h"
#include "renderer.h"
#include "viewData.h"

#ifdef SEQSPLOTCH_USE_OSPRAY
#  include <ospray/ospray.h>
#endif

namespace seqSplotch
{

Application::Application()
{}

Application::~Application()
{}

bool Application::init( int argc, char** argv, co::Object* initData )
{
    std::string paramfile;
    for( int i = 1; i < argc; ++i )
    {
        if( strcmp( argv[i], "--paramfile" ) == 0 )
        {
            ++i;
            paramfile = std::string( argv[i] );
            break;
        }
    }

    if( paramfile.empty( ))
        paramfile = "/home/nachbaur/dev/viz.stable/splotch/configs/snap092.par";

    _model.reset( new Model( servus::URI( paramfile )));

#ifdef SEQSPLOTCH_USE_OSPRAY
    ospInit( &argc, const_cast< const char** >( argv ));
#endif

#ifdef SEQSPLOTCH_USE_ZEROEQ
    _httpServer = ::zeroeq::http::Server::parse( argc, argv );
#endif

    return seq::Application::init( argc, argv, initData );
}

bool Application::run( co::Object* frameData )
{
    return seq::Application::run( frameData );
}

bool Application::exit()
{
    _model.reset();
    return seq::Application::exit();
}

seq::Renderer* Application::createRenderer()
{
    return new Renderer( *this );
}

co::Object* Application::createObject( const uint32_t type )
{
    switch( type )
    {
      case seq::OBJECTTYPE_FRAMEDATA:
          return nullptr;

      default:
          return seq::Application::createObject( type );
    }
}

seq::ViewData* Application::createViewData( seq::View& view )
{
    ViewData* viewData = new ViewData( view, _model.get( ));

#ifdef SEQSPLOTCH_USE_ZEROEQ
    if( _httpServer )
        _httpServer->handle( *viewData );
#endif

    return viewData;
}

void Application::destroyViewData( seq::ViewData* viewData )
{
#ifdef SEQSPLOTCH_USE_ZEROEQ
    if( _httpServer )
        _httpServer->remove( static_cast< ViewData& >( *viewData ));
#endif

    delete viewData;
}

Model& Application::getModel()
{
    return *_model;
}

bool Application::handleEvents()
{
#ifdef SEQSPLOTCH_USE_ZEROEQ
    bool redraw = false;
    while( _httpServer && _httpServer->receive( 0 ))
        redraw = true;
    return redraw;
#endif
}

}
