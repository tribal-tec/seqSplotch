
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

#ifndef SEQ_SPLOTCH_VIEWDATA_H
#define SEQ_SPLOTCH_VIEWDATA_H

#include <seq/sequel.h>

#include "types.h"

namespace seqSplotch
{

enum RendererType
{
    RENDERER_GPU,
    RENDERER_SPLOTCH_OLD,
    RENDERER_SPLOTCH_NEW,
#ifdef SEQSPLOTCH_USE_OSPRAY
    RENDERER_OSPRAY,
#endif
    RENDERER_LAST
};

class ViewData : public seq::ViewData
{
public:
    explicit ViewData( seq::View& view );
    ViewData( seq::View& view, Model* model );
    ~ViewData();

    bool handleEvent( const eq::ConfigEvent* event_ ) final;
    RendererType getRenderer() const;
    bool useBlur() const;
    float getFOV() const;
    float getEyeSeparation() const;

private:
    enum DirtyBits
    {
        DIRTY_RENDERER = seq::ViewData::DIRTY_CUSTOM << 0,
        DIRTY_BLUR = seq::ViewData::DIRTY_CUSTOM << 1
    };

    void serialize( co::DataOStream& os, const uint64_t dirtyBits ) final;
    void deserialize( co::DataIStream& is, const uint64_t dirtyBits ) final;

    const seq::Matrix4f _initialModelMatrix;
    Model* _model;
    RendererType _renderer;
    bool _useBlur;
    float _eyeSeparation;
    seq::View& _view;
};

}

namespace lunchbox
{
template<> inline void byteswap( seqSplotch::RendererType& value )
{ byteswap( reinterpret_cast< uint32_t& >( value )); }
}

#endif
