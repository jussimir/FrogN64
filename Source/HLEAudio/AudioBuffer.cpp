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

#include "Base/Types.h"
#include "HLEAudio/AudioBuffer.h"
#include "System/Thread/Thread.h"
#include <algorithm>
#include <cstring>
#include <fstream>

CAudioBuffer::CAudioBuffer(u32 buffer_size)
    : mBufferBegin(new Sample[std::max<u32>(2, buffer_size)]),
      mBufferEnd(mBufferBegin + std::max<u32>(2, buffer_size)),
      mReadPtr(mBufferBegin), mWritePtr(mBufferBegin) {}

CAudioBuffer::~CAudioBuffer() { delete[] mBufferBegin; }

u32 CAudioBuffer::GetNumBufferedSamples() const {
  const auto write = mWritePtr.load(std::memory_order_acquire);
  const auto read = mReadPtr.load(std::memory_order_acquire);
  auto count = write - read;
  if (count < 0) count += mBufferEnd - mBufferBegin;
  return static_cast<u32>(count);
}

void CAudioBuffer::Push(Sample sample, Sample *&write, const Sample *&read) {
  Sample *next = write + 1;
  if (next == mBufferEnd) next = mBufferBegin;
  if (next == read) {
    // Publish the partial block before waiting: the consumer must be able
    // to drain it even when one input block exceeds the whole ring.
    mWritePtr.store(write, std::memory_order_release);
    do {
      ThreadYield();
      read = mReadPtr.load(std::memory_order_acquire);
    } while (next == read);
  }
  *write = sample;
  write = next;
}

void CAudioBuffer::AddSamples(const Sample *samples, u32 num_samples,
                              u32 frequency, u32 output_freq) {
  if (!samples || !num_samples || !frequency || !output_freq) return;
#ifdef DAEDALUS_DEBUG_AUDIO
  std::ofstream input("audio_in.raw", std::ios::binary);
  input.write(reinterpret_cast<const char *>(samples), sizeof(Sample) * num_samples);
#endif
  if (frequency != mInputFrequency || output_freq != mOutputFrequency) {
    mHasLastInput = false;
    mPhase = 0;
    mInputFrequency = frequency;
    mOutputFrequency = output_freq;
  }
  Sample *write = mWritePtr.load(std::memory_order_relaxed);
  const Sample *read = mReadPtr.load(std::memory_order_acquire);
  for (u32 i = 0; i < num_samples; ++i) {
    const Sample current = samples[i];
    if (!mHasLastInput) {
      Push(current, write, read);
      mHasLastInput = true;
      mPhase = frequency;
    } else {
      // Phase is measured in output-frequency units of one input interval.
      // Carry it and the preceding sample across packets, so packet boundaries
      // cannot reset interpolation or drop a sample on every AI buffer.
      while (mPhase <= output_freq) {
        Sample out;
        if (mPhase == output_freq) {
          out = current;
        } else {
          const s32 fraction = static_cast<s32>((mPhase << 12) / output_freq);
          out.L = static_cast<s16>(mLastInput.L + (((current.L - mLastInput.L) * fraction) >> 12));
          out.R = static_cast<s16>(mLastInput.R + (((current.R - mLastInput.R) * fraction) >> 12));
        }
        Push(out, write, read);
        mPhase += frequency;
      }
      mPhase -= output_freq;
    }
    mLastInput = current;
  }
  mWritePtr.store(write, std::memory_order_release);
}

u32 CAudioBuffer::Drain(Sample *samples, u32 num_samples) {
  if (!samples || !num_samples) return 0;
  const Sample *read = mReadPtr.load(std::memory_order_relaxed);
  const Sample *write = mWritePtr.load(std::memory_order_acquire);
  u32 count = 0;
  while (count < num_samples && read != write) {
    samples[count++] = *read++;
    if (read == mBufferEnd) read = mBufferBegin;
  }
  mReadPtr.store(read, std::memory_order_release);
  if (count < num_samples) std::memset(samples + count, 0, (num_samples - count) * sizeof(Sample));
#ifdef DAEDALUS_DEBUG_AUDIO
  std::ofstream output("audio_out.raw", std::ios::binary);
  output.write(reinterpret_cast<const char *>(samples), sizeof(Sample) * count);
#endif
  return count;
}
