
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

#include "osprayRenderer.h"

#include "model.h"

#include <eq/gl.h>

namespace seqSplotch
{

OSPRayRenderer::OSPRayRenderer()
    : _model( nullptr )
    , _fb( nullptr )
    , _renderer( nullptr )
    , _camera( nullptr )
{
}

OSPRayRenderer::~OSPRayRenderer()
{

}

void OSPRayRenderer::update( Model& model )
{
    if( !_renderer )
    {
        _renderer = ospNewRenderer( "eyeLight_vertexColor" );
        std::vector<OSPLight> dirLights;
        OSPLight ospLight = ospNewLight(_renderer, "DirectionalLight");
        ospSetString(ospLight, "name", "sun" );
        ospSet3f( _renderer, "bgColor", 0, 0, 0 );
        ospSet3f(ospLight, "color", 1, 1, 1);
        const vec3f defaultDirLight_direction(.3, -1, -.2);
        ospSet3fv(ospLight, "direction", &defaultDirLight_direction.x);
        ospCommit(ospLight);
        dirLights.push_back(ospLight);
        OSPData dirLightArray = ospNewData(dirLights.size(), OSP_OBJECT, &dirLights[0], 0);
        ospSetData(_renderer, "directionalLights", dirLightArray);

        _camera = ospNewCamera( "perspective" );
        ospSet3f( _camera, "pos", -1, +1, -1 );
        ospSet3f( _camera, "dir", +1, -1, +1 );
        ospCommit( _camera );

        ospSetObject( _renderer,"camera", _camera );
        ospCommit( _camera );
        ospCommit( _renderer );
    }

    if( _model )
        ospRelease( _model );

    struct Atom
    {
        vec3f position;
        float radius;
        int type;
    };

    static std::vector< Atom > atoms;
    atoms.clear();
    atoms.reserve( model.getParticles().size( ));
    std::vector< OSPMaterial > materials;
    std::vector< int > types;
    for( const auto& i : model.getParticles( ))
    {
        if( i.r <= std::numeric_limits< float >::epsilon( ))
            continue;
        atoms.emplace_back( Atom { vec3f( i.x, i.y, i.z ), i.r, i.type } );
        if( std::find( types.begin(), types.end(), i.type ) == types.end( ))
        {
            OSPMaterial mat = ospNewMaterial(_renderer,"OBJMaterial");
            ospSet3fv(mat,"kd", &i.e.r);
            ospCommit(mat);
            materials.push_back( mat );
            types.push_back( i.type );
        }
    }
    OSPData materialData = ospNewData( materials.size(),OSP_OBJECT,materials.data());
    ospCommit(materialData);

    _model = ospNewModel();

    OSPData data = ospNewData(atoms.size()*5,OSP_FLOAT,
                              atoms.data(),OSP_DATA_SHARED_BUFFER);
    ospCommit(data);

    OSPGeometry geom = ospNewGeometry("spheres");
    ospSet1i(geom,"bytes_per_sphere",sizeof(Atom));
    ospSet1i(geom,"offset_center",0);
    ospSet1i(geom,"offset_radius",3*sizeof(float));
    ospSet1i(geom,"offset_materialID",4*sizeof(float));
    ospSetData(geom,"spheres",data);
    ospSetData(geom,"materialList",materialData);
    ospCommit(geom);

    ospAddGeometry(_model,geom);
    ospCommit(_model);

    ospSetObject( _renderer,"model", _model );

    ospCommit(_renderer);
}

void OSPRayRenderer::render( Model& /*model*/, const seq::Vector2i& size, const seq::Matrix4f& matrix, const float fovy )
{
    if( size != _size )
    {
        _size = size;

        if( _fb )
            ospFreeFrameBuffer( _fb );

        const auto &newSize = reinterpret_cast<const osp::vec2i&>( size );
        _fb = ospNewFrameBuffer( newSize, OSP_FB_SRGBA, OSP_FB_COLOR/* | OSP_FB_ACCUM*/);
        //ospFrameBufferClear(fb,OSP_FB_ACCUM);
        //accumID = 0;
        ospSetf( _camera, "aspect", float(size[0]) / float(size[1]) );
    }

    seq::Vector3f origin, lookAt, up;
    matrix.getLookAt( origin, lookAt, up );
    lookAt = vmml::normalize( lookAt - origin );
    ospSetVec3f( _camera, "pos", reinterpret_cast<osp::vec3f&>(origin) );
    ospSetVec3f( _camera, "dir", reinterpret_cast<osp::vec3f&>(lookAt));
    ospSetVec3f( _camera, "up", reinterpret_cast<osp::vec3f&>(up));
    ospSetf( _camera, "fovy", fovy );
    ospCommit( _camera );

    ospRenderFrame( _fb, _renderer, OSP_FB_COLOR );

    uint32_t* ucharFB = (uint32_t *)ospMapFrameBuffer( _fb );

    glDrawPixels( size[0], size[1], GL_RGBA, GL_UNSIGNED_BYTE, ucharFB );

    ospUnmapFrameBuffer( ucharFB, _fb );
}

}
