
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

#include "model.h"

namespace seqSplotch
{

Model::Model( const servus::URI& uri )
    : _params( std::to_string( uri ), false )
    , _sceneMaker( _params )
    , _currentFrame( std::numeric_limits< size_t >::max( ))
    , _haveAll( false )
{
    get_colourmaps( _params, _colorMaps );
    _boost = _params.find< bool >( "boost", false );

    const unsigned numTypes = _params.find<int>( "ptypes", 1 );
    for( unsigned i = 0; i < numTypes; ++i )
        _colourIsVec.push_back( _params.find<bool>("color_is_vector" + dataToString(i), 0 ));

    loadNextFrame();
}

void Model::loadNextFrame()
{
    _currentFrame = _currentFrame == std::numeric_limits< size_t >::max()
            ? 0 : _currentFrame+1;
    bool isEOF = _currentFrame >= _particles.size();

    if( !_haveAll )
    {
        std::string outfile;
        vec3 centerPos;
        Particles particles, points;
        isEOF = !_sceneMaker.getNextScene( particles, points, _cameraPosition,
                                           centerPos, _lookAt, _up, outfile );
        if( !isEOF )
        {
            _particles.emplace_back( particles );
            _points.emplace_back( points );
        }
        else
            _haveAll = true;
    }

    if( isEOF )
        _currentFrame = 0;

    _brightness = _boost ? float(_particles.size())/float(_points.size()) : 1.0;

    _computeBoundingSphere();
}

const Model::Particles& Model::getParticles() const
{
    return _boost ? _points[_currentFrame] : _particles[_currentFrame];
}

seq::Matrix4f Model::getModelMatrix() const
{
    const seq::Vector3f eye( _cameraPosition.x, _cameraPosition.y, _cameraPosition.z );
    const seq::Vector3f center( _lookAt.x, _lookAt.y, _lookAt.z );
    const seq::Vector3f up( _up.x, _up.y, _up.z );
    return seq::Matrix4f( eye, center, up );
}

const seq::Vector4f& Model::getBoundingSphere() const
{
    return _boundingSphere;
}

paramfile& Model::getParams()
{
    return _params;
}

std::vector< COLOURMAP >& Model::getColorMaps()
{
    return _colorMaps;
}

float Model::getBrightness() const
{
    return _brightness;
}

const std::vector<bool>& Model::getColourIsVec() const
{
    return _colourIsVec;
}

size_t Model::getFrameIndex() const
{
    return _currentFrame;
}

void Model::_computeBoundingSphere()
{
    if( getParticles().empty( ))
    {
        _boundingSphere = seq::Vector4f( 0, 0, 0, 1 );
        return;
    }

    BoundingBox bbox;
    bbox.Compute( getParticles( ));

    const seq::Vector3f minExtend( bbox.minX, bbox.minY, bbox.minZ );
    const seq::Vector3f maxExtend( bbox.maxX, bbox.maxY, bbox.maxZ );

    _boundingSphere.set_sub_vector< 3, 0 >( (minExtend + maxExtend) / 2.f );
    _boundingSphere.w() = minExtend.distance( maxExtend );
}

}
