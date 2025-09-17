//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_importer.h"
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct WavHeader
{
    char chunk_id[4];       // "RIFF"
    u32 chunk_size;         // Size of entire file - 8 bytes
    char format[4];         // "WAVE"
};

struct WavFmtChunk
{
    char sub_chunk1_id[4];  // "fmt "
    u32 sub_chunk1_size;    // 16 for PCM
    u16 audio_format;       // PCM = 1
    u16 num_channels;       // Mono = 1, Stereo = 2
    u32 sample_rate;        // Sample rate
    u32 byte_rate;          // SampleRate * NumChannels * BitsPerSample/8
    u16 block_align;        // NumChannels * BitsPerSample/8
    u16 bits_per_sample;    // 8 bits = 8, 16 bits = 16, etc.
};

struct WavDataChunk
{
    char sub_chunk2_id[4];  // "data"
    u32 sub_chunk2_size;    // NumSamples * NumChannels * BitsPerSample/8
};
#pragma pack(pop)

bool ValidateWavHeader(const WavHeader* header)
{
    return (
        header->chunk_id[0] == 'R' &&
        header->chunk_id[1] == 'I' &&
        header->chunk_id[2] == 'F' &&
        header->chunk_id[3] == 'F' &&
        header->format[0] == 'W' &&
        header->format[1] == 'A' &&
        header->format[2] == 'V' &&
        header->format[3] == 'E'
    );
}

bool ValidateWavFmtChunk(const WavFmtChunk* fmt)
{
    return (
        fmt->sub_chunk1_id[0] == 'f' &&
        fmt->sub_chunk1_id[1] == 'm' &&
        fmt->sub_chunk1_id[2] == 't' &&
        fmt->sub_chunk1_id[3] == ' ' &&
        fmt->audio_format == 1 // PCM only
    );
}

bool ValidateWavDataChunk(const WavDataChunk* data)
{
    return (
        data->sub_chunk2_id[0] == 'd' &&
        data->sub_chunk2_id[1] == 'a' &&
        data->sub_chunk2_id[2] == 't' &&
        data->sub_chunk2_id[3] == 'a'
    );
}

void ImportSound(const fs::path& source_path, Stream* output_stream, Props* config, Props* meta)
{
    (void)config;
    (void)meta;

    std::ifstream input_file(source_path, std::ios::binary);
    if (!input_file.is_open())
    {
        throw std::runtime_error("Failed to open WAV file");
    }
    
    // Read WAV header
    WavHeader wav_header;
    input_file.read(reinterpret_cast<char*>(&wav_header), sizeof(WavHeader));
    
    if (!ValidateWavHeader(&wav_header))
    {
        throw std::runtime_error("Invalid WAV header");
    }
    
    // Read format chunk
    WavFmtChunk fmt_chunk;
    input_file.read(reinterpret_cast<char*>(&fmt_chunk), sizeof(WavFmtChunk));
    
    if (!ValidateWavFmtChunk(&fmt_chunk))
    {
        throw std::runtime_error("Invalid or unsupported WAV format (only PCM supported)");
    }
    
    // Skip any extra fmt chunk data
    if (fmt_chunk.sub_chunk1_size > 16)
    {
        input_file.seekg(fmt_chunk.sub_chunk1_size - 16, std::ios::cur);
    }
    
    // Find the data chunk by scanning through any other chunks (like bext)
    WavDataChunk data_chunk = {};
    bool found_data_chunk = false;
    
    while (!found_data_chunk && input_file.good())
    {
        // Read potential chunk header
        char chunk_id[4];
        u32 chunk_size;
        
        input_file.read(chunk_id, 4);
        if (input_file.gcount() != 4) break;
        
        input_file.read(reinterpret_cast<char*>(&chunk_size), sizeof(u32));
        if (input_file.gcount() != sizeof(u32)) break;
        
        // Check if this is the data chunk
        if (chunk_id[0] == 'd' && chunk_id[1] == 'a' && chunk_id[2] == 't' && chunk_id[3] == 'a')
        {
            // Found data chunk, copy the header info
            memcpy(data_chunk.sub_chunk2_id, chunk_id, 4);
            data_chunk.sub_chunk2_size = chunk_size;
            found_data_chunk = true;
        }
        else
        {
            // Skip this chunk and continue searching
            input_file.seekg(chunk_size, std::ios::cur);
        }
    }
    
    if (!found_data_chunk)
    {
        throw std::runtime_error("Could not find WAV data chunk");
    }
    
    // Validate audio parameters
    if (fmt_chunk.num_channels < 1 || fmt_chunk.num_channels > 2)
    {
        throw std::runtime_error("Unsupported channel count (only mono and stereo supported)");
    }
    
    if (fmt_chunk.bits_per_sample != 8 && fmt_chunk.bits_per_sample != 16)
    {
        throw std::runtime_error("Unsupported bit depth (only 8-bit and 16-bit supported)");
    }
    
    // Write NoZ sound asset header
    AssetHeader asset_header = {};
    asset_header.signature = ASSET_SIGNATURE_SOUND;
    asset_header.version = 1;
    asset_header.flags = 0;
    
    WriteAssetHeader(output_stream, &asset_header);
    
    // Write sound header
    WriteU32(output_stream, fmt_chunk.sample_rate);
    WriteU32(output_stream, fmt_chunk.num_channels);
    WriteU32(output_stream, fmt_chunk.bits_per_sample);
    WriteU32(output_stream, data_chunk.sub_chunk2_size);
    
    // Copy audio data
    std::vector<char> audio_data(data_chunk.sub_chunk2_size);
    input_file.read(audio_data.data(), data_chunk.sub_chunk2_size);
    
    if (input_file.gcount() != data_chunk.sub_chunk2_size)
    {
        throw std::runtime_error("Failed to read complete audio data");
    }
    
    WriteBytes(output_stream, audio_data.data(), data_chunk.sub_chunk2_size);
}

static AssetImporterTraits g_sound_importer_traits = {
    .signature = ASSET_SIGNATURE_SOUND,
    .ext = ".wav",
    .import_func = ImportSound
};

AssetImporterTraits* GetSoundImporterTraits()
{
    return &g_sound_importer_traits;
}