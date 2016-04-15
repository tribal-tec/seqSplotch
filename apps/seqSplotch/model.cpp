
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

/** The Sequel polygonal rendering example. */
namespace seqSplotch
{

Model::Model( const servus::URI& uri )
    : _params( std::to_string( uri ), false )
    , _sceneMaker( _params )
{
    get_colourmaps( _params, _colorMaps );
    _boost = _params.find< bool >( "boost", false );
    _radialMod = _params.find< double >( "pv_radial_mod", 1.f );

    const unsigned numTypes = _params.find<int>( "ptypes", 1 );

    for(unsigned i = 0; i < numTypes; i++)
    {
        _brightnesses.push_back(_params.find<float>("brightness"+dataToString(i),1.f) * _params.find<float>("pv_brightness_mod"+dataToString(i),1.f));
        //colour_is_vec.push_back(splotchParams->find<bool>("color_is_vector"+dataToString(i),0));
        _smoothingLength.push_back(_params.find<float>("size_fix"+dataToString(i),0) );
    }

    // Ensure unused elements up to tenth are 1 (static size 10 array in shader)
    if(_brightnesses.size()<10)
        _brightnesses.resize(10,1);

    if(_smoothingLength.size()<10)
        _smoothingLength.resize(10,0);

    loadNextFrame();
}

void Model::loadNextFrame()
{
    _particles.clear();
    _points.clear();
    _sceneMaker.getNextScene( _particles, _points, _cameraPosition, _centerPosition, _lookAt, _sky, _outfile );
    _brightness = _boost ? float(_particles.size())/float(_points.size()) : 1.0;
}

std::vector< particle_sim > Model::getParticles() const
{
    return _boost ? _points : _particles;
}

seq::Matrix4f Model::getModelMatrix() const
{
    const seq::Vector3f eye( _cameraPosition.x, _cameraPosition.y, _cameraPosition.z );
    const seq::Vector3f center( _lookAt.x, _lookAt.y, _lookAt.z );
    const seq::Vector3f up( _sky.x, _sky.y, _sky.z );
    return seq::Matrix4f( eye, center, up );
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

}