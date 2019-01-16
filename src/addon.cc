//
// This file is part of node-pulseaudio
//
// Copyright © 2013  Kayo Phoenix <kayo@illumium.org>
//             2017-2019 The Board of Trustees of the Leland Stanford Junior University
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include "common.hh"
#include "context.hh"
#include "stream.hh"

namespace pulse {
  void
  init(Local<Object> exports){
    HandleScope scope(exports->GetIsolate());
    
    Context::Init(exports);
    Stream::Init(exports);
  }
}

NODE_MODULE(NODE_GYP_MODULE_NAME, pulse::init);
