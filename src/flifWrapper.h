/*
Copyright 2016 Freddy Herzog

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once

#include <flif_dec.h>

/*!
* Basic RAII wrapper for the decoder
*/
class flifDecoder
{
public:
	flifDecoder()
		: _decoder(flif_create_decoder())
	{
	}

	~flifDecoder()
	{
		if(_decoder != 0)
			flif_destroy_decoder(_decoder);
		_decoder = 0;
	}

	operator FLIF_DECODER*() const
	{
		return _decoder;
	}

private:
	FLIF_DECODER* _decoder;
};

struct flifRGBA
{
	uint8_t r,g,b,a;
};