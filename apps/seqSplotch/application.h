
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

#ifndef SEQ_SPLOTCH_APPLICATION_H
#define SEQ_SPLOTCH_APPLICATION_H

#include <seq/sequel.h>

#ifdef SEQSPLOTCH_USE_ZEROEQ
#  include <zeroeq/http/server.h>
#endif

#include "types.h"

namespace seqSplotch
{

class Application : public seq::Application
{
public:
    Application();
    ~Application();

    bool init( int argc, char** argv, co::Object* initData ) final;
    bool run( co::Object* frameData ) final;
    bool exit() final;

    seq::Renderer* createRenderer()  final;
    co::Object* createObject( const uint32_t type ) final;

    Model& getModel();

private:
    seq::ViewData* createViewData( seq::View& view ) final;
    void destroyViewData( seq::ViewData* viewData ) final;

    bool handleEvents() final;

    std::unique_ptr< Model > _model;

#ifdef SEQSPLOTCH_USE_ZEROEQ
    std::unique_ptr< ::zeroeq::http::Server > _httpServer;
#endif
};

}

#endif
