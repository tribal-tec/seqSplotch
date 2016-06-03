
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

#include "viewData.h"

#include "model.h"

#include <eq/view.h>

namespace seqSplotch
{

ViewData::ViewData( seq::View& view )
    : seq::ViewData( view )
    , _model( nullptr )
    , _renderer( RENDERER_GPU )
    , _useBlur( false )
    , _view( view )
{
}

ViewData::ViewData( seq::View& view, Model* model )
    : seq::ViewData( view )
    , _initialModelMatrix( model->getModelMatrix( ))
    , _model( model )
    , _renderer( RENDERER_GPU )
    , _useBlur( false )
    , _eyeSeparation( 0.f )
    , _view( view )
{
    view.setModelUnit( EQ_MM * 10.f );
    setModelMatrix( _initialModelMatrix );

    const auto& observer = _view.getObserver();
    if( !observer )
        return;
    const auto& leftEye = observer->getEyePosition( eq::EYE_LEFT );
    const auto& rightEye = observer->getEyePosition( eq::EYE_RIGHT );
    _eyeSeparation = rightEye.x() - leftEye.x();
}

ViewData::~ViewData()
{}

seq::Vector2f ViewData::getFOV() const
{
    eq::Projection proj;
    proj = _view.getWall();
    return proj.fov;
}

float ViewData::getEyeSeparation() const
{
    return _eyeSeparation;
}

RendererType ViewData::getRenderer() const
{
    return _renderer;
}

bool ViewData::useBlur() const
{
    return _useBlur;
}

bool ViewData::handleEvent( const eq::ConfigEvent* event_ )
{
    const eq::Event& event = event_->data;
    switch( event.type )
    {
    case eq::Event::KEY_PRESS:
        switch( event.keyPress.key )
        {
        case ' ':
            setModelMatrix( _initialModelMatrix );
            return true;
        case 'n':
            _model->loadNextFrame();
            return true;
        case 'r':
            _renderer = RendererType((int(_renderer)+1) % RENDERER_LAST);
            std::cout << "Using renderer " << int(_renderer) << std::endl;
            setDirty( DIRTY_RENDERER );
            return true;
        case 'b':
            _useBlur = !_useBlur;
            setDirty( DIRTY_BLUR );
            return true;
        }
    }
    return seq::ViewData::handleEvent( event_ );
}

void ViewData::serialize( co::DataOStream& os, const uint64_t dirtyBits )
{
    seq::ViewData::serialize( os, dirtyBits );
    if( dirtyBits == DIRTY_ALL )
        os << _eyeSeparation;
    if( dirtyBits & DIRTY_RENDERER )
        os << _renderer;
    if( dirtyBits & DIRTY_BLUR )
        os << _useBlur;
}

void ViewData::deserialize( co::DataIStream& is, const uint64_t dirtyBits )
{
    seq::ViewData::deserialize( is, dirtyBits );
    if( dirtyBits == DIRTY_ALL )
        is >> _eyeSeparation;
    if( dirtyBits & DIRTY_RENDERER )
        is >> _renderer;
    if( dirtyBits & DIRTY_BLUR )
        is >> _useBlur;
}

}
