/*
 * Six Sines A Sinnin'
 *
 * A mess, with audio rate modulation.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work can also be
 * released under GPL3. You know the drill.
 */

#ifndef BACONPAUL_SIX_SINES_INFRA_RIFFWAVWRITER_H
#define BACONPAUL_SIX_SINES_INFRA_RIFFWAVWRITER_H

#include <filesystem>
#include <string>
#include <fstream>
#include <iostream>

namespace baconpaul::six_sines
{
/*
 * A very simple RIFF Wav Writer which *just* writes F32 stereo wav files
 * with an inst block.
 */
struct RIFFWavWriter
{
    std::filesystem::path outPath{};
    FILE *outf{nullptr};
    size_t elementsWritten{0};
    size_t fileSizeLocation{0};
    size_t dataSizeLocation{0};
    size_t dataLen{0};

    uint16_t nChannels{2};

    std::string errMsg{};

    RIFFWavWriter() {}

    RIFFWavWriter(const std::filesystem::path &p, uint16_t chan) : outPath(p), nChannels(chan) {}
    ~RIFFWavWriter()
    {
        if (!closeFile())
        {
            // Unhandleable error here. Throwing is bad. Reporting is useless.
        }
    }

    void writeRIFFHeader()
    {
        pushc4('R', 'I', 'F', 'F');
        fileSizeLocation = elementsWritten;
        pushi32(0);
        pushc4('W', 'A', 'V', 'E');
    }

    void writeFMTChunk(int32_t samplerate)
    {
        pushc4('f', 'm', 't', ' ');
        pushi32(16);
        pushi16(3);         // IEEE float
        pushi16(nChannels); // channels
        pushi32(samplerate);
        pushi32(samplerate * nChannels * 4); // channels * bytes * samplerate
        pushi16(nChannels * 4);              // align on pair of 4 byte samples
        pushi16(8 * 4);                      // bits per sample
    }

    void writeINSTChunk(char keyroot, char keylow, char keyhigh, char vellow, char velhigh)
    {
        pushc4('i', 'n', 's', 't');
        pushi32(8);
        pushi8(keyroot);
        pushi8(0);
        pushi8(127);
        pushi8(keylow);
        pushi8(keyhigh);
        pushi8(vellow);
        pushi8(velhigh);
        pushi8(0);
    }

    void startDataChunk()
    {
        pushc4('d', 'a', 't', 'a');
        dataSizeLocation = elementsWritten;
        pushi32(0);
    }

    void pushSamples(float d[2])
    {
        if (outf)
        {
            elementsWritten += fwrite(d, 1, nChannels * sizeof(float), outf);
            dataLen += nChannels * sizeof(float);
        }
    }

    void pushInterleavedBlock(float *d, size_t nSamples)
    {
        if (outf)
        {
            elementsWritten += fwrite(d, 1, nSamples * sizeof(float), outf);
            dataLen += nSamples * sizeof(float);
        }
    }

    void pushc4(char a, char b, char c, char d)
    {
        char f[4]{a, b, c, d};
        pushc4(f);
    }
    void pushc4(char f[4])
    {
        if (outf)
            elementsWritten += fwrite(f, sizeof(char), 4, outf);
    }

    void pushi32(int32_t i)
    {
        if (outf)
            elementsWritten += std::fwrite(&i, sizeof(char), sizeof(uint32_t), outf);
    }

    void pushi16(int16_t i)
    {
        if (outf)
            elementsWritten += fwrite(&i, sizeof(char), sizeof(uint16_t), outf);
    }

    void pushi8(char i)
    {
        if (outf)
            elementsWritten += std::fwrite(&i, 1, 1, outf);
    }
    [[nodiscard]] bool openFile()
    {
        elementsWritten = 0;
        dataLen = 0;
        dataSizeLocation = 0;
        fileSizeLocation = 0;

        try
        {
            outf = fopen(outPath.u8string().c_str(), "wb");
            if (!outf)
            {
                errMsg = "Unable to open '" + outPath.u8string() + "' for writing";
                return false;
            }
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            outf = nullptr;
            errMsg = e.what();
            return false;
        }
        return true;
    }

    bool isOpen() { return outf != nullptr; }
    [[nodiscard]] bool closeFile()
    {
        if (outf)
        {
            int res;
            res = std::fseek(outf, fileSizeLocation, SEEK_SET);
            if (res)
            {
                std::cout << "SEEK ZERO ERROR" << std::endl;
                return false;
            }
            int32_t chunklen = elementsWritten - 8; // minus riff and size
            fwrite(&chunklen, sizeof(uint32_t), 1, outf);

            res = std::fseek(outf, dataSizeLocation, SEEK_SET);
            if (res)
            {
                return false;
                std::cout << "SEEK ONE ERROR" << std::endl;
            }
            fwrite(&dataLen, sizeof(uint32_t), 1, outf);
            std::fclose(outf);
            outf = nullptr;
        }
        return true;
    }

    [[nodiscard]] size_t getSampleCount() const { return dataLen / (nChannels * sizeof(float)); }
};
} // namespace baconpaul::six_sines
#endif // SAMPLECREATOR_RIFFWAVWRITER_HPP
