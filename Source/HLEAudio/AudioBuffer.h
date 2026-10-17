/*
Copyright (C) 2007 StrmnNrmn

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#pragma once

#ifndef HLEAUDIO_AUDIOBUFFER_H_
#define HLEAUDIO_AUDIOBUFFER_H_

#include "Base/Types.h"
#include <atomic>

struct Sample {
  s16 L;
  s16 R;
};

// A utility class for buffering up samples, upsampling to the desired
// output frequency and copying them to the desired output buffer.
//
// Single producer / single consumer. Sample writes are published before
// the consumer can read them; read slots are released before reuse.
class CAudioBuffer {
public:
  CAudioBuffer(u32 buffer_size);
  ~CAudioBuffer();

  void AddSamples(const Sample *samples, u32 num_samples, u32 frequency,
                  u32 output_freq);
  u32 Drain(Sample *samples, u32 num_samples);

  u32 GetNumBufferedSamples() const;

private:
  void Push(Sample sample, Sample *&write, const Sample *&read);

  Sample *mBufferBegin;
  Sample *mBufferEnd;

  std::atomic<const Sample *> mReadPtr;
  std::atomic<Sample *> mWritePtr;
  Sample mLastInput{};
  bool mHasLastInput = false;
  u32 mInputFrequency = 0;
  u32 mOutputFrequency = 0;
  u64 mPhase = 0;
};

#endif // HLEAUDIO_AUDIOBUFFER_H_
