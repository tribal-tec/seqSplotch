
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
    : Super( view )
    , _model( nullptr )
    , _view( view )
{
}

ViewData::ViewData( seq::View& view, Model* model )
    : Super( view )
    , _initialModelMatrix( model->getModelMatrix( ))
    , _model( model )
    , _view( view )
{
    view.setModelUnit( EQ_MM * 10.f );
    setModelMatrix( _initialModelMatrix );

    const auto& observer = _view.getObserver();
    if( !observer )
        return;
    const auto& leftEye = observer->getEyePosition( eq::EYE_LEFT );
    const auto& rightEye = observer->getEyePosition( eq::EYE_RIGHT );
    setEyeSeparation( rightEye.x() - leftEye.x( ));
}

ViewData::~ViewData()
{}

seq::Vector2f ViewData::getFOV() const
{
    eq::Projection proj;
    proj = _view.getWall();
    return proj.fov;
}

Camera ViewData::getCamera() const
{
    if( _view.getMode() == eq::View::MODE_STEREO )
        return CAM_STEREO;
    return getOrtho() ? CAM_ORTHO : CAM_PERSPECTIVE;
}

bool ViewData::handleEvent( const eq::EventType type,
                            const seq::KeyEvent& keyEvent )
{
    switch( type )
    {
    case eq::EVENT_KEY_PRESS:
        switch( keyEvent.key )
        {
        case ' ':
            setModelMatrix( _initialModelMatrix );
            return true;
        case 'n':
            _model->loadNextFrame();
            return true;
        case 'r':
            setRenderer(serializable::RendererType((int(getRenderer())+1) % (int(serializable::RendererType::OSPRAY)+1)));
            return true;
        case 'b':
            setBlur( !getBlur( ));
            return true;
        case '+':
            setBlurStrength( getBlurStrength() + 0.05f );
            return true;
        case '-':
            setBlurStrength( getBlurStrength() - 0.05f );
            return true;
        case 'l':
        {
            auto& canvas = _view.getConfig()->getCanvases().front();
            uint32_t index = canvas->getActiveLayoutIndex() + 1;
            const auto& layouts = canvas->getLayouts();
            index %= layouts.size();
            canvas->useLayout( index );

            const eq::Layout* layout = layouts[index];
            std::ostringstream stream;
            stream << "Layout ";
            if( layout )
            {
                const std::string& name = layout->getName();
                if( name.empty( ))
                    stream << index;
                else
                    stream << name;
            }
            else
                stream << "NONE";

            stream << " active";
            LBINFO << stream.str() << std::endl;
            return true;
        }
        }
    default:
        return seq::ViewData::handleEvent( type, keyEvent );
    }
}

void ViewData::notifyChanged()
{
    ::seq::ViewData::setOrtho( ViewData::getOrtho( ));
    _view.changeMode( getStereo() ? eq::View::MODE_STEREO
                                  : eq::View::MODE_MONO );

    const auto& observer = _view.getObserver();
    if( !observer )
        return;
    observer->setEyePosition( eq::EYE_LEFT, seq::Vector3f( -getEyeSeparation()/2.f, 0.f , 0 ));
    observer->setEyePosition( eq::EYE_RIGHT, seq::Vector3f( getEyeSeparation()/2.f, 0.f , 0 ));
}

}
